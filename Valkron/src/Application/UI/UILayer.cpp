#include "Application/UI/UILayer.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include "Core/Log.hpp"
#include "Engine/AssetLoader.hpp"
#include "Event/Event.hpp"
#include "Renderer/Renderer.hpp"
#include "Window/Window.hpp"

#include "imgui.h"

#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <optional>
#include <unordered_set>

namespace Valkron {

    const char* sceneStateToString(SceneState state) {
        switch (state) {
            case SceneState::Edit:
                return "Edit";
            case SceneState::Play:
                return "Play";
            case SceneState::Pause:
                return "Pause";
            default:
                return "Unknown";
        }
    }

    const char* cameraTypeToString(CameraType type) {
        switch (type) {
            case CameraType::Perspective:
                return "Perspective";
            case CameraType::Orthographic:
                return "Orthographic";
            default:
                return "Unknown";
        }
    }

    std::string openFileDialogNative(const char* dialogTitle, const char* filterSpec) {
#if defined(_WIN32)
        char selectedPath[MAX_PATH] = {};
        OPENFILENAMEA dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = selectedPath;
        dialog.nMaxFile = MAX_PATH;
        dialog.lpstrFilter = filterSpec;
        dialog.nFilterIndex = 1;
        dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrTitle = dialogTitle;

        if (GetOpenFileNameA(&dialog) != 0) {
            return std::string(selectedPath);
        }
#endif
        (void)dialogTitle;
        (void)filterSpec;
        return {};
    }

    std::string makeUniqueAssetName(std::string baseName, const std::vector<std::string>& existingNames) {
        if (baseName.empty()) {
            baseName = "Asset";
        }

        std::unordered_set<std::string> existingSet(existingNames.begin(), existingNames.end());
        if (existingSet.find(baseName) == existingSet.end()) {
            return baseName;
        }

        int suffix = 1;
        for (;;) {
            const std::string candidate = baseName + "_" + std::to_string(suffix);
            if (existingSet.find(candidate) == existingSet.end()) {
                return candidate;
            }

            ++suffix;
        }
    }

    glm::vec3 computeOrbitOffset(float distance, float yawRadians, float pitchRadians) {
        const float cosPitch = glm::cos(pitchRadians);
        return glm::vec3(
            distance * cosPitch * glm::sin(yawRadians),
            distance * glm::sin(pitchRadians),
            distance * cosPitch * glm::cos(yawRadians)
        );
    }

    std::string deriveAssetBaseName(const std::string& absoluteOrRelativePath, const std::string& fallbackName) {
        const std::filesystem::path filePath(absoluteOrRelativePath);
        const std::string stem = filePath.stem().string();
        return stem.empty() ? fallbackName : stem;
    }

    std::vector<std::string> collectAllRuntimeAssetNames() {
        std::vector<std::string> names;

        auto appendNames = [&names](const std::vector<std::string>& sourceNames) {
            names.insert(names.end(), sourceNames.begin(), sourceNames.end());
        };

        appendNames(AssetLoader::getTexture2DNames());
        appendNames(AssetLoader::getTexture3DNames());
        appendNames(AssetLoader::getShaderNames());
        appendNames(AssetLoader::getComputeShaderNames());
        appendNames(AssetLoader::getModelNames());
        return names;
    }

    const char kImageFileFilter[] = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm\0All Files\0*.*\0\0";
    const char kVertexShaderFilter[] = "Vertex Shaders\0*.vert;*.vs;*.glsl\0All Files\0*.*\0\0";
    const char kFragmentShaderFilter[] = "Fragment Shaders\0*.frag;*.fs;*.glsl\0All Files\0*.*\0\0";
    const char kComputeShaderFilter[] = "Compute Shaders\0*.comp;*.glsl\0All Files\0*.*\0\0";
    const char kModelFileFilter[] = "Model Files\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds\0All Files\0*.*\0\0";

    void UILayer::bindEngineSettings(Window* window, EngineConfig* engineConfig) {
        m_window = window;
        m_engineConfig = engineConfig;
        syncEngineSettingsEditorState();
    }

