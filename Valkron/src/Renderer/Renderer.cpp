#include "Renderer/Renderer.hpp"
#include "Renderer/Camera.hpp"
#include "Core/Log.hpp"
#include "Renderer/Buffers.hpp"
#include "Renderer/RenderCommand.hpp"
#include "Renderer/Shader.hpp"
#include "Renderer/Texture.hpp"

#include "glad/gl.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include <memory>

namespace Valkron {

    namespace {
        struct RendererData {
            std::unique_ptr<VertexArray> vertexArray;
            std::unique_ptr<VertexBuffer> vertexBuffer;
            std::unique_ptr<IndexBuffer> indexBuffer;
            std::unique_ptr<Shader> shader;
            std::unique_ptr<Texture> texture;
            std::unique_ptr<Camera> camera;

            std::unique_ptr<FrameBuffer> frameBuffer;
            std::unique_ptr<Texture> frameTexture;
            std::unique_ptr<DepthBuffer> depthBuffer;

            int viewportWidth = 0;
            int viewportHeight = 0;
            bool initialized = false;
        };

        RendererData s_data;

        void rebuildFrameBufferAttachments() {
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
            }
            s_data.frameBuffer->unbind();
        }
    }

    void Renderer::init() {
        if (s_data.initialized) {
            return;
        }

        RenderCommand::init();

        const float vertices[] = {
              // position          // color
              -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
               0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
               0.0f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f
        };

        const unsigned int indices[] = {
            0, 1, 2
        };

        VertexLayout layout;
        layout.push<float>(3);
        layout.push<float>(3);

        s_data.vertexArray = std::make_unique<VertexArray>();
        s_data.vertexBuffer = std::make_unique<VertexBuffer>(vertices, sizeof(vertices));
        s_data.indexBuffer = std::make_unique<IndexBuffer>(indices, 3);
        s_data.vertexArray->addBuffer(*s_data.vertexBuffer, layout);

        s_data.shader = std::make_unique<Shader>("shaders/textured.vert", "shaders/textured.frag");

        s_data.texture = std::make_unique<Texture>();
        if (!s_data.texture->loadTexture("textures/checker.ppm")) {
            LOG_WARN("Using fallback 1x1 texture because file loading failed");
            const unsigned char fallbackWhite[] = {255, 255, 255, 255};
            s_data.texture->createEmpty(1, 1, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
            s_data.texture->bind();
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, fallbackWhite);
        }

        s_data.camera = std::make_unique<Camera>();

        s_data.frameBuffer = std::make_unique<FrameBuffer>();
        s_data.frameTexture = std::make_unique<Texture>();
        s_data.depthBuffer = std::make_unique<DepthBuffer>();

        s_data.initialized = true;
    }

    void Renderer::shutdown() {
        s_data.depthBuffer.reset();
        s_data.frameTexture.reset();
        s_data.frameBuffer.reset();
        s_data.texture.reset();
        s_data.shader.reset();
        s_data.camera.reset();
        s_data.indexBuffer.reset();
        s_data.vertexBuffer.reset();
        s_data.vertexArray.reset();
        s_data.viewportWidth = 0;
        s_data.viewportHeight = 0;
        s_data.initialized = false;
    }

    void Renderer::beginFrame() {
        if (!s_data.initialized || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        s_data.frameBuffer->bind();
        RenderCommand::setViewport(0, 0, s_data.viewportWidth, s_data.viewportHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        s_data.shader->bind();

        const float aspect = static_cast<float>(s_data.viewportWidth) / static_cast<float>(s_data.viewportHeight);
        const glm::mat4 model = glm::mat4(1.0f);
        const glm::mat4 view = s_data.camera->getViewMatrix();
        const glm::mat4 projection = s_data.camera->getProjectionMatrix(aspect);

        s_data.shader->setMat4("u_Model", glm::value_ptr(model));
        s_data.shader->setMat4("u_View", glm::value_ptr(view));
        s_data.shader->setMat4("u_Projection", glm::value_ptr(projection));

        s_data.vertexArray->bind();
        s_data.indexBuffer->bind();
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(s_data.indexBuffer->getCount()), GL_UNSIGNED_INT, nullptr);
    }

    void Renderer::endFrame() {
        if (!s_data.initialized || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            return;
        }

        s_data.frameBuffer->unbind();
        s_data.frameBuffer->blitToDefault(
            s_data.viewportWidth,
            s_data.viewportHeight,
            s_data.viewportWidth,
            s_data.viewportHeight,
            GL_COLOR_BUFFER_BIT
        );
    }

    void Renderer::onWindowResize(int width, int height) {
        s_data.viewportWidth = width;
        s_data.viewportHeight = height;
        RenderCommand::setViewport(0, 0, width, height);
        rebuildFrameBufferAttachments();
    }

}