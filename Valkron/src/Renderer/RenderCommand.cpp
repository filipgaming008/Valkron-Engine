#include "Renderer/RenderCommand.hpp"
#include "Renderer/API/OpenGLRendererAPI.hpp"
#include "Core/Log.hpp"

namespace Valkron {

    std::unique_ptr<RendererAPI> RenderCommand::s_rendererAPI = std::make_unique<OpenGLRendererAPI>();

    void RenderCommand::init() {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->init();
    }

    void RenderCommand::setViewport(int x, int y, int width, int height) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setViewport(x, y, width, height);
    }

    void RenderCommand::setClearColor(float red, float green, float blue, float alpha) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setClearColor(red, green, blue, alpha);
    }

    void RenderCommand::clear(bool clearColor, bool clearDepth) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->clear(clearColor, clearDepth);
    }

    void RenderCommand::setDepthTest(bool enabled) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setDepthTest(enabled);
    }

    void RenderCommand::useShader(GLuint shaderID) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->useShader(shaderID);
    }

    void RenderCommand::bindVertexArray(GLuint vaoID) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->bindVertexArray(vaoID);
    }

    void RenderCommand::bindTexture(GLuint textureID, GLuint slot) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->bindTexture(textureID, slot);
    }

    void RenderCommand::drawIndexed(GLuint indexBufferID, GLuint indexCount) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->drawIndexed(indexBufferID, indexCount);
    }

    void RenderCommand::setUniformMat4(GLuint shaderID, const std::string& name, const glm::mat4& matrix) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setUniformMat4(shaderID, name, matrix);
    }

    void RenderCommand::setUniformVec4(GLuint shaderID, const std::string& name, const glm::vec4& vector) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setUniformVec4(shaderID, name, vector);
    }

    void RenderCommand::setUniformFloat(GLuint shaderID, const std::string& name, float value) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setUniformFloat(shaderID, name, value);
    }

    void RenderCommand::setUniformInt(GLuint shaderID, const std::string& name, int value) {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->setUniformInt(shaderID, name, value);
    }

}