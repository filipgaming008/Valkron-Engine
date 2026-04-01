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

#include "glm/gtc/type_ptr.hpp"

#include <memory>

namespace Valkron {

    struct RendererData {
        std::unique_ptr<Camera> camera;

        std::unique_ptr<FrameBuffer> frameBuffer;
        std::unique_ptr<Texture> frameTexture;
        std::unique_ptr<DepthBuffer> depthBuffer;

        int viewportWidth = 0;
        int viewportHeight = 0;
        int windowFramebufferWidth = 0;
        int windowFramebufferHeight = 0;
        GLFWwindow* window = nullptr;
        bool initialized = false;
    };

    static RendererData s_data;

    static void applyModernImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 10.0f;
        style.ChildRounding = 8.0f;
        style.FrameRounding = 7.0f;
        style.PopupRounding = 8.0f;
        style.ScrollbarRounding = 9.0f;
        style.GrabRounding = 7.0f;
        style.TabRounding = 8.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.TabBorderSize = 0.0f;
        style.WindowPadding = ImVec2(12.0f, 10.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(9.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.IndentSpacing = 18.0f;

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.90f, 0.94f, 0.97f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.52f, 0.58f, 0.64f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.10f, 0.12f, 0.97f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.12f, 0.14f, 0.85f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.11f, 0.14f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.21f, 0.27f, 0.32f, 0.85f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.16f, 0.20f, 0.95f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.17f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.30f, 0.37f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.18f, 0.23f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.10f, 0.12f, 0.80f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.36f, 0.44f, 0.85f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.29f, 0.48f, 0.58f, 0.95f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.57f, 0.69f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.78f, 0.92f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.70f, 0.83f, 0.90f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.42f, 0.84f, 0.98f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.16f, 0.28f, 0.35f, 0.85f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.41f, 0.50f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.26f, 0.49f, 0.59f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.15f, 0.30f, 0.37f, 0.85f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.24f, 0.46f, 0.55f, 0.95f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.28f, 0.55f, 0.66f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.30f, 0.37f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.24f, 0.45f, 0.54f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.35f, 0.63f, 0.75f, 0.78f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.75f, 0.89f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.20f, 0.26f, 0.92f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.39f, 0.48f, 0.95f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.35f, 0.43f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.16f, 0.20f, 0.95f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.23f, 0.29f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.68f, 0.78f, 0.90f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.89f, 0.61f, 0.34f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.18f, 0.45f, 0.55f, 0.45f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.95f, 0.75f, 0.26f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.37f, 0.74f, 0.89f, 1.00f);
    }

    static void renderLoadedAssets() {
        if (s_data.camera == nullptr || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        const std::string activeModelName = AssetLoader::getActiveModel();
        if (activeModelName.empty()) {
            return;
        }

        std::shared_ptr<Model> model = AssetLoader::getModel(activeModelName);
        if (model == nullptr || !model->isLoaded()) {
            return;
        }

        const std::string shaderName = AssetLoader::getModelShader(activeModelName);
        std::shared_ptr<Shader> shader = AssetLoader::getShader(shaderName);
        if (shader == nullptr) {
            return;
        }

        const float aspectRatio = static_cast<float>(s_data.viewportWidth) / static_cast<float>(s_data.viewportHeight);
        const glm::mat4 modelMatrix(1.0f);
        const glm::mat4 viewMatrix = s_data.camera->getViewMatrix();
        const glm::mat4 projectionMatrix = s_data.camera->getProjectionMatrix(aspectRatio);

        const glm::vec3 lightPosition(3.0f, 4.0f, 3.0f);
        const glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
        const glm::vec3 ambientLight(0.15f, 0.15f, 0.18f);
        const glm::vec3 cameraPosition = s_data.camera->getPosition();

        shader->bind();
        shader->setMat4("u_Model", glm::value_ptr(modelMatrix));
        shader->setMat4("u_View", glm::value_ptr(viewMatrix));
        shader->setMat4("u_Projection", glm::value_ptr(projectionMatrix));
        shader->setVec3("u_Light.position", &lightPosition.x);
        shader->setVec3("u_Light.color", &lightColor.x);
        shader->setVec3("u_Light.ambient", &ambientLight.x);
        shader->setVec3("u_ViewPos", &cameraPosition.x);

        model->draw(*shader);
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

        s_data.frameBuffer = std::make_unique<FrameBuffer>();
        s_data.frameTexture = std::make_unique<Texture>();
        s_data.depthBuffer = std::make_unique<DepthBuffer>();

        VALKRON_CORE_ASSERT(s_data.frameBuffer != nullptr, "Failed to create FrameBuffer");
        VALKRON_CORE_ASSERT(s_data.frameTexture != nullptr, "Failed to create frame texture");
        VALKRON_CORE_ASSERT(s_data.depthBuffer != nullptr, "Failed to create depth buffer");

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        applyModernImGuiStyle();

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
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
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