    void UILayer::onAttach() {
        m_activeScene = Scene("Sandbox Scene");

        m_activeScene.addEntity("Camera");
        m_activeScene.addEntity("Directional Light");
        m_activeScene.addEntity("Cube");

        m_activeScene.addAsset("checker.ppm", "assets/textures/checker.ppm");
        m_activeScene.addAsset("textured.vert", "assets/shaders/textured.vert");
        m_activeScene.addAsset("textured.frag", "assets/shaders/textured.frag");

        m_activeScene.addScript("SpinController", "scripts/SpinController.lua", true);
        m_activeScene.addScript("CameraOrbit", "scripts/CameraOrbit.lua", false);

        m_activeScene.addCamera("Editor Camera", CameraType::Perspective, true);
        m_activeScene.addCamera("Ortho Debug Camera", CameraType::Orthographic, false);

        m_activeScene.setSetting("Renderer.VSync", "true");
        m_activeScene.setSetting("Renderer.MSAA", "4");
        m_activeScene.setSetting("Physics.Gravity", "0,-9.81,0");

        m_activeScene.setGameStateValue("Mode", "Edit");
        m_activeScene.setGameStateValue("SelectedEntity", "None");

        AssetLoader::initialize();

        const bool hasBlinnShaderAsset = std::any_of(
            m_activeScene.getAssets().begin(),
            m_activeScene.getAssets().end(),
            [](const SceneAsset& asset) { return asset.name == "blinn_phong.vert"; }
        );

        if (!hasBlinnShaderAsset) {
            m_activeScene.addAsset("blinn_phong.vert", "assets/shaders/blinn_phong.vert");
            m_activeScene.addAsset("blinn_phong.frag", "assets/shaders/blinn_phong.frag");
            m_activeScene.addAsset("default_compute.comp", "assets/shaders/default_compute.comp");
            m_activeScene.addAsset("test_cube.obj", "assets/models/test_cube.obj");
        }

        m_terminalLines.clear();
        m_selectedEntityIndex = -1;
        m_pendingViewportWidth = 0;
        m_pendingViewportHeight = 0;
        m_viewportResizeDebounceTimer = 0.0f;
        m_sceneViewImageHovered = false;
        m_sceneCameraControllerInitialized = false;
        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);

