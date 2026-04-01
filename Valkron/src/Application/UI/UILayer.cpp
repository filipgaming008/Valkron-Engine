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
#include "imgui_internal.h"

#include "glm/glm.hpp"
#include "glm/geometric.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/trigonometric.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <utility>
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

    std::string toLowercase(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string extractAssetFolderName(const SceneAsset& asset) {
        const std::filesystem::path assetPath(asset.path);
        const std::string folderName = assetPath.parent_path().filename().string();
        if (!folderName.empty()) {
            return folderName;
        }

        return "Root";
    }

    std::string getAssetExtensionLowercase(const SceneAsset& asset) {
        return toLowercase(std::filesystem::path(asset.path).extension().string());
    }

    const char* getAssetIconToken(const SceneAsset& asset) {
        const std::string extension = getAssetExtensionLowercase(asset);
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm") {
            return "IMG";
        }
        if (extension == ".vert" || extension == ".frag" || extension == ".vs" || extension == ".fs" || extension == ".glsl" || extension == ".comp") {
            return "SHD";
        }
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds") {
            return "MOD";
        }
        if (extension == ".lua") {
            return "LUA";
        }

        return "AST";
    }

    ImVec4 getAssetIconColor(const SceneAsset& asset) {
        const std::string extension = getAssetExtensionLowercase(asset);
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm") {
            return ImVec4(0.18f, 0.34f, 0.58f, 1.0f);
        }
        if (extension == ".vert" || extension == ".frag" || extension == ".vs" || extension == ".fs" || extension == ".glsl" || extension == ".comp") {
            return ImVec4(0.42f, 0.24f, 0.14f, 1.0f);
        }
        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds") {
            return ImVec4(0.16f, 0.44f, 0.28f, 1.0f);
        }
        if (extension == ".lua") {
            return ImVec4(0.45f, 0.34f, 0.14f, 1.0f);
        }

        return ImVec4(0.24f, 0.24f, 0.30f, 1.0f);
    }

    bool isCameraEntityName(const std::string& entityName) {
        return toLowercase(entityName).find("camera") != std::string::npos;
    }

    bool isLightEntityName(const std::string& entityName) {
        return toLowercase(entityName).find("light") != std::string::npos;
    }

    const char* getEntityCategoryToken(const SceneEntity& entity) {
        const std::string lowerName = toLowercase(entity.name);
        if (lowerName.find("camera") != std::string::npos) {
            return "CAM";
        }
        if (lowerName.find("light") != std::string::npos) {
            return "LGT";
        }
        if (lowerName.find("player") != std::string::npos) {
            return "PLY";
        }

        return "ENT";
    }

    const char* getEntityIconToken(const SceneEntity& entity) {
        const std::string lowerName = toLowercase(entity.name);
        if (lowerName.find("camera") != std::string::npos) {
            return "[@]";
        }
        if (lowerName.find("light") != std::string::npos) {
            return "[*]";
        }

        return "[ ]";
    }

    ImVec4 getEntityBadgeColor(const SceneEntity& entity) {
        const std::string lowerName = toLowercase(entity.name);
        if (lowerName.find("camera") != std::string::npos) {
            return ImVec4(0.16f, 0.30f, 0.54f, 1.0f);
        }
        if (lowerName.find("light") != std::string::npos) {
            return ImVec4(0.42f, 0.32f, 0.10f, 1.0f);
        }
        if (lowerName.find("player") != std::string::npos) {
            return ImVec4(0.20f, 0.42f, 0.20f, 1.0f);
        }

        return ImVec4(0.30f, 0.30f, 0.34f, 1.0f);
    }

    ImVec4 brightenColor(const ImVec4& color, float amount) {
        return ImVec4(
            std::clamp(color.x + amount, 0.0f, 1.0f),
            std::clamp(color.y + amount, 0.0f, 1.0f),
            std::clamp(color.z + amount, 0.0f, 1.0f),
            color.w
        );
    }

    glm::mat4 composeLocalTransformMatrix(const SceneTransform& transform) {
        glm::mat4 matrix(1.0f);
        matrix = glm::translate(matrix, transform.position);
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        matrix = glm::scale(matrix, glm::vec3(
            std::max(0.01f, transform.scale.x),
            std::max(0.01f, transform.scale.y),
            std::max(0.01f, transform.scale.z)
        ));
        return matrix;
    }

    glm::mat4 composeEntityWorldTransformMatrix(const std::vector<SceneEntity>& entities, std::size_t entityIndex) {
        if (entityIndex >= entities.size()) {
            return glm::mat4(1.0f);
        }

        glm::mat4 worldMatrix = composeLocalTransformMatrix(entities[entityIndex].transform);

        int parentIndex = entities[entityIndex].parentIndex;
        std::size_t safety = 0;
        while (parentIndex >= 0 && parentIndex < static_cast<int>(entities.size()) && safety < entities.size()) {
            worldMatrix = composeLocalTransformMatrix(entities[static_cast<std::size_t>(parentIndex)].transform) * worldMatrix;
            parentIndex = entities[static_cast<std::size_t>(parentIndex)].parentIndex;
            ++safety;
        }

        return worldMatrix;
    }

    glm::vec3 extractWorldPosition(const glm::mat4& worldMatrix) {
        return glm::vec3(worldMatrix[3]);
    }

    glm::vec3 extractForwardDirection(const glm::mat4& worldMatrix) {
        glm::vec3 forward = glm::mat3(worldMatrix) * glm::vec3(0.0f, 0.0f, -1.0f);
        if (glm::length(forward) < 0.0001f) {
            return glm::vec3(0.0f, 0.0f, -1.0f);
        }

        return glm::normalize(forward);
    }

    std::optional<float> raySphereIntersectionDistance(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& sphereCenter,
        float sphereRadius
    ) {
        const glm::vec3 oc = rayOrigin - sphereCenter;
        const float b = 2.0f * glm::dot(oc, rayDirection);
        const float c = glm::dot(oc, oc) - (sphereRadius * sphereRadius);
        const float discriminant = b * b - 4.0f * c;
        if (discriminant < 0.0f) {
            return std::nullopt;
        }

        const float root = std::sqrt(discriminant);
        const float nearDistance = (-b - root) * 0.5f;
        const float farDistance = (-b + root) * 0.5f;
        if (nearDistance > 0.0f) {
            return nearDistance;
        }

        if (farDistance > 0.0f) {
            return farDistance;
        }

        return std::nullopt;
    }

    bool isTextureImageExtension(const std::string& extension) {
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga" || extension == ".ppm";
    }

    bool isModelAssetExtension(const std::string& extension) {
        return extension == ".obj" || extension == ".fbx" || extension == ".gltf" || extension == ".glb" || extension == ".dae" || extension == ".3ds";
    }

    bool isComputeShaderExtension(const std::string& extension) {
        return extension == ".comp";
    }

    bool isVertexShaderExtension(const std::string& extension) {
        return extension == ".vert" || extension == ".vs";
    }

    bool isFragmentShaderExtension(const std::string& extension) {
        return extension == ".frag" || extension == ".fs";
    }

    std::optional<std::pair<std::string, std::string>> resolveShaderPair(const std::filesystem::path& selectedPath) {
        const std::filesystem::path parent = selectedPath.parent_path();
        const std::string stem = selectedPath.stem().string();
        const std::string lowerStem = toLowercase(stem);

        std::vector<std::string> candidateStems;
        candidateStems.push_back(stem);

        auto addStemCandidate = [&candidateStems](const std::string& value) {
            if (value.empty()) {
                return;
            }

            if (std::find(candidateStems.begin(), candidateStems.end(), value) == candidateStems.end()) {
                candidateStems.push_back(value);
            }
        };

        if (lowerStem.ends_with("_vert") && stem.size() > 5) {
            addStemCandidate(stem.substr(0, stem.size() - 5));
        }
        if (lowerStem.ends_with("_frag") && stem.size() > 5) {
            addStemCandidate(stem.substr(0, stem.size() - 5));
        }
        if (lowerStem.ends_with("_vs") && stem.size() > 3) {
            addStemCandidate(stem.substr(0, stem.size() - 3));
        }
        if (lowerStem.ends_with("_fs") && stem.size() > 3) {
            addStemCandidate(stem.substr(0, stem.size() - 3));
        }

        for (const std::string& candidateStem : candidateStems) {
            const std::filesystem::path vertexPath = parent / (candidateStem + ".vert");
            const std::filesystem::path fragmentPath = parent / (candidateStem + ".frag");
            if (std::filesystem::exists(vertexPath) && std::filesystem::exists(fragmentPath)) {
                return std::make_pair(vertexPath.string(), fragmentPath.string());
            }

            const std::filesystem::path vertexPathShort = parent / (candidateStem + ".vs");
            const std::filesystem::path fragmentPathShort = parent / (candidateStem + ".fs");
            if (std::filesystem::exists(vertexPathShort) && std::filesystem::exists(fragmentPathShort)) {
                return std::make_pair(vertexPathShort.string(), fragmentPathShort.string());
            }
        }

        const std::string selectedExtension = toLowercase(selectedPath.extension().string());
        if (isVertexShaderExtension(selectedExtension)) {
            for (const std::string fragmentExtension : {".frag", ".fs"}) {
                std::filesystem::path fragmentPath = selectedPath;
                fragmentPath.replace_extension(fragmentExtension);
                if (std::filesystem::exists(fragmentPath)) {
                    return std::make_pair(selectedPath.string(), fragmentPath.string());
                }
            }
        }

        if (isFragmentShaderExtension(selectedExtension)) {
            for (const std::string vertexExtension : {".vert", ".vs"}) {
                std::filesystem::path vertexPath = selectedPath;
                vertexPath.replace_extension(vertexExtension);
                if (std::filesystem::exists(vertexPath)) {
                    return std::make_pair(vertexPath.string(), selectedPath.string());
                }
            }
        }

        return std::nullopt;
    }

    const char kImageFileFilter[] = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm\0All Files\0*.*\0\0";
    const char kAllRuntimeAssetFilter[] = "Supported Assets\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm;*.vert;*.vs;*.frag;*.fs;*.comp;*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds\0All Files\0*.*\0\0";
    const char kVertexShaderFilter[] = "Vertex Shaders\0*.vert;*.vs;*.glsl\0All Files\0*.*\0\0";
    const char kFragmentShaderFilter[] = "Fragment Shaders\0*.frag;*.fs;*.glsl\0All Files\0*.*\0\0";
    const char kComputeShaderFilter[] = "Compute Shaders\0*.comp;*.glsl\0All Files\0*.*\0\0";
    const char kModelFileFilter[] = "Model Files\0*.obj;*.fbx;*.gltf;*.glb;*.dae;*.3ds\0All Files\0*.*\0\0";

    void UILayer::bindEngineSettings(Window* window, EngineConfig* engineConfig) {
        m_window = window;
        m_engineConfig = engineConfig;
        syncEngineSettingsEditorState();
    }

    void UILayer::setSelectedEntity(int entityIndex) {
        const auto& entities = m_activeScene.getEntityData();
        if (entityIndex < 0 || entityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
            return;
        }

        m_selectedEntityIndex = entityIndex;
        m_selectedEntityNameBufferEntityIndex = -1;
        m_activeScene.setGameStateValue("SelectedEntity", entities[static_cast<std::size_t>(entityIndex)].name);
    }

    void UILayer::clearEntitySelection() {
        m_selectedEntityIndex = -1;
        m_selectedEntityNameBufferEntityIndex = -1;
        m_activeScene.setGameStateValue("SelectedEntity", "None");
    }

    void UILayer::onAttach() {
        m_activeScene = Scene("Sandbox Scene");

        m_activeScene.addEntity("Camera");
        m_activeScene.addEntity("Directional Light");
        m_activeScene.addEntity("Cube");

        if (const std::optional<std::size_t> cameraEntityIndex = m_activeScene.findEntityIndex("Camera"); cameraEntityIndex.has_value()) {
            if (SceneEntity* cameraEntity = m_activeScene.getEntityByIndex(cameraEntityIndex.value()); cameraEntity != nullptr) {
                cameraEntity->transform.position = glm::vec3(0.0f, 1.6f, 4.0f);
                cameraEntity->transform.rotation = glm::vec3(-12.0f, 0.0f, 0.0f);
            }
        }

        if (const std::optional<std::size_t> lightEntityIndex = m_activeScene.findEntityIndex("Directional Light"); lightEntityIndex.has_value()) {
            if (SceneEntity* lightEntity = m_activeScene.getEntityByIndex(lightEntityIndex.value()); lightEntity != nullptr) {
                lightEntity->transform.position = glm::vec3(3.0f, 4.0f, 3.0f);
            }
        }

        if (const std::optional<std::size_t> cubeEntityIndex = m_activeScene.findEntityIndex("Cube"); cubeEntityIndex.has_value()) {
            if (SceneEntity* cubeEntity = m_activeScene.getEntityByIndex(cubeEntityIndex.value()); cubeEntity != nullptr) {
                cubeEntity->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
                cubeEntity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
                cubeEntity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
            }
        }

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
        m_selectedEntityNameBufferEntityIndex = -1;
        std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
        std::fill(m_hierarchySearchBuffer.begin(), m_hierarchySearchBuffer.end(), '\0');
        m_pendingViewportWidth = 0;
        m_pendingViewportHeight = 0;
        m_viewportResizeDebounceTimer = 0.0f;
        m_sceneViewImageHovered = false;
        m_sceneCameraControllerInitialized = false;
        m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);
        m_assetBrowserFolderFilter = "All";
        m_selectedAssetIndex = -1;
        m_showSettingsPanel = false;
        m_showDebugPanel = false;
        m_dockspaceBuilt = false;

        syncEngineSettingsEditorState();
        appendTerminalLine("UI Manager attached (Dear ImGui). Editor layout ready.");
    }

    void UILayer::onDetach() {
        appendTerminalLine("UI Manager detached.");
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        drawTopNavbar();
        drawDockspaceHost();

        m_lastFrameDeltaTimeSeconds = std::max(0.0f, deltaTime);

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        if (displaySize.x <= 0.0f || displaySize.y <= 0.0f) {
            return;
        }

        const std::vector<SceneEntity>& entities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
        }

        std::vector<glm::mat4> entityWorldTransforms;
        std::vector<glm::vec3> lightEntityPositions;
        entityWorldTransforms.reserve(entities.size());
        lightEntityPositions.reserve(entities.size());

        std::optional<glm::mat4> runtimeCameraTransform;
        for (std::size_t entityIndex = 0; entityIndex < entities.size(); ++entityIndex) {
            const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(entities, entityIndex);
            entityWorldTransforms.push_back(worldTransform);

            if (isLightEntityName(entities[entityIndex].name)) {
                lightEntityPositions.push_back(extractWorldPosition(worldTransform));
            }

            if (!runtimeCameraTransform.has_value() && isCameraEntityName(entities[entityIndex].name)) {
                runtimeCameraTransform = worldTransform;
            }
        }

        Renderer::setSceneEntityTransforms(entityWorldTransforms, m_selectedEntityIndex);
        Renderer::setLightEntityPositions(lightEntityPositions);

        m_runtimeEntityCameraActive = false;
        if (m_activeScene.getState() == SceneState::Play && runtimeCameraTransform.has_value()) {
            const glm::vec3 cameraPosition = extractWorldPosition(runtimeCameraTransform.value());
            const glm::vec3 cameraForward = extractForwardDirection(runtimeCameraTransform.value());
            Renderer::setCameraLookAt(cameraPosition, cameraPosition + cameraForward);
            m_runtimeEntityCameraActive = true;
        }

        if (m_showSceneHierarchyPanel) {
            drawSceneHierarchyPanel();
        }

        if (m_showSceneViewPanel) {
            drawSceneViewPanel(deltaTime);
        }

        if (m_showInspectorPanel) {
            drawInspectorPanel();
        }

        if (m_showSettingsPanel) {
            drawSettingsPanel();
        }

        if (m_showBottomPanel) {
            drawBottomPanel();
        }

        if (m_showDebugPanel) {
            drawDebugPanel();
        }

        m_resetLayoutRequested = false;
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

    bool UILayer::loadRuntimeAssetAuto() {
        const std::string assetPath = openFileDialogNative("Import Asset", kAllRuntimeAssetFilter);
        if (assetPath.empty()) {
            return false;
        }

        const std::filesystem::path selectedPath(assetPath);
        const std::string extension = toLowercase(selectedPath.extension().string());
        const std::vector<std::string> existingNames = collectAllRuntimeAssetNames();

        if (isTextureImageExtension(extension)) {
            const std::string lowerPath = toLowercase(assetPath);
            const bool importAsVolume = lowerPath.find("_3d") != std::string::npos || lowerPath.find("volume") != std::string::npos;
            const std::string fallbackName = importAsVolume ? "Texture3D" : "Texture2D";
            const std::string textureName = makeUniqueAssetName(deriveAssetBaseName(assetPath, fallbackName), existingNames);

            const bool loaded = importAsVolume
                ? AssetLoader::loadTexture3D(textureName, assetPath, std::max(1, m_runtimeTexture3DDepth))
                : AssetLoader::loadTexture2D(textureName, assetPath);

            if (!loaded) {
                appendTerminalLine("Failed to import texture asset: " + assetPath);
                return false;
            }

            m_activeScene.addAsset(textureName, assetPath);
            appendTerminalLine("Imported texture: " + textureName + (importAsVolume ? " (3D)" : " (2D)"));
            return true;
        }

        if (isModelAssetExtension(extension)) {
            const std::string modelName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Model"), existingNames);
            if (!AssetLoader::loadModel(modelName, assetPath)) {
                appendTerminalLine("Failed to import model asset: " + assetPath);
                return false;
            }

            AssetLoader::setActiveModel(modelName);
            m_activeScene.addAsset(modelName, assetPath);
            appendTerminalLine("Imported model and set active: " + modelName);
            return true;
        }

        const std::string fileNameLower = toLowercase(selectedPath.filename().string());
        if (isComputeShaderExtension(extension) || (extension == ".glsl" && fileNameLower.find("comp") != std::string::npos)) {
            const std::string computeName = makeUniqueAssetName(deriveAssetBaseName(assetPath, "Compute"), existingNames);
            if (!AssetLoader::loadComputeShader(computeName, assetPath)) {
                appendTerminalLine("Failed to import compute shader: " + assetPath);
                return false;
            }

            m_activeScene.addAsset(computeName, assetPath);
            appendTerminalLine("Imported compute shader: " + computeName);
            return true;
        }

        if (isVertexShaderExtension(extension) || isFragmentShaderExtension(extension) || extension == ".glsl") {
            const std::optional<std::pair<std::string, std::string>> shaderPair = resolveShaderPair(selectedPath);
            if (!shaderPair.has_value()) {
                appendTerminalLine("Unable to auto-resolve shader pair. Select a .vert/.frag (or .vs/.fs) file with matching sibling file.");
                return false;
            }

            std::string baseName = deriveAssetBaseName(shaderPair->first, "Shader");
            if (baseName.ends_with("_vert")) {
                baseName = baseName.substr(0, baseName.size() - 5);
            }

            const std::string shaderName = makeUniqueAssetName(baseName, existingNames);
            if (!AssetLoader::loadShader(shaderName, shaderPair->first, shaderPair->second)) {
                appendTerminalLine("Failed to import shader pair: " + shaderPair->first + " + " + shaderPair->second);
                return false;
            }

            m_activeScene.addAsset(shaderName + "_vert", shaderPair->first);
            m_activeScene.addAsset(shaderName + "_frag", shaderPair->second);
            appendTerminalLine("Imported shader pair: " + shaderName);
            return true;
        }

        appendTerminalLine("Unsupported asset type for import: " + assetPath);
        return false;
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
            ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showSceneHierarchyPanel);
            ImGui::MenuItem("Scene View", nullptr, &m_showSceneViewPanel);
            ImGui::MenuItem("Inspector", nullptr, &m_showInspectorPanel);
            ImGui::MenuItem("Asset Browser", nullptr, &m_showBottomPanel);
            ImGui::MenuItem("Debug Panel", nullptr, &m_showDebugPanel);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) {
                m_showSceneHierarchyPanel = true;
                m_showSceneViewPanel = true;
                m_showInspectorPanel = true;
                m_showSettingsPanel = false;
                m_showBottomPanel = true;
                m_showDebugPanel = false;
                m_resetLayoutRequested = true;
                appendTerminalLine("Editor layout reset to default window arrangement.");
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("Open Settings")) {
                m_showSettingsPanel = true;
            }
            if (m_showSettingsPanel && ImGui::MenuItem("Close Settings")) {
                m_showSettingsPanel = false;
            }
            if (ImGui::MenuItem("Reload Engine Settings In Editor")) {
                syncEngineSettingsEditorState();
                appendTerminalLine("Settings editor reloaded from current engine config.");
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());

        ImGui::EndMainMenuBar();
    }

    void UILayer::drawDockspaceHost() {
        ImGuiIO& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0) {
            return;
        }

        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGuiID dockspaceID = ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_None);

        if (!m_dockspaceBuilt || m_resetLayoutRequested) {
            m_dockspaceBuilt = true;

            ImGui::DockBuilderRemoveNode(dockspaceID);
            ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

            ImGuiID dockMain = dockspaceID;
            const ImGuiID dockLeft = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Left, 0.20f, nullptr, &dockMain);
            const ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.24f, nullptr, &dockMain);
            const ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);

            ImGui::DockBuilderDockWindow("Scene Hierarchy", dockLeft);
            ImGui::DockBuilderDockWindow("Scene View", dockMain);
            ImGui::DockBuilderDockWindow("Inspector", dockRight);
            ImGui::DockBuilderDockWindow("Asset Browser", dockBottom);

            ImGui::DockBuilderFinish(dockspaceID);
        }
    }

    void UILayer::drawSceneHierarchyPanel() {
        if (!ImGui::Begin("Scene Hierarchy", &m_showSceneHierarchyPanel)) {
            ImGui::End();
            return;
        }

        const auto& entities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex >= static_cast<int>(entities.size())) {
            clearEntitySelection();
        }

        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("| Hierarchy");
        ImGui::Text("Entities: %d", static_cast<int>(entities.size()));

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_hierarchySearchBuffer.data(), m_hierarchySearchBuffer.size());
        const std::string searchFilter = toLowercase(std::string(m_hierarchySearchBuffer.data()));
        const bool hasSearchFilter = !searchFilter.empty();
        ImGui::Separator();

        std::vector<std::vector<std::size_t>> childrenByParent(entities.size());
        for (std::size_t i = 0; i < entities.size(); ++i) {
            const int parentIndex = entities[i].parentIndex;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(entities.size())) {
                childrenByParent[static_cast<std::size_t>(parentIndex)].push_back(i);
            }
        }

        std::optional<std::size_t> pendingDeleteEntity;
        std::optional<std::size_t> pendingDuplicateEntity;
        std::optional<std::size_t> pendingCreateChildEntity;
        std::optional<std::pair<std::size_t, std::size_t>> pendingReparentEntity;
        std::optional<std::size_t> pendingClearParentByDrop;

        ImGui::TextDisabled("Drag entities here to place them at scene root");
        const bool rootSelected = m_selectedEntityIndex < 0;
        if (ImGui::Selectable("Scene Root", rootSelected, ImGuiSelectableFlags_SpanAvailWidth)) {
            clearEntitySelection();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy.EntityIndex")) {
                if (payload->DataSize == sizeof(std::size_t)) {
                    pendingClearParentByDrop = *static_cast<const std::size_t*>(payload->Data);
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Separator();

        std::vector<std::string> lowerEntityNames;
        lowerEntityNames.reserve(entities.size());
        for (const SceneEntity& entity : entities) {
            lowerEntityNames.push_back(toLowercase(entity.name));
        }

        std::function<bool(std::size_t)> subtreeMatchesFilter = [&](std::size_t entityIndex) {
            if (entityIndex >= entities.size()) {
                return false;
            }

            if (!hasSearchFilter) {
                return true;
            }

            if (lowerEntityNames[entityIndex].find(searchFilter) != std::string::npos) {
                return true;
            }

            for (std::size_t childIndex : childrenByParent[entityIndex]) {
                if (subtreeMatchesFilter(childIndex)) {
                    return true;
                }
            }

            return false;
        };

        std::function<void(std::size_t)> drawEntityNode = [&](std::size_t entityIndex) {
            if (entityIndex >= entities.size() || !subtreeMatchesFilter(entityIndex)) {
                return;
            }

            const SceneEntity& entity = entities[entityIndex];
            const bool hasChildren = !childrenByParent[entityIndex].empty();
            const bool selected = m_selectedEntityIndex == static_cast<int>(entityIndex);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
            if (!hasChildren) {
                flags |= ImGuiTreeNodeFlags_Leaf;
            }
            if (selected) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }

            ImVec4 badgeColor = getEntityBadgeColor(entity);
            if (selected) {
                badgeColor = brightenColor(badgeColor, 0.12f);
            }

            ImGui::PushStyleColor(ImGuiCol_Header, badgeColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, brightenColor(badgeColor, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, brightenColor(badgeColor, 0.14f));

            std::string treeLabel = std::string(getEntityIconToken(entity)) + " [" + getEntityCategoryToken(entity) + "] " + entity.name;
            if (hasChildren) {
                treeLabel += "  {" + std::to_string(childrenByParent[entityIndex].size()) + "}";
            }

            const bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(entityIndex + 1)), flags, "%s", treeLabel.c_str());
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                setSelectedEntity(static_cast<int>(entityIndex));
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                const std::size_t payloadEntityIndex = entityIndex;
                ImGui::SetDragDropPayload("SceneHierarchy.EntityIndex", &payloadEntityIndex, sizeof(payloadEntityIndex));
                ImGui::Text("Reparent %s", entity.name.c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SceneHierarchy.EntityIndex")) {
                    if (payload->DataSize == sizeof(std::size_t)) {
                        const std::size_t sourceEntityIndex = *static_cast<const std::size_t*>(payload->Data);
                        if (sourceEntityIndex != entityIndex) {
                            pendingReparentEntity = std::make_pair(sourceEntityIndex, entityIndex);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Select")) {
                    setSelectedEntity(static_cast<int>(entityIndex));
                }
                if (ImGui::MenuItem("Create Child Entity")) {
                    pendingCreateChildEntity = entityIndex;
                }
                if (ImGui::MenuItem("Duplicate Entity")) {
                    pendingDuplicateEntity = entityIndex;
                }
                if (entity.parentIndex >= 0 && ImGui::MenuItem("Clear Parent")) {
                    if (m_activeScene.setEntityParent(entityIndex, std::nullopt)) {
                        appendTerminalLine("Removed parent from " + entity.name + ".");
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Entity")) {
                    pendingDeleteEntity = entityIndex;
                }
                ImGui::EndPopup();
            }

            if (nodeOpen) {
                for (std::size_t childIndex : childrenByParent[entityIndex]) {
                    drawEntityNode(childIndex);
                }
                ImGui::TreePop();
            }
        };

        bool drewAnyNode = false;
        for (std::size_t i = 0; i < entities.size(); ++i) {
            const int parentIndex = entities[i].parentIndex;
            if (parentIndex < 0 || parentIndex >= static_cast<int>(entities.size())) {
                drawEntityNode(i);
                drewAnyNode = true;
            }
        }

        if (!drewAnyNode && !entities.empty() && !hasSearchFilter) {
            for (std::size_t i = 0; i < entities.size(); ++i) {
                drawEntityNode(i);
            }
        }

        if (entities.empty()) {
            ImGui::TextDisabled("No entities in this scene.");
        } else if (hasSearchFilter && !drewAnyNode) {
            ImGui::TextDisabled("No entities matched your search.");
        }

        if (ImGui::BeginPopupContextWindow("SceneHierarchyWindowContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Add Empty Entity")) {
                const std::string entityName = m_activeScene.makeUniqueEntityName("Entity");
                m_activeScene.addEntity(entityName);
                appendTerminalLine("Created " + entityName + ".");
            }
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size()) && ImGui::MenuItem("Create Child From Selection")) {
                pendingCreateChildEntity = static_cast<std::size_t>(m_selectedEntityIndex);
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        if (ImGui::Button("Add Empty Entity")) {
            const std::string entityName = m_activeScene.makeUniqueEntityName("Entity");
            m_activeScene.addEntity(entityName);
            appendTerminalLine("Created " + entityName + ".");
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            clearEntitySelection();
        }

        ImGui::SameLine();
        const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());
        if (!hasSelection) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("Delete Selected") && hasSelection) {
            pendingDeleteEntity = static_cast<std::size_t>(m_selectedEntityIndex);
        }

        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        const auto processDelete = [&](std::size_t entityIndex) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (entityIndex >= currentEntities.size()) {
                return;
            }

            const std::string removedEntityName = currentEntities[entityIndex].name;
            if (m_activeScene.removeEntity(removedEntityName)) {
                appendTerminalLine("Removed " + removedEntityName + ".");
            }

            if (m_selectedEntityIndex == static_cast<int>(entityIndex)) {
                clearEntitySelection();
                return;
            }

            if (m_selectedEntityIndex > static_cast<int>(entityIndex)) {
                --m_selectedEntityIndex;
            }

            const auto& updatedEntities = m_activeScene.getEntityData();
            if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(updatedEntities.size())) {
                setSelectedEntity(m_selectedEntityIndex);
            } else {
                clearEntitySelection();
            }
        };

        if (pendingClearParentByDrop.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingClearParentByDrop.value() < currentEntities.size()) {
                const std::string movedEntityName = currentEntities[pendingClearParentByDrop.value()].name;
                if (m_activeScene.setEntityParent(pendingClearParentByDrop.value(), std::nullopt)) {
                    appendTerminalLine("Moved " + movedEntityName + " to root.");
                }
            }
        }

        if (pendingReparentEntity.has_value()) {
            const std::size_t sourceEntityIndex = pendingReparentEntity->first;
            const std::size_t targetParentIndex = pendingReparentEntity->second;

            const auto& currentEntities = m_activeScene.getEntityData();
            if (sourceEntityIndex < currentEntities.size() && targetParentIndex < currentEntities.size()) {
                const std::string sourceName = currentEntities[sourceEntityIndex].name;
                const std::string parentName = currentEntities[targetParentIndex].name;

                if (m_activeScene.setEntityParent(sourceEntityIndex, targetParentIndex)) {
                    appendTerminalLine("Reparented " + sourceName + " under " + parentName + ".");
                } else {
                    appendTerminalLine("Unable to reparent " + sourceName + " under " + parentName + ".");
                }
            }
        }

        if (pendingCreateChildEntity.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingCreateChildEntity.value() < currentEntities.size()) {
                const std::string parentName = currentEntities[pendingCreateChildEntity.value()].name;
                const std::string childName = m_activeScene.makeUniqueEntityName(parentName + "_Child");
                m_activeScene.addEntity(childName);

                const std::optional<std::size_t> childIndex = m_activeScene.findEntityIndex(childName);
                if (childIndex.has_value() && m_activeScene.setEntityParent(childIndex.value(), pendingCreateChildEntity.value())) {
                    setSelectedEntity(static_cast<int>(childIndex.value()));
                    appendTerminalLine("Created child entity " + childName + " under " + parentName + ".");
                }
            }
        }

        if (pendingDuplicateEntity.has_value()) {
            const auto& currentEntities = m_activeScene.getEntityData();
            if (pendingDuplicateEntity.value() < currentEntities.size()) {
                const SceneEntity sourceEntity = currentEntities[pendingDuplicateEntity.value()];
                const std::string duplicatedName = m_activeScene.makeUniqueEntityName(sourceEntity.name + "_Copy");
                m_activeScene.addEntity(duplicatedName);

                const std::optional<std::size_t> duplicatedIndex = m_activeScene.findEntityIndex(duplicatedName);
                if (duplicatedIndex.has_value()) {
                    SceneEntity* duplicatedEntity = m_activeScene.getEntityByIndex(duplicatedIndex.value());
                    if (duplicatedEntity != nullptr) {
                        duplicatedEntity->transform = sourceEntity.transform;
                        duplicatedEntity->parentIndex = sourceEntity.parentIndex;
                    }

                    setSelectedEntity(static_cast<int>(duplicatedIndex.value()));
                    appendTerminalLine("Duplicated entity " + sourceEntity.name + " as " + duplicatedName + ".");
                }
            }
        }

        if (pendingDeleteEntity.has_value()) {
            processDelete(pendingDeleteEntity.value());
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered()) {
            clearEntitySelection();
        }

        ImGui::End();
    }

    void UILayer::drawSceneViewPanel(float deltaTime) {
        const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (!ImGui::Begin("Scene View", &m_showSceneViewPanel, windowFlags)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Renderer Output");
        ImGui::Separator();
        ImGui::Text("Frame dt: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("FPS: %.1f", deltaTime > 0.0f ? (1.0f / deltaTime) : 0.0f);
        ImGui::Text("Mode: %s", sceneStateToString(m_activeScene.getState()));

        const auto& entities = m_activeScene.getEntityData();
        const bool hasSelectedEntity = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());
        ImGui::Text("Selected Entity: %s", hasSelectedEntity ? entities[static_cast<std::size_t>(m_selectedEntityIndex)].name.c_str() : "None");

        if (ImGui::BeginCombo("Scene Selection", hasSelectedEntity ? entities[static_cast<std::size_t>(m_selectedEntityIndex)].name.c_str() : "None")) {
            if (ImGui::Selectable("None", !hasSelectedEntity)) {
                clearEntitySelection();
            }

            for (std::size_t i = 0; i < entities.size(); ++i) {
                const bool selected = static_cast<int>(i) == m_selectedEntityIndex;
                if (ImGui::Selectable(entities[i].name.c_str(), selected)) {
                    setSelectedEntity(static_cast<int>(i));
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if (ImGui::Button("Deselect##SceneViewSelection")) {
            clearEntitySelection();
        }

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

        const ImVec2 imageRectMin = ImGui::GetItemRectMin();
        const ImVec2 imageRectMax = ImGui::GetItemRectMax();
        const bool imageClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        m_sceneViewImageHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);

        if (imageClicked && !ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            const float imageRectWidth = imageRectMax.x - imageRectMin.x;
            const float imageRectHeight = imageRectMax.y - imageRectMin.y;
            if (imageRectWidth > 1.0f && imageRectHeight > 1.0f) {
                const ImVec2 mousePosition = ImGui::GetIO().MousePos;
                const float ndcX = ((mousePosition.x - imageRectMin.x) / imageRectWidth) * 2.0f - 1.0f;
                const float ndcY = 1.0f - ((mousePosition.y - imageRectMin.y) / imageRectHeight) * 2.0f;

                const glm::mat4 viewProjection = Renderer::getCameraProjectionMatrix() * Renderer::getCameraViewMatrix();
                const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);

                glm::vec4 nearPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                glm::vec4 farPoint = inverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

                if (std::abs(nearPoint.w) > 0.0001f && std::abs(farPoint.w) > 0.0001f) {
                    nearPoint /= nearPoint.w;
                    farPoint /= farPoint.w;

                    const glm::vec3 rayOrigin = glm::vec3(nearPoint);
                    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));

                    const auto& currentEntities = m_activeScene.getEntityData();
                    int pickedEntityIndex = -1;
                    float nearestDistance = std::numeric_limits<float>::max();

                    for (std::size_t entityIndex = 0; entityIndex < currentEntities.size(); ++entityIndex) {
                        const glm::mat4 worldTransform = composeEntityWorldTransformMatrix(currentEntities, entityIndex);
                        const glm::vec3 entityCenter = extractWorldPosition(worldTransform);

                        const SceneTransform& transform = currentEntities[entityIndex].transform;
                        const glm::vec3 extents(
                            std::max(0.05f, std::abs(transform.size.x * transform.scale.x)),
                            std::max(0.05f, std::abs(transform.size.y * transform.scale.y)),
                            std::max(0.05f, std::abs(transform.size.z * transform.scale.z))
                        );
                        const float sphereRadius = std::max(0.2f, 0.5f * std::max({extents.x, extents.y, extents.z}));

                        const std::optional<float> hitDistance = raySphereIntersectionDistance(rayOrigin, rayDirection, entityCenter, sphereRadius);
                        if (!hitDistance.has_value()) {
                            continue;
                        }

                        if (hitDistance.value() < nearestDistance) {
                            nearestDistance = hitDistance.value();
                            pickedEntityIndex = static_cast<int>(entityIndex);
                        }
                    }

                    if (pickedEntityIndex >= 0) {
                        setSelectedEntity(pickedEntityIndex);
                    } else {
                        clearEntitySelection();
                    }
                } else {
                    clearEntitySelection();
                }
            } else {
                clearEntitySelection();
            }
        }

        if (!m_runtimeEntityCameraActive) {
            updateSceneCameraController(m_sceneViewImageHovered);
        }

        ImGui::Spacing();
        if (m_runtimeEntityCameraActive) {
            ImGui::TextDisabled("Runtime camera entity drives view (Play mode).");
        } else {
            ImGui::TextDisabled("MMB: Orbit  |  Wheel: Zoom  |  Ctrl+RMB: Pan  |  RMB: Options");
        }

        ImGui::End();
    }

    void UILayer::drawInspectorPanel() {
        if (!ImGui::Begin("Inspector", &m_showInspectorPanel)) {
            ImGui::End();
            return;
        }

        const auto& entities = m_activeScene.getEntityData();
        const bool hasSelection = m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size());

        ImGui::Text("Scene Element Inspector");
        ImGui::Separator();
        ImGui::Text("Scene: %s", m_activeScene.getName().c_str());
        ImGui::Text("State: %s", sceneStateToString(m_activeScene.getState()));
        ImGui::Text("Entities: %d", static_cast<int>(entities.size()));
        ImGui::Text("Assets: %d", static_cast<int>(m_activeScene.getAssets().size()));

        if (!hasSelection) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("No selected entity.");
            ImGui::TextWrapped("Select an entity from Scene Hierarchy to edit transform, parent, and metadata.");
            ImGui::End();
            return;
        }

        SceneEntity* selectedEntity = m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
        if (selectedEntity == nullptr) {
            ImGui::End();
            return;
        }

        if (m_selectedEntityNameBufferEntityIndex != m_selectedEntityIndex) {
            std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
            std::snprintf(m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size(), "%s", selectedEntity->name.c_str());
            m_selectedEntityNameBufferEntityIndex = m_selectedEntityIndex;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Entity");

        ImGui::InputText("Name", m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size());
        ImGui::SameLine();
        if (ImGui::Button("Apply Name")) {
            std::string desiredName = std::string(m_selectedEntityNameBuffer.data());
            if (desiredName.empty()) {
                desiredName = "Entity";
            }

            if (desiredName != selectedEntity->name) {
                std::string appliedName = desiredName;
                if (m_activeScene.findEntityIndex(desiredName).has_value()) {
                    appliedName = m_activeScene.makeUniqueEntityName(desiredName);
                }

                const std::string oldName = selectedEntity->name;
                if (m_activeScene.renameEntity(oldName, appliedName)) {
                    appendTerminalLine("Renamed entity " + oldName + " to " + appliedName + ".");
                    m_activeScene.setGameStateValue("SelectedEntity", appliedName);
                    std::fill(m_selectedEntityNameBuffer.begin(), m_selectedEntityNameBuffer.end(), '\0');
                    std::snprintf(m_selectedEntityNameBuffer.data(), m_selectedEntityNameBuffer.size(), "%s", appliedName.c_str());
                }
            }
        }

        const auto& refreshedEntities = m_activeScene.getEntityData();
        if (m_selectedEntityIndex < 0 || m_selectedEntityIndex >= static_cast<int>(refreshedEntities.size())) {
            clearEntitySelection();
            ImGui::End();
            return;
        }

        selectedEntity = m_activeScene.getEntityByIndex(static_cast<std::size_t>(m_selectedEntityIndex));
        if (selectedEntity == nullptr) {
            ImGui::End();
            return;
        }

        int childCount = 0;
        for (const SceneEntity& entity : refreshedEntities) {
            if (entity.parentIndex == m_selectedEntityIndex) {
                ++childCount;
            }
        }

        int hierarchyDepth = 0;
        int currentParent = selectedEntity->parentIndex;
        while (currentParent >= 0 && currentParent < static_cast<int>(refreshedEntities.size())) {
            ++hierarchyDepth;
            currentParent = refreshedEntities[static_cast<std::size_t>(currentParent)].parentIndex;
        }

        ImGui::Text("Attributes");
        ImGui::BulletText("Entity ID: %d", m_selectedEntityIndex);
        ImGui::BulletText("Category: %s", getEntityCategoryToken(*selectedEntity));
        ImGui::BulletText("Hierarchy Depth: %d", hierarchyDepth);
        ImGui::BulletText("Child Count: %d", childCount);
        ImGui::BulletText("Root Entity: %s", selectedEntity->parentIndex < 0 ? "Yes" : "No");

        if (selectedEntity->parentIndex >= 0 && selectedEntity->parentIndex < static_cast<int>(refreshedEntities.size())) {
            ImGui::BulletText("Parent Entity: %s", refreshedEntities[static_cast<std::size_t>(selectedEntity->parentIndex)].name.c_str());
        } else {
            ImGui::BulletText("Parent Entity: None");
        }

        ImGui::Spacing();
        ImGui::Separator();

        const int currentParentIndex = selectedEntity->parentIndex;
        const char* parentPreview = "None";
        if (currentParentIndex >= 0 && currentParentIndex < static_cast<int>(refreshedEntities.size())) {
            parentPreview = refreshedEntities[static_cast<std::size_t>(currentParentIndex)].name.c_str();
        }

        if (ImGui::BeginCombo("Parent", parentPreview)) {
            const bool noParentSelected = currentParentIndex < 0;
            if (ImGui::Selectable("None", noParentSelected)) {
                if (m_activeScene.setEntityParent(static_cast<std::size_t>(m_selectedEntityIndex), std::nullopt)) {
                    appendTerminalLine("Parent cleared for " + selectedEntity->name + ".");
                }
            }

            for (std::size_t i = 0; i < refreshedEntities.size(); ++i) {
                if (static_cast<int>(i) == m_selectedEntityIndex) {
                    continue;
                }

                const bool isSelected = currentParentIndex == static_cast<int>(i);
                if (ImGui::Selectable(refreshedEntities[i].name.c_str(), isSelected)) {
                    if (m_activeScene.setEntityParent(static_cast<std::size_t>(m_selectedEntityIndex), i)) {
                        appendTerminalLine("Parent of " + selectedEntity->name + " set to " + refreshedEntities[i].name + ".");
                    } else {
                        appendTerminalLine("Parent assignment rejected for " + selectedEntity->name + " (cycle or invalid relationship).");
                    }
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Transform");

        bool transformChanged = false;
        transformChanged |= ImGui::DragFloat3("Position", &selectedEntity->transform.position.x, 0.05f);
        transformChanged |= ImGui::DragFloat3("Rotation", &selectedEntity->transform.rotation.x, 0.4f);
        transformChanged |= ImGui::DragFloat3("Scale", &selectedEntity->transform.scale.x, 0.02f, 0.01f, 500.0f);
        transformChanged |= ImGui::DragFloat3("Size", &selectedEntity->transform.size.x, 0.02f, 0.01f, 500.0f);

        selectedEntity->transform.scale.x = std::max(0.01f, selectedEntity->transform.scale.x);
        selectedEntity->transform.scale.y = std::max(0.01f, selectedEntity->transform.scale.y);
        selectedEntity->transform.scale.z = std::max(0.01f, selectedEntity->transform.scale.z);
        selectedEntity->transform.size.x = std::max(0.01f, selectedEntity->transform.size.x);
        selectedEntity->transform.size.y = std::max(0.01f, selectedEntity->transform.size.y);
        selectedEntity->transform.size.z = std::max(0.01f, selectedEntity->transform.size.z);

        if (transformChanged) {
            setSelectedEntity(m_selectedEntityIndex);
        }

        if (ImGui::Button("Reset Transform")) {
            selectedEntity->transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
            selectedEntity->transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
            selectedEntity->transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
            selectedEntity->transform.size = glm::vec3(1.0f, 1.0f, 1.0f);
            appendTerminalLine("Transform reset for " + selectedEntity->name + ".");
        }

        ImGui::End();
    }

    void UILayer::drawSettingsPanel() {
        if (!ImGui::Begin("Settings", &m_showSettingsPanel)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Engine Configuration");
        ImGui::Separator();
        drawEngineSettingsSection();

        ImGui::Spacing();
        ImGui::Separator();
        if (ImGui::Button("Close Settings")) {
            m_showSettingsPanel = false;
        }

        ImGui::End();
    }

    void UILayer::drawDebugPanel() {
        if (!ImGui::Begin("Debug", &m_showDebugPanel)) {
            ImGui::End();
            return;
        }

        const float frameMs = m_lastFrameDeltaTimeSeconds * 1000.0f;
        const float fps = m_lastFrameDeltaTimeSeconds > 0.0f ? (1.0f / m_lastFrameDeltaTimeSeconds) : 0.0f;
        const auto& entities = m_activeScene.getEntityData();

        ImGui::Text("Runtime");
        ImGui::Separator();
        ImGui::BulletText("Frame: %.2f ms", frameMs);
        ImGui::BulletText("FPS: %.1f", fps);
        ImGui::BulletText("Scene State: %s", sceneStateToString(m_activeScene.getState()));
        ImGui::BulletText("Entities: %d", static_cast<int>(entities.size()));
        ImGui::BulletText("Assets: %d", static_cast<int>(m_activeScene.getAssets().size()));
        ImGui::BulletText("Viewport: %dx%d", Renderer::getViewportWidth(), Renderer::getViewportHeight());
        ImGui::BulletText("Frame Texture ID: %u", Renderer::getFrameTextureID());

        ImGui::Spacing();
        ImGui::Text("Selection");
        ImGui::Separator();
        if (m_selectedEntityIndex >= 0 && m_selectedEntityIndex < static_cast<int>(entities.size())) {
            const SceneEntity& selectedEntity = entities[static_cast<std::size_t>(m_selectedEntityIndex)];
            ImGui::BulletText("Index: %d", m_selectedEntityIndex);
            ImGui::BulletText("Name: %s", selectedEntity.name.c_str());
            ImGui::BulletText("Parent Index: %d", selectedEntity.parentIndex);
            ImGui::BulletText("Position: %.2f %.2f %.2f", selectedEntity.transform.position.x, selectedEntity.transform.position.y, selectedEntity.transform.position.z);
        } else {
            ImGui::TextDisabled("No selected entity.");
        }

        ImGui::Spacing();
        ImGui::Text("AssetLoader");
        ImGui::Separator();
        ImGui::BulletText("Texture2D: %d", static_cast<int>(AssetLoader::getTexture2DNames().size()));
        ImGui::BulletText("Texture3D: %d", static_cast<int>(AssetLoader::getTexture3DNames().size()));
        ImGui::BulletText("Shaders: %d", static_cast<int>(AssetLoader::getShaderNames().size()));
        ImGui::BulletText("Compute Shaders: %d", static_cast<int>(AssetLoader::getComputeShaderNames().size()));
        ImGui::BulletText("Models: %d", static_cast<int>(AssetLoader::getModelNames().size()));
        ImGui::BulletText("Log Lines: %d", static_cast<int>(m_terminalLines.size()));

        ImGui::End();
    }

    void UILayer::drawBottomPanel() {
        if (!ImGui::Begin("Asset Browser", &m_showBottomPanel)) {
            ImGui::End();
            return;
        }

        ImGui::Text("Asset Viewer / Loader");
        ImGui::Separator();
        drawAssetsPanel();

        ImGui::End();
    }

    void UILayer::drawAssetsPanel() {
        constexpr float kFixedAssetIconSize = 72.0f;

        ImGui::Text("Runtime Import");
        if (ImGui::Button("Import Asset...")) {
            loadRuntimeAssetAuto();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::InputInt("3D Depth Hint", &m_runtimeTexture3DDepth, 1, 8)) {
            m_runtimeTexture3DDepth = std::max(1, m_runtimeTexture3DDepth);
        }
        ImGui::TextDisabled("Auto import routes texture/model/shader/compute files through AssetLoader.");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Asset Viewer");

        std::set<std::string> folderNames;
        folderNames.insert("All");
        const auto& assets = m_activeScene.getAssets();
        for (const SceneAsset& asset : assets) {
            folderNames.insert(extractAssetFolderName(asset));
        }

        if (ImGui::BeginChild("AssetFolderFilterStrip", ImVec2(0.0f, 38.0f), true, ImGuiWindowFlags_HorizontalScrollbar)) {
            for (const std::string& folderName : folderNames) {
                const bool isSelectedFolder = m_assetBrowserFolderFilter == folderName;
                if (isSelectedFolder) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.32f, 0.52f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.38f, 0.60f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.44f, 0.68f, 1.0f));
                }

                const std::string folderButtonLabel = "DIR " + folderName + "##asset_folder_" + folderName;
                if (ImGui::Button(folderButtonLabel.c_str())) {
                    m_assetBrowserFolderFilter = folderName;
                }

                if (isSelectedFolder) {
                    ImGui::PopStyleColor(3);
                }

                ImGui::SameLine();
            }
        }
        ImGui::EndChild();

        ImGui::Text("Icon Size: %.0f px (fixed)", kFixedAssetIconSize);
        ImGui::SameLine();
        ImGui::Text("Total: %d", static_cast<int>(assets.size()));

        std::vector<std::size_t> filteredAssetIndices;
        filteredAssetIndices.reserve(assets.size());
        for (std::size_t i = 0; i < assets.size(); ++i) {
            if (m_assetBrowserFolderFilter == "All" || extractAssetFolderName(assets[i]) == m_assetBrowserFolderFilter) {
                filteredAssetIndices.push_back(i);
            }
        }

        if (m_selectedAssetIndex >= static_cast<int>(assets.size())) {
            m_selectedAssetIndex = -1;
        }

        if (filteredAssetIndices.empty()) {
            ImGui::TextDisabled("No assets in selected folder.");
        } else {
            const float cellWidth = kFixedAssetIconSize + 30.0f;
            const int columns = std::max(1, static_cast<int>(std::max(1.0f, ImGui::GetContentRegionAvail().x) / cellWidth));

            if (ImGui::BeginTable("AssetIconGrid", columns, ImGuiTableFlags_SizingStretchSame)) {
                for (std::size_t assetIndex : filteredAssetIndices) {
                    ImGui::TableNextColumn();
                    ImGui::PushID(static_cast<int>(assetIndex));

                    const SceneAsset& asset = assets[assetIndex];
                    ImVec4 assetColor = getAssetIconColor(asset);
                    if (m_selectedAssetIndex == static_cast<int>(assetIndex)) {
                        assetColor = brightenColor(assetColor, 0.12f);
                    }

                    ImGui::PushStyleColor(ImGuiCol_Button, assetColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, brightenColor(assetColor, 0.08f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, brightenColor(assetColor, 0.14f));

                    const std::string iconLabel = std::string(getAssetIconToken(asset)) + "##asset_icon";
                    if (ImGui::Button(iconLabel.c_str(), ImVec2(kFixedAssetIconSize, kFixedAssetIconSize))) {
                        m_selectedAssetIndex = static_cast<int>(assetIndex);
                    }

                    const bool iconHovered = ImGui::IsItemHovered();
                    ImGui::PopStyleColor(3);

                    if (iconHovered) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", asset.name.c_str());
                        ImGui::TextDisabled("%s", asset.path.c_str());
                        ImGui::EndTooltip();
                    }

                    ImGui::TextWrapped("%s", asset.name.c_str());
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

        if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(assets.size())) {
            const SceneAsset& selectedAsset = assets[static_cast<std::size_t>(m_selectedAssetIndex)];
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Selected Asset");
            ImGui::BulletText("Name: %s", selectedAsset.name.c_str());
            ImGui::BulletText("Folder: %s", extractAssetFolderName(selectedAsset).c_str());
            ImGui::TextWrapped("Path: %s", selectedAsset.path.c_str());

            if (ImGui::Button("Remove From Scene Asset List")) {
                if (m_activeScene.removeAsset(selectedAsset.name)) {
                    appendTerminalLine("Removed scene asset entry: " + selectedAsset.name + ".");
                    m_selectedAssetIndex = -1;
                }
            }
        }
    }

    void UILayer::drawTerminalPanel() {
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
