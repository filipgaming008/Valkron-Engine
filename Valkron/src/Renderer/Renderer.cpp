#include "Renderer/Renderer.hpp"
#include "Renderer/Camera.hpp"
#include "Core/Log.hpp"
#include "Renderer/Buffers.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Renderer/Texture.hpp"

#define GLFW_INCLUDE_NONE
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/gl.h"

#include <memory>

namespace Valkron {

    struct RendererData {
        std::unique_ptr<Camera> camera;

        std::unique_ptr<FrameBuffer> frameBuffer;
        std::unique_ptr<Texture> frameTexture;
        std::unique_ptr<DepthBuffer> depthBuffer;

        int viewportWidth = 0;
        int viewportHeight = 0;
        GLFWwindow* window = nullptr;
        bool initialized = false;
    };

    static RendererData s_data;

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
        ImGui::StyleColorsDark();

        const bool glfwInitOk = ImGui_ImplGlfw_InitForOpenGL(s_data.window, true);
        VALKRON_CORE_ASSERT(glfwInitOk, "Failed to initialize ImGui GLFW backend");

        const bool openglInitOk = ImGui_ImplOpenGL3_Init("#version 460 core");
        VALKRON_CORE_ASSERT(openglInitOk, "Failed to initialize ImGui OpenGL3 backend");

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
        s_data.frameBuffer->blitToDefault(
            s_data.viewportWidth,
            s_data.viewportHeight,
            s_data.viewportWidth,
            s_data.viewportHeight,
            GL_COLOR_BUFFER_BIT
        );

        ImGui::Render();
        RenderCommand::setViewport(0, 0, s_data.viewportWidth, s_data.viewportHeight);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void Renderer::setCameraType(CameraType type) {
        if (!s_data.initialized || s_data.camera == nullptr) {
            LOG_WARN("Renderer::setCameraType called before camera initialization");
            return;
        }

        s_data.camera->setType(type);
    }

    void Renderer::onWindowResize(int width, int height) {
        if (width <= 0 || height <= 0) {
            LOG_WARN("Ignoring window resize with non-positive size: " + std::to_string(width) + "x" + std::to_string(height));
            return;
        }

        s_data.viewportWidth = width;
        s_data.viewportHeight = height;
        RenderCommand::setViewport(0, 0, width, height);
        rebuildFrameBufferAttachments();
    }

}