        syncEngineSettingsEditorState();
        appendTerminalLine("UI Manager attached (Dear ImGui). Editor layout ready.");
    }

    void UILayer::onDetach() {
        appendTerminalLine("UI Manager detached.");
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        drawTopNavbar();

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        if (displaySize.x <= 0.0f || displaySize.y <= 0.0f) {
            return;
        }

        const float menuBarHeight = ImGui::GetFrameHeight();
        const float topOffset = menuBarHeight;

        float terminalHeight = std::clamp(displaySize.y * 0.24f, 140.0f, 320.0f);
        float centerHeight = displaySize.y - topOffset - terminalHeight;
        if (centerHeight < 120.0f) {
            centerHeight = 120.0f;
            terminalHeight = std::max(80.0f, displaySize.y - topOffset - centerHeight);
        }

        float leftWidth = std::clamp(displaySize.x * 0.20f, 220.0f, 380.0f);
        float rightWidth = std::clamp(displaySize.x * 0.24f, 260.0f, 460.0f);
        float centerWidth = displaySize.x - leftWidth - rightWidth;

        if (centerWidth < 260.0f) {
            const float deficit = 260.0f - centerWidth;
            leftWidth = std::max(160.0f, leftWidth - deficit * 0.5f);
            rightWidth = std::max(180.0f, rightWidth - deficit * 0.5f);
            centerWidth = std::max(100.0f, displaySize.x - leftWidth - rightWidth);
        }

        drawSceneHierarchyPanel(topOffset, centerHeight, leftWidth);
        drawSceneViewPanel(deltaTime, topOffset, centerHeight, leftWidth, centerWidth);
        drawAssetsPanel(topOffset, centerHeight, leftWidth + centerWidth, rightWidth);
        drawTerminalPanel(topOffset + centerHeight, terminalHeight, displaySize.x);
    }

    void UILayer::onEvent(Event& event) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse && (
            event.type == EventType::MouseButton ||
            event.type == EventType::MouseMove ||
            event.type == EventType::MouseScroll
        )) {
            event.handled = true;
            return;
        }

        if (io.WantCaptureKeyboard && event.type == EventType::Key) {
            event.handled = true;
        }
    }

    void UILayer::initializeSceneCameraController() {
        if (m_sceneCameraControllerInitialized) {
            return;
        }

        const glm::vec3 cameraPosition = Renderer::getCameraPosition();
        const glm::vec3 cameraTarget = Renderer::getCameraTarget();
        m_sceneCameraUp = Renderer::getCameraUp();
        m_sceneCameraPivot = cameraTarget;

        glm::vec3 orbitOffset = cameraPosition - cameraTarget;
        float orbitDistance = glm::length(orbitOffset);
        if (orbitDistance < 0.01f) {
            orbitOffset = glm::vec3(0.0f, 0.0f, 2.0f);
            orbitDistance = 2.0f;
        }

        m_sceneCameraDistance = std::max(0.15f, orbitDistance);
        m_sceneCameraPitchRadians = std::clamp(std::asin(orbitOffset.y / m_sceneCameraDistance), -1.52f, 1.52f);
        m_sceneCameraYawRadians = std::atan2(orbitOffset.x, orbitOffset.z);
        m_sceneCameraControllerInitialized = true;
    }

    void UILayer::syncRendererCameraFromController() {
        const glm::vec3 offset = computeOrbitOffset(m_sceneCameraDistance, m_sceneCameraYawRadians, m_sceneCameraPitchRadians);
        const glm::vec3 cameraPosition = m_sceneCameraPivot + offset;
        Renderer::setCameraLookAt(cameraPosition, m_sceneCameraPivot, m_sceneCameraUp);
    }

    void UILayer::updateSceneCameraController(bool sceneImageHovered) {
        initializeSceneCameraController();

        ImGuiIO& io = ImGui::GetIO();
        bool changedCamera = false;

        if (sceneImageHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            m_sceneCameraYawRadians -= io.MouseDelta.x * m_sceneCameraRotateSpeed;
            m_sceneCameraPitchRadians -= io.MouseDelta.y * m_sceneCameraRotateSpeed;
            m_sceneCameraPitchRadians = std::clamp(m_sceneCameraPitchRadians, -1.52f, 1.52f);
            changedCamera = true;
        }

        if (sceneImageHovered && io.KeyCtrl && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            const glm::vec3 orbitUnitOffset = computeOrbitOffset(1.0f, m_sceneCameraYawRadians, m_sceneCameraPitchRadians);
            const glm::vec3 forward = glm::normalize(-orbitUnitOffset);
            const glm::vec3 right = glm::normalize(glm::cross(forward, m_sceneCameraUp));
            const glm::vec3 viewUp = glm::normalize(glm::cross(right, forward));

            glm::vec3 panOffset = ((right * -io.MouseDelta.x) + (viewUp * io.MouseDelta.y)) * (m_sceneCameraDistance * m_sceneCameraPanSpeed);
            if (m_sceneCameraInvertPan) {
                panOffset *= -1.0f;
            }

            m_sceneCameraPivot += panOffset;
            changedCamera = true;
        }

        if (sceneImageHovered && std::abs(io.MouseWheel) > 0.0001f) {
            m_sceneCameraDistance *= std::exp(-io.MouseWheel * m_sceneCameraZoomSpeed);
            m_sceneCameraDistance = std::clamp(m_sceneCameraDistance, 0.15f, 500.0f);
            changedCamera = true;
        }

        if (sceneImageHovered && m_sceneViewOptionsPopupEnabled && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !io.KeyCtrl) {
            ImGui::OpenPopup("SceneView.Options");
        }

        if (ImGui::BeginPopup("SceneView.Options")) {
            ImGui::Text("Scene View Options");
            ImGui::Separator();
            ImGui::TextDisabled("Controls");
            ImGui::BulletText("Middle mouse drag: orbit around pivot");
            ImGui::BulletText("Mouse wheel: zoom in/out");
            ImGui::BulletText("Ctrl + right mouse drag: pan scene");
            ImGui::BulletText("Right click: open this options menu");

            ImGui::Spacing();
            if (ImGui::SliderFloat("Rotate Speed", &m_sceneCameraRotateSpeed, 0.002f, 0.03f, "%.3f")) {
                changedCamera = true;
            }
            if (ImGui::SliderFloat("Pan Speed", &m_sceneCameraPanSpeed, 0.0005f, 0.02f, "%.4f")) {
                changedCamera = true;
            }
            if (ImGui::SliderFloat("Zoom Speed", &m_sceneCameraZoomSpeed, 0.02f, 0.40f, "%.2f")) {
                changedCamera = true;
            }
            ImGui::Checkbox("Invert Pan", &m_sceneCameraInvertPan);

            if (ImGui::Button("Reset Camera")) {
                m_sceneCameraPivot = glm::vec3(0.0f, 0.0f, 0.0f);
                m_sceneCameraDistance = 2.0f;
                m_sceneCameraYawRadians = 0.0f;
                m_sceneCameraPitchRadians = 0.0f;
                m_sceneCameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
                changedCamera = true;
            }

            ImGui::EndPopup();
        }

        if (changedCamera) {
            syncRendererCameraFromController();
        }
    }

    bool UILayer::loadRuntimeTexture2D() {
        const std::string texturePath = openFileDialogNative("Select 2D Texture", kImageFileFilter);
        if (texturePath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(texturePath, "Texture2D"), existingNames);

        if (!AssetLoader::loadTexture2D(textureName, texturePath)) {
            appendTerminalLine("Failed to import 2D texture: " + texturePath);
            return false;
        }

        m_activeScene.addAsset(textureName, texturePath);
        appendTerminalLine("Imported 2D texture: " + textureName + " from " + texturePath);
        return true;
    }

    bool UILayer::loadRuntimeTexture3D() {
        const std::string texturePath = openFileDialogNative("Select 3D Texture Source", kImageFileFilter);
        if (texturePath.empty()) {
            return false;
        }

        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(texturePath, "Texture3D"), existingNames);

        if (!AssetLoader::loadTexture3D(textureName, texturePath, m_runtimeTexture3DDepth)) {
            appendTerminalLine("Failed to import 3D texture: " + texturePath);
            return false;
        }

        m_activeScene.addAsset(textureName, texturePath);
        appendTerminalLine("Imported 3D texture: " + textureName + " (depth " + std::to_string(m_runtimeTexture3DDepth) + ")");
        return true;
    }

    bool UILayer::loadRuntimeShader() {
        const std::string vertexPath = openFileDialogNative("Select Vertex Shader", kVertexShaderFilter);
        if (vertexPath.empty()) {
            return false;
        }

        const std::string fragmentPath = openFileDialogNative("Select Fragment Shader", kFragmentShaderFilter);
        if (fragmentPath.empty()) {
            return false;
        }

        std::string baseName = deriveAssetBaseName(vertexPath, "Shader");
        if (baseName.ends_with("_vert")) {
            baseName = baseName.substr(0, baseName.size() - 5);
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string shaderName = makeUniqueAssetName(baseName, existingNames);

        if (!AssetLoader::loadShader(shaderName, vertexPath, fragmentPath)) {
            appendTerminalLine("Failed to import shader pair: " + vertexPath + " + " + fragmentPath);
            return false;
        }

        m_activeScene.addAsset(shaderName + "_vert", vertexPath);
        m_activeScene.addAsset(shaderName + "_frag", fragmentPath);
        appendTerminalLine("Imported shader pair: " + shaderName);
        return true;
    }

    bool UILayer::loadRuntimeComputeShader() {
        const std::string computePath = openFileDialogNative("Select Compute Shader", kComputeShaderFilter);
        if (computePath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string shaderName = makeUniqueAssetName(deriveAssetBaseName(computePath, "Compute"), existingNames);

        if (!AssetLoader::loadComputeShader(shaderName, computePath)) {
            appendTerminalLine("Failed to import compute shader: " + computePath);
            return false;
        }

        m_activeScene.addAsset(shaderName, computePath);
        appendTerminalLine("Imported compute shader: " + shaderName);
        return true;
    }

    bool UILayer::loadRuntimeModel() {
        const std::string modelPath = openFileDialogNative("Select Model", kModelFileFilter);
        if (modelPath.empty()) {
            return false;
        }

        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();
        const std::string modelName = makeUniqueAssetName(deriveAssetBaseName(modelPath, "Model"), existingNames);

        if (!AssetLoader::loadModel(modelName, modelPath)) {
            appendTerminalLine("Failed to import model: " + modelPath);
            return false;
        }

        AssetLoader::setActiveModel(modelName);
        m_activeScene.addAsset(modelName, modelPath);
        appendTerminalLine("Imported model and set active: " + modelName);
        return true;
    }

    void UILayer::appendTerminalLine(const std::string& line) {
        m_terminalLines.push_back(line);
        constexpr std::size_t maxLogLines = 500;
        if (m_terminalLines.size() > maxLogLines) {
            const std::size_t overflow = m_terminalLines.size() - maxLogLines;
            m_terminalLines.erase(m_terminalLines.begin(), m_terminalLines.begin() + static_cast<std::ptrdiff_t>(overflow));
        }

        m_scrollTerminalToBottom = true;
        LOG_INFO("UI: " + line);
    }

    void UILayer::drawTopNavbar() {
        if (!ImGui::BeginMainMenuBar()) {
            return;
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                appendTerminalLine("TODO: New Scene flow will be implemented.");
            }
            if (ImGui::MenuItem("Open Scene")) {
                appendTerminalLine("TODO: Open Scene flow will be implemented.");
            }
            if (ImGui::MenuItem("Save Scene")) {
                appendTerminalLine("TODO: Save Scene flow will be implemented.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene")) {
            const bool isEdit = m_activeScene.getState() == SceneState::Edit;
            const bool isPlay = m_activeScene.getState() == SceneState::Play;
            const bool isPause = m_activeScene.getState() == SceneState::Pause;

            if (ImGui::MenuItem("Edit Mode", nullptr, isEdit)) {
                m_activeScene.setState(SceneState::Edit);
                m_activeScene.setGameStateValue("Mode", "Edit");
                appendTerminalLine("Scene switched to Edit mode.");
            }
            if (ImGui::MenuItem("Play Mode", nullptr, isPlay)) {
                m_activeScene.setState(SceneState::Play);
                m_activeScene.setGameStateValue("Mode", "Play");
                appendTerminalLine("Scene switched to Play mode.");
            }
            if (ImGui::MenuItem("Pause Mode", nullptr, isPause)) {
                m_activeScene.setState(SceneState::Pause);
                m_activeScene.setGameStateValue("Mode", "Pause");
                appendTerminalLine("Scene switched to Pause mode.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            if (ImGui::MenuItem("Reset Layout")) {
                appendTerminalLine("Layout reset support is reserved for future implementation.");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());

        ImGui::EndMainMenuBar();
    }

    void UILayer::drawSceneHierarchyPanel(float panelTop, float panelHeight, float panelWidth) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, panelTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Scene Hierarchy", nullptr, windowFlags);

        const auto& entities = m_activeScene.getEntities();

        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
        ImGui::Text("Entities: %d", static_cast<int>(entities.size()));
        ImGui::Separator();

        for (std::size_t i = 0; i < entities.size(); ++i) {
            const bool selected = (m_selectedEntityIndex == static_cast<int>(i));
            if (ImGui::Selectable(entities[i].c_str(), selected)) {
                m_selectedEntityIndex = static_cast<int>(i);
                m_activeScene.setGameStateValue("SelectedEntity", entities[i]);
            }
        }

        if (entities.empty()) {
            ImGui::TextDisabled("No entities in this scene.");
        }

        ImGui::Separator();
        if (ImGui::Button("Add Empty Entity")) {
            const std::string entityName = "Entity " + std::to_string(entities.size() + 1);
            m_activeScene.addEntity(entityName);
            appendTerminalLine("Created " + entityName + ".");
        }

        ImGui::SameLine();
        const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Remove Selected") && hasSelection) {
            const std::string selectedEntityName = entities[static_cast<std::size_t>(m_selectedEntityIndex)];
            if (m_activeScene.removeEntity(selectedEntityName)) {
                appendTerminalLine("Removed " + selectedEntityName + ".");
            }
            m_selectedEntityIndex = -1;
            m_activeScene.setGameStateValue("SelectedEntity", "None");
        }

        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        ImGui::End();
    }

    void UILayer::drawSceneViewPanel(float deltaTime, float panelTop, float panelHeight, float panelX, float panelWidth) {
        ImGui::SetNextWindowPos(ImVec2(panelX, panelTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Scene View", nullptr, windowFlags);

        ImGui::Text("Renderer Output");
        ImGui::Separator();
        ImGui::Text("Frame dt: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("FPS: %.1f", deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f);
        ImGui::Text("Mode: %s", sceneStateToString(m_activeScene.getState()));

        const std::optional<std::string> selectedEntity = m_activeScene.getGameStateValue("SelectedEntity");
        ImGui::Text("Selected Entity: %s", selectedEntity ? selectedEntity->c_str() : "None");

        ImGui::Spacing();
        const ImVec2 availableRegion = ImGui::GetContentRegionAvail();

        if (availableRegion.x > 2.0f && availableRegion.y > 2.0f) {
            const int desiredViewportWidth = std::max(1, static_cast<int>(availableRegion.x));
            const int desiredViewportHeight = std::max(1, static_cast<int>(availableRegion.y));

            const bool viewportSizeChanged = desiredViewportWidth != m_pendingViewportWidth || desiredViewportHeight != m_pendingViewportHeight;
            if (viewportSizeChanged) {
                m_pendingViewportWidth = desiredViewportWidth;
                m_pendingViewportHeight = desiredViewportHeight;
                m_viewportResizeDebounceTimer = 0.0f;
            } else {
                m_viewportResizeDebounceTimer += std::max(0.0f, deltaTime);
            }

            const int currentViewportWidth = Renderer::getViewportWidth();
            const int currentViewportHeight = Renderer::getViewportHeight();
            const bool needsViewportResize = currentViewportWidth != m_pendingViewportWidth || currentViewportHeight != m_pendingViewportHeight;

            if (needsViewportResize && (
                m_viewportResizeDebounceTimer >= m_viewportResizeDebounceDelaySeconds ||
                currentViewportWidth <= 0 ||
                currentViewportHeight <= 0
            )) {
                Renderer::setViewportSize(m_pendingViewportWidth, m_pendingViewportHeight);
                m_viewportResizeDebounceTimer = 0.0f;
            }
        }

        const unsigned int frameTextureID = Renderer::getFrameTextureID();
        const int renderWidth = Renderer::getViewportWidth();
        const int renderHeight = Renderer::getViewportHeight();

        ImGui::Text("Render Target: %dx%d", renderWidth, renderHeight);

        if (frameTextureID == 0 || renderWidth <= 0 || renderHeight <= 0 || availableRegion.x <= 2.0f || availableRegion.y <= 2.0f) {
            m_sceneViewImageHovered = false;
            ImGui::TextDisabled("Renderer output is not ready yet.");
            ImGui::TextWrapped("Camera controls: MMB orbit, wheel zoom, Ctrl+RMB pan, RMB options.");
            ImGui::End();
            return;
        }

        const float renderAspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
        float imageWidth = availableRegion.x;
        float imageHeight = imageWidth / renderAspect;

        if (imageHeight > availableRegion.y) {
            imageHeight = availableRegion.y;
            imageWidth = imageHeight * renderAspect;
        }

        const float xOffset = (availableRegion.x - imageWidth) * 0.5f;
        const float yOffset = (availableRegion.y - imageHeight) * 0.5f;
        if (xOffset > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + xOffset);
        }
        if (yOffset > 0.0f) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
        }

        const ImTextureID textureHandle = (ImTextureID)(uintptr_t)frameTextureID;
        ImGui::Image(textureHandle, ImVec2(imageWidth, imageHeight), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        m_sceneViewImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        updateSceneCameraController(m_sceneViewImageHovered);

        ImGui::Spacing();
        ImGui::TextDisabled("MMB: Orbit  |  Wheel: Zoom  |  Ctrl+RMB: Pan  |  RMB: Options");

        ImGui::End();
    }

    void UILayer::drawAssetsPanel(float panelTop, float panelHeight, float panelX, float panelWidth) {
        ImGui::SetNextWindowPos(ImVec2(panelX, panelTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Assets", nullptr, windowFlags);

        if (ImGui::BeginTabBar("AssetsTabs")) {
            if (ImGui::BeginTabItem("Assets")) {
                ImGui::Text("Runtime Import");
                ImGui::Separator();

                if (ImGui::Button("Load 2D Texture")) {
                    loadRuntimeTexture2D();
                }

                ImGui::SameLine();
                if (ImGui::Button("Load 3D Texture")) {
                    loadRuntimeTexture3D();
                }

                ImGui::SetNextItemWidth(110.0f);
                if (ImGui::InputInt("3D Depth", &m_runtimeTexture3DDepth, 1, 8)) {
                    m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);
                }

                if (ImGui::Button("Load Shader Pair")) {
                    loadRuntimeShader();
                }

                ImGui::SameLine();
                if (ImGui::Button("Load Compute Shader")) {
                    loadRuntimeComputeShader();
                }

                if (ImGui::Button("Load Model")) {
                    loadRuntimeModel();
                }

                ImGui::Spacing();
                ImGui::Text("Scene Asset Registry");
                ImGui::Separator();

                for (const SceneAsset& asset : m_activeScene.getAssets()) {
                    ImGui::BulletText("%s", asset.name.c_str());
                    ImGui::TextDisabled("%s", asset.path.c_str());
                }

                if (m_activeScene.getAssets().empty()) {
                    ImGui::TextDisabled("No assets loaded.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Model Shading Assignment");

                std::vector<std::string> modelNames = AssetLoader::getModelNames();
                std::vector<std::string> shaderNames = AssetLoader::getShaderNames();
                if (modelNames.empty()) {
                    ImGui::TextDisabled("No models loaded through AssetLoader.");
                } else if (shaderNames.empty()) {
                    ImGui::TextDisabled("No shaders loaded through AssetLoader.");
                } else {
                    std::string activeModel = AssetLoader::getActiveModel();
                    if (activeModel.empty()) {
                        activeModel = modelNames.front();
                        AssetLoader::setActiveModel(activeModel);
                    }

                    const char* activeModelLabel = activeModel.c_str();
                    if (ImGui::BeginCombo("Active Model", activeModelLabel)) {
                        for (const std::string& modelName : modelNames) {
                            const bool selected = modelName == activeModel;
                            if (ImGui::Selectable(modelName.c_str(), selected)) {
                                AssetLoader::setActiveModel(modelName);
                                appendTerminalLine("Active model set to " + modelName + ".");
                                activeModel = modelName;
                            }

                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    std::string assignedShader = AssetLoader::getModelShader(activeModel);
                    if (assignedShader.empty()) {
                        assignedShader = shaderNames.front();
                        AssetLoader::setModelShader(activeModel, assignedShader);
                    }

                    if (ImGui::BeginCombo("Model Shader", assignedShader.c_str())) {
                        for (const std::string& shaderName : shaderNames) {
                            const bool selected = shaderName == assignedShader;
                            if (ImGui::Selectable(shaderName.c_str(), selected)) {
                                if (AssetLoader::setModelShader(activeModel, shaderName)) {
                                    appendTerminalLine("Assigned shader " + shaderName + " to model " + activeModel + ".");
                                    assignedShader = shaderName;
                                }
                            }

                            if (selected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::TextDisabled("Loaded 2D Textures: %d", static_cast<int>(AssetLoader::getTexture2DNames().size()));
                    ImGui::TextDisabled("Loaded 3D Textures: %d", static_cast<int>(AssetLoader::getTexture3DNames().size()));
                    ImGui::TextDisabled("Loaded Compute Shaders: %d", static_cast<int>(AssetLoader::getComputeShaderNames().size()));
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Scripts")) {
                for (const SceneScript& script : m_activeScene.getScripts()) {
                    bool enabled = script.enabled;
                    const std::string checkboxId = "##script_enabled_" + script.name;
                    if (ImGui::Checkbox(checkboxId.c_str(), &enabled)) {
                        if (m_activeScene.setScriptEnabled(script.name, enabled)) {
                            appendTerminalLine("Script " + script.name + (enabled ? " enabled." : " disabled."));
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("%s", script.name.c_str());
                    ImGui::TextDisabled("%s", script.path.c_str());
                }

                if (m_activeScene.getScripts().empty()) {
                    ImGui::TextDisabled("No scripts linked.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Cameras")) {
                for (const SceneCamera& camera : m_activeScene.getCameras()) {
                    const std::string primaryId = "##camera_primary_" + camera.name;
                    if (ImGui::RadioButton(primaryId.c_str(), camera.primary)) {
                        if (m_activeScene.setPrimaryCamera(camera.name)) {
                            appendTerminalLine("Primary camera set to " + camera.name + ".");
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("%s", camera.name.c_str());
                    ImGui::TextDisabled("Type: %s", cameraTypeToString(camera.camera.getType()));

                    if (camera.primary) {
                        ImGui::TextDisabled("Primary");
                    }
                }

                if (m_activeScene.getCameras().empty()) {
                    ImGui::TextDisabled("No cameras available.");
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Settings")) {
                ImGui::Text("Scene Settings");
                ImGui::Separator();
                for (const auto& [key, value] : m_activeScene.getSettings()) {
                    ImGui::BulletText("%s = %s", key.c_str(), value.c_str());
                }

                if (m_activeScene.getSettings().empty()) {
                    ImGui::TextDisabled("No scene settings.");
                }

                ImGui::Spacing();
                ImGui::Text("Game State");
                ImGui::Separator();
                for (const auto& [key, value] : m_activeScene.getGameState()) {
                    ImGui::BulletText("%s = %s", key.c_str(), value.c_str());
                }

                if (m_activeScene.getGameState().empty()) {
                    ImGui::TextDisabled("No game state entries.");
                }

                drawEngineSettingsSection();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Inspector")) {
                const auto& entities = m_activeScene.getEntities();
                const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());

                ImGui::Text("Scene Element Inspector");
                ImGui::Separator();
                ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
                ImGui::Text("State: %s", sceneStateToString(m_activeScene.getState()));
                ImGui::Text("Entities: %d", static_cast<int>(entities.size()));
                ImGui::Text("Assets: %d", static_cast<int>(m_activeScene.getAssets().size()));
                ImGui::Text("Scripts: %d", static_cast<int>(m_activeScene.getScripts().size()));
                ImGui::Text("Cameras: %d", static_cast<int>(m_activeScene.getCameras().size()));

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Selected Entity");

                if (hasSelection) {
                    const std::string& selectedEntity = entities[static_cast<std::size_t>(m_selectedEntityIndex)];
                    ImGui::Text("Name: %s", selectedEntity.c_str());
                    ImGui::Text("Index: %d", m_selectedEntityIndex);
                    ImGui::TextWrapped("This panel is reserved for future per-entity component editing.");
                } else {
                    ImGui::TextDisabled("No selected entity.");
                    ImGui::TextWrapped("Select an entity from Scene Hierarchy to inspect it here.");
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Text("Primary Camera");
                bool foundPrimary = false;
                for (const SceneCamera& camera : m_activeScene.getCameras()) {
                    if (!camera.primary) {
                        continue;
                    }

                    ImGui::Text("%s", camera.name.c_str());
                    ImGui::TextDisabled("Type: %s", cameraTypeToString(camera.camera.getType()));
                    foundPrimary = true;
                    break;
                }

                if (!foundPrimary) {
                    ImGui::TextDisabled("No primary camera set.");
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

    void UILayer::drawTerminalPanel(float panelTop, float panelHeight, float panelWidth) {
        ImGui::SetNextWindowPos(ImVec2(0.0f, panelTop), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
        ImGui::Begin("Terminal", nullptr, windowFlags);

        if (ImGui::Button("Clear")) {
            m_terminalLines.clear();
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy")) {
            std::string fullLog;
            for (const std::string& line : m_terminalLines) {
                fullLog += line;
                fullLog += '\n';
            }
            ImGui::SetClipboardText(fullLog.c_str());
            appendTerminalLine("Terminal log copied to clipboard.");
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_autoScrollTerminal);
        ImGui::Separator();

        ImGui::BeginChild("TerminalLogRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (const std::string& line : m_terminalLines) {
            ImGui::TextUnformatted(line.c_str());
        }

        if (m_autoScrollTerminal && m_scrollTerminalToBottom) {
            ImGui::SetScrollHereY(1.0f);
        }

        m_scrollTerminalToBottom = false;
        ImGui::EndChild();
        ImGui::End();
    }

    void UILayer::drawEngineSettingsSection() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Engine Settings");

        if (m_window == nullptr || m_engineConfig == nullptr) {
            ImGui::TextDisabled("Engine settings are not available.");
            return;
        }

        auto [monitorWidth, monitorHeight] = m_window->getPrimaryMonitorSize();
        ImGui::Text("Primary Monitor: %d x %d", monitorWidth, monitorHeight);

        bool changed = false;
        if (ImGui::Checkbox("Auto Detect Monitor Size", &m_editableEngineSettings.autoDetectMonitorSize)) {
            changed = true;
        }

        if (m_editableEngineSettings.autoDetectMonitorSize) {
            ImGui::BeginDisabled();
        }

        if (ImGui::InputInt("Window Width", &m_editableEngineSettings.windowWidth, 64, 256)) {
            changed = true;
        }
        if (ImGui::InputInt("Window Height", &m_editableEngineSettings.windowHeight, 64, 256)) {
            changed = true;
        }

        if (m_editableEngineSettings.autoDetectMonitorSize) {
            ImGui::EndDisabled();
        }

        if (ImGui::Checkbox("Fullscreen", &m_editableEngineSettings.fullscreen)) {
            if (m_editableEngineSettings.fullscreen) {
                m_editableEngineSettings.dockedFullscreenWindowed = false;
            }
            changed = true;
        }

        if (ImGui::Checkbox("Docked Fullscreen (Windowed)", &m_editableEngineSettings.dockedFullscreenWindowed)) {
            if (m_editableEngineSettings.dockedFullscreenWindowed) {
                m_editableEngineSettings.fullscreen = false;
            }
            changed = true;
        }

        if (ImGui::InputText("Window Title", m_windowTitleBuffer.data(), m_windowTitleBuffer.size())) {
            m_editableEngineSettings.windowTitle = std::string(m_windowTitleBuffer.data());
            changed = true;
        }

        if (changed) {
            m_editableEngineSettings.windowWidth = std::max(64, m_editableEngineSettings.windowWidth);
            m_editableEngineSettings.windowHeight = std::max(64, m_editableEngineSettings.windowHeight);
            if (m_editableEngineSettings.windowTitle.empty()) {
                m_editableEngineSettings.windowTitle = "Valkron Engine";
                std::snprintf(m_windowTitleBuffer.data(), m_windowTitleBuffer.size(), "%s", m_editableEngineSettings.windowTitle.c_str());
            }

            m_engineSettingsDirty = true;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        m_window->getFramebufferSize(framebufferWidth, framebufferHeight);
        ImGui::Text("Current Framebuffer: %d x %d", framebufferWidth, framebufferHeight);

        const bool disableApplyControls = !m_engineSettingsDirty;
        if (disableApplyControls) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Apply and Save")) {
            m_editableEngineSettings.windowTitle = std::string(m_windowTitleBuffer.data());
            if (m_editableEngineSettings.windowTitle.empty()) {
                m_editableEngineSettings.windowTitle = "Valkron Engine";
            }

            m_window->applySettings(m_editableEngineSettings);

            m_engineConfig->setSettings(m_editableEngineSettings);
            if (m_engineConfig->save()) {
                appendTerminalLine("Engine settings applied and saved.");
            } else {
                appendTerminalLine("Failed to save engine settings file.");
            }

            m_engineSettingsDirty = false;

            int resizedFramebufferWidth = 0;
            int resizedFramebufferHeight = 0;
            m_window->getFramebufferSize(resizedFramebufferWidth, resizedFramebufferHeight);
            Renderer::onWindowResize(resizedFramebufferWidth, resizedFramebufferHeight);
        }

        if (disableApplyControls) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();
        if (ImGui::Button("Reload from File")) {
            if (m_engineConfig->load()) {
                syncEngineSettingsEditorState();
                m_window->applySettings(m_engineConfig->getSettings());

                int resizedFramebufferWidth = 0;
                int resizedFramebufferHeight = 0;
                m_window->getFramebufferSize(resizedFramebufferWidth, resizedFramebufferHeight);
                Renderer::onWindowResize(resizedFramebufferWidth, resizedFramebufferHeight);

                appendTerminalLine("Engine settings reloaded from config file.");
            } else {
                appendTerminalLine("Failed to reload engine settings from config file.");
            }
        }
    }

    void UILayer::syncEngineSettingsEditorState() {
        if (m_engineConfig == nullptr) {
            return;
        }

        m_editableEngineSettings = m_engineConfig->getSettings();
        std::fill(m_windowTitleBuffer.begin(), m_windowTitleBuffer.end(), '\0');
        std::snprintf(m_windowTitleBuffer.data(), m_windowTitleBuffer.size(), "%s", m_editableEngineSettings.windowTitle.c_str());
        m_engineSettingsDirty = false;
    }

}
