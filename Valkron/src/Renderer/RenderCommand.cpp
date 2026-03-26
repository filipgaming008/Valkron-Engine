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

    void RenderCommand::clear() {
        VALKRON_CORE_ASSERT(s_rendererAPI != nullptr, "Renderer API backend is not initialized");
        s_rendererAPI->clear();
    }

}