#include "Renderer/RenderCommand.hpp"
#include "Renderer/API/OpenGLRendererAPI.hpp"

namespace Valkron {

    std::unique_ptr<RendererAPI> RenderCommand::s_rendererAPI = std::make_unique<OpenGLRendererAPI>();

    void RenderCommand::init() {
        s_rendererAPI->init();
    }

    void RenderCommand::setViewport(int x, int y, int width, int height) {
        s_rendererAPI->setViewport(x, y, width, height);
    }

    void RenderCommand::setClearColor(float red, float green, float blue, float alpha) {
        s_rendererAPI->setClearColor(red, green, blue, alpha);
    }

    void RenderCommand::clear() {
        s_rendererAPI->clear();
    }

}