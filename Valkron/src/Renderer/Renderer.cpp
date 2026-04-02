#include "Renderer/Renderer.hpp"
#include "Renderer/Camera.hpp"
#include "Core/Log.hpp"
#include "Engine/AssetLoader.hpp"
#include "Renderer/Buffers.hpp"
#include "Renderer/Model.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/Texture.hpp"

#define GLFW_INCLUDE_NONE
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/gl.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/trigonometric.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace Valkron {

    struct RendererData {
        std::unique_ptr<Camera> camera;

        std::unique_ptr<FrameBuffer> frameBuffer;
        std::unique_ptr<Texture> frameTexture;
        std::unique_ptr<DepthBuffer> depthBuffer;

        glm::mat4 modelMatrix{1.0f};
        std::vector<SceneModelInstance> sceneModelInstances;
        std::vector<glm::mat4> sceneEntityTransforms;
        std::vector<glm::vec3> lightEntityPositions;
        int selectedEntityRenderIndex = -1;

        int viewportWidth = 0;
        int viewportHeight = 0;
        int windowFramebufferWidth = 0;
        int windowFramebufferHeight = 0;
        GLFWwindow* window = nullptr;
        bool initialized = false;
    };

    static RendererData s_data;

    static void applyRetroImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 0.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 1.0f;
        style.PopupRounding = 2.0f;
        style.ScrollbarRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.WindowPadding = ImVec2(8.0f, 6.0f);
        style.FramePadding = ImVec2(6.0f, 3.0f);
        style.ItemSpacing = ImVec2(6.0f, 5.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
        style.IndentSpacing = 18.0f;
        style.AntiAliasedLines = true;
        style.AntiAliasedFill = true;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.86f, 0.87f, 0.89f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.53f, 0.56f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.15f, 0.17f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.45f, 0.15f, 0.15f, 0.85f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.21f, 0.23f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.24f, 0.27f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.11f, 0.12f, 0.95f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.12f, 0.13f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.33f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.37f, 0.38f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.47f, 0.50f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.62f, 0.22f, 0.22f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.82f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.17f, 0.18f, 0.20f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.24f, 0.27f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.18f, 0.19f, 0.21f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.24f, 0.27f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.19f, 0.19f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.66f, 0.17f, 0.17f, 0.90f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.45f, 0.52f, 0.66f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.55f, 0.64f, 0.80f, 0.72f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.62f, 0.72f, 0.90f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.45f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.14f, 0.15f, 0.16f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.30f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.74f, 0.80f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.86f, 0.61f, 0.27f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.62f, 0.22f, 0.22f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.78f, 0.18f, 0.18f, 0.95f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.78f, 0.18f, 0.18f, 1.00f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.64f, 0.16f, 0.16f, 0.72f);
    }

    static void renderLoadedAssets() {
        if (s_data.camera == nullptr || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        const float aspectRatio = static_cast<float>(s_data.viewportWidth) / static_cast<float>(s_data.viewportHeight);
        const glm::mat4 viewMatrix = s_data.camera->getViewMatrix();
        const glm::mat4 projectionMatrix = s_data.camera->getProjectionMatrix(aspectRatio);

        const std::vector<SceneModelInstance>& modelInstances = s_data.sceneModelInstances;

        if (modelInstances.empty()) {
            return;
        }

        constexpr int kMaxSceneLights = 8;
        std::vector<glm::vec3> activeLightPositions;
        if (!s_data.lightEntityPositions.empty()) {
            activeLightPositions = s_data.lightEntityPositions;
        } else {
            activeLightPositions.push_back(glm::vec3(3.0f, 4.0f, 3.0f));
        }

        if (activeLightPositions.size() > static_cast<std::size_t>(kMaxSceneLights)) {
            activeLightPositions.resize(static_cast<std::size_t>(kMaxSceneLights));
        }

        const float ambientStrength = std::clamp(0.05f + static_cast<float>(activeLightPositions.size()) * 0.02f, 0.05f, 0.20f);
        const glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
        const glm::vec3 ambientColor(ambientStrength, ambientStrength, ambientStrength + 0.01f);
        const glm::vec3 cameraPosition = s_data.camera->getPosition();
        const glm::vec3 selectedHighlightColor(0.98f, 0.78f, 0.24f);

        for (const SceneModelInstance& instance : modelInstances) {
            if (instance.modelName.empty()) {
                continue;
            }

            std::shared_ptr<Model> model = AssetLoader::getModel(instance.modelName);
            if (model == nullptr || !model->isLoaded()) {
                continue;
            }

            const std::string shaderName = AssetLoader::getModelShader(instance.modelName);
            std::shared_ptr<Shader> shader = AssetLoader::getShader(shaderName);
            if (shader == nullptr) {
                continue;
            }

            shader->bind();
            shader->setMat4("u_View", glm::value_ptr(viewMatrix));
            shader->setMat4("u_Projection", glm::value_ptr(projectionMatrix));
            shader->setInt("u_LightCount", static_cast<int>(activeLightPositions.size()));
            for (std::size_t lightIndex = 0; lightIndex < activeLightPositions.size(); ++lightIndex) {
                const std::string prefix = "u_Lights[" + std::to_string(lightIndex) + "]";
                shader->setVec3(prefix + ".position", &activeLightPositions[lightIndex].x);
                shader->setVec3(prefix + ".color", &lightColor.x);
                shader->setVec3(prefix + ".ambient", &ambientColor.x);
            }
            shader->setVec3("u_ViewPos", &cameraPosition.x);

            shader->setMat4("u_Model", glm::value_ptr(instance.transform));
            shader->setVec3("u_SelectionColor", &selectedHighlightColor.x);
            shader->setFloat("u_SelectionMix", instance.selected ? 0.38f : 0.0f);
            model->draw(*shader);
        }
    }

    static void rebuildFrameBufferAttachments() {
        VALKRON_CORE_ASSERT(s_data.frameTexture != nullptr, "Frame texture must exist before rebuilding attachments");
        VALKRON_CORE_ASSERT(s_data.depthBuffer != nullptr, "Depth buffer must exist before rebuilding attachments");

        if (!s_data.frameBuffer || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        s_data.frameTexture->createEmpty(s_data.viewportWidth, s_data.viewportHeight, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
        s_data.depthBuffer->allocateStorage(s_data.viewportWidth, s_data.viewportHeight);

        s_data.frameBuffer->bind();
        s_data.frameBuffer->attachColorTexture(s_data.frameTexture->getID());
        s_data.frameBuffer->attachDepthBuffer(s_data.depthBuffer->getID());
        if (!s_data.frameBuffer->isComplete()) {
            LOG_ERROR("FrameBuffer is not complete after resize/update");
            VALKRON_CORE_ASSERT(false, "Framebuffer is incomplete after attachment rebuild");
        }
        s_data.frameBuffer->unbind();
    }

    void Renderer::init(GLFWwindow* window) {
        if (s_data.initialized) {
            LOG_WARN("Renderer::init called more than once. Ignoring duplicate initialization.");
            return;
        }

        VALKRON_CORE_ASSERT(window != nullptr, "Renderer::init requires a valid GLFW window");

        RenderCommand::init();
        s_data.window = window;

        s_data.camera = std::make_unique<Camera>(CameraType::Perspective);
        s_data.modelMatrix = glm::mat4(1.0f);
        s_data.sceneModelInstances.clear();
        s_data.sceneEntityTransforms.clear();
        s_data.lightEntityPositions.clear();
        s_data.selectedEntityRenderIndex = -1;

        s_data.frameBuffer = std::make_unique<FrameBuffer>();
        s_data.frameTexture = std::make_unique<Texture>();
        s_data.depthBuffer = std::make_unique<DepthBuffer>();

        VALKRON_CORE_ASSERT(s_data.frameBuffer != nullptr, "Failed to create FrameBuffer");
        VALKRON_CORE_ASSERT(s_data.frameTexture != nullptr, "Failed to create frame texture");
        VALKRON_CORE_ASSERT(s_data.depthBuffer != nullptr, "Failed to create depth buffer");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        applyRetroImGuiStyle();

        const bool glfwInitOk = ImGui_ImplGlfw_InitForOpenGL(s_data.window, true);
        VALKRON_CORE_ASSERT(glfwInitOk, "Failed to initialize ImGui GLFW backend");

        const bool openglInitOk = ImGui_ImplOpenGL3_Init("#version 460 core");
        VALKRON_CORE_ASSERT(openglInitOk, "Failed to initialize ImGui OpenGL3 backend");

        AssetLoader::initialize();

        s_data.initialized = true;
    }

    void Renderer::shutdown() {
        if (s_data.initialized) {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
        }

        s_data.depthBuffer.reset();
        s_data.frameTexture.reset();
        s_data.frameBuffer.reset();
        s_data.camera.reset();
        s_data.modelMatrix = glm::mat4(1.0f);
        s_data.sceneModelInstances.clear();
        s_data.sceneEntityTransforms.clear();
        s_data.lightEntityPositions.clear();
        s_data.selectedEntityRenderIndex = -1;
        s_data.viewportWidth = 0;
        s_data.viewportHeight = 0;
        s_data.windowFramebufferWidth = 0;
        s_data.windowFramebufferHeight = 0;
        s_data.window = nullptr;
        s_data.initialized = false;
    }

    void Renderer::beginFrame() {
        if (!s_data.initialized || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            if (!s_data.initialized) {
                LOG_ERROR("Renderer::beginFrame called before Renderer::init");
            }
            return;
        }

        VALKRON_CORE_ASSERT(s_data.frameBuffer != nullptr, "FrameBuffer is not initialized");

        s_data.frameBuffer->bind();
        RenderCommand::setViewport(0, 0, s_data.viewportWidth, s_data.viewportHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.13f, 0.14f, 0.19f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderLoadedAssets();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void Renderer::endFrame() {
        if (!s_data.initialized || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        VALKRON_CORE_ASSERT(s_data.frameBuffer != nullptr, "FrameBuffer is not initialized");

        s_data.frameBuffer->unbind();
        const int destinationWidth = s_data.windowFramebufferWidth > 0 ? s_data.windowFramebufferWidth : s_data.viewportWidth;
        const int destinationHeight = s_data.windowFramebufferHeight > 0 ? s_data.windowFramebufferHeight : s_data.viewportHeight;
        s_data.frameBuffer->blitToDefault(
            s_data.viewportWidth,
            s_data.viewportHeight,
            destinationWidth,
            destinationHeight,
            GL_COLOR_BUFFER_BIT
        );

        ImGui::Render();
        RenderCommand::setViewport(0, 0, destinationWidth, destinationHeight);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void Renderer::setCameraType(CameraType type) {
        if (!s_data.initialized || s_data.camera == nullptr) {
            LOG_WARN("Renderer::setCameraType called before camera initialization");
            return;
        }

        s_data.camera->setType(type);
    }

    void Renderer::setCameraLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return;
        }

        s_data.camera->setPosition(position);
        s_data.camera->setTarget(target);
        s_data.camera->setUp(up);
    }

    void Renderer::setModelTransform(const glm::vec3& position, const glm::vec3& rotationDegrees, const glm::vec3& scale) {
        if (!s_data.initialized) {
            return;
        }

        glm::mat4 model(1.0f);
        model = glm::translate(model, position);
        model = glm::rotate(model, glm::radians(rotationDegrees.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotationDegrees.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotationDegrees.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, glm::vec3(
            std::max(0.01f, scale.x),
            std::max(0.01f, scale.y),
            std::max(0.01f, scale.z)
        ));

        s_data.modelMatrix = model;
    }

    void Renderer::setSceneModelInstances(const std::vector<SceneModelInstance>& modelInstances) {
        if (!s_data.initialized) {
            return;
        }

        s_data.sceneModelInstances = modelInstances;
    }

    void Renderer::setSceneEntityTransforms(const std::vector<glm::mat4>& entityTransforms, int selectedEntityIndex) {
        if (!s_data.initialized) {
            return;
        }

        s_data.sceneEntityTransforms = entityTransforms;
        s_data.selectedEntityRenderIndex = selectedEntityIndex;
    }

    void Renderer::setLightEntityPositions(const std::vector<glm::vec3>& lightPositions) {
        if (!s_data.initialized) {
            return;
        }

        s_data.lightEntityPositions = lightPositions;
    }

    glm::vec3 Renderer::getCameraPosition() {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return glm::vec3(0.0f, 0.0f, 2.0f);
        }

        return s_data.camera->getPosition();
    }

    glm::vec3 Renderer::getCameraTarget() {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return glm::vec3(0.0f, 0.0f, 0.0f);
        }

        return s_data.camera->getTarget();
    }

    glm::vec3 Renderer::getCameraUp() {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return glm::vec3(0.0f, 1.0f, 0.0f);
        }

        return s_data.camera->getUp();
    }

    glm::mat4 Renderer::getCameraViewMatrix() {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return glm::mat4(1.0f);
        }

        return s_data.camera->getViewMatrix();
    }

    glm::mat4 Renderer::getCameraProjectionMatrix() {
        if (!s_data.initialized || s_data.camera == nullptr) {
            return glm::mat4(1.0f);
        }

        const float aspectRatio = s_data.viewportHeight > 0
            ? static_cast<float>(s_data.viewportWidth) / static_cast<float>(s_data.viewportHeight)
            : 1.0f;
        return s_data.camera->getProjectionMatrix(aspectRatio);
    }

    void Renderer::onWindowResize(int width, int height) {
        if (width <= 0 || height <= 0) {
            LOG_WARN("Ignoring window resize with non-positive size: " + std::to_string(width) + "x" + std::to_string(height));
            return;
        }

        s_data.windowFramebufferWidth = width;
        s_data.windowFramebufferHeight = height;

        if (s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            s_data.viewportWidth = width;
            s_data.viewportHeight = height;
            rebuildFrameBufferAttachments();
        }

        RenderCommand::setViewport(0, 0, width, height);
    }

    void Renderer::setViewportSize(int width, int height) {
        if (!s_data.initialized) {
            return;
        }

        if (width <= 0 || height <= 0) {
            return;
        }

        if (s_data.viewportWidth == width && s_data.viewportHeight == height) {
            return;
        }

        s_data.viewportWidth = width;
        s_data.viewportHeight = height;
        rebuildFrameBufferAttachments();
    }

    unsigned int Renderer::getFrameTextureID() {
        if (!s_data.initialized || s_data.frameTexture == nullptr) {
            return 0;
        }

        return s_data.frameTexture->getID();
    }

    int Renderer::getViewportWidth() {
        return s_data.viewportWidth;
    }

    int Renderer::getViewportHeight() {
        return s_data.viewportHeight;
    }

}