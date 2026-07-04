#include "Renderer/Renderer.hpp"
#include "Core/Log.hpp"
#include "Renderer/RenderCommand.hpp"

namespace Valkron {

    namespace {
        struct RendererData {
            int viewportWidth = 0;
            int viewportHeight = 0;
            bool initialized = false;
            std::vector<Renderer::RenderCommandFn> renderQueue;
        };

        RendererData s_data;
    }

    void Renderer::init() {
        if (s_data.initialized) {
            LOG_WARN("Renderer::init called more than once. Ignoring duplicate initialization.");
            return;
        }

        RenderCommand::init();
        s_data.initialized = true;
    }

    void Renderer::shutdown() {
        clearQueue();
        s_data.viewportWidth = 0;
        s_data.viewportHeight = 0;
        s_data.initialized = false;
    }

    void Renderer::beginFrame() {
        if (!s_data.initialized || s_data.viewportWidth <= 0 || s_data.viewportHeight <= 0) {
            if (!s_data.initialized) {
                LOG_ERROR("Renderer::beginFrame called before Renderer::init");
            }
            return;
        }

        RenderCommand::setViewport(0, 0, s_data.viewportWidth, s_data.viewportHeight);
        RenderCommand::setDepthTest(true);
        RenderCommand::setClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        RenderCommand::clear();
    }

    void Renderer::endFrame() {
        renderQueue();
    }

    void Renderer::onWindowResize(int width, int height) {
        if (width <= 0 || height <= 0) {
            LOG_WARN("Ignoring window resize with non-positive size: " + std::to_string(width) + "x" + std::to_string(height));
            return;
        }

        s_data.viewportWidth = width;
        s_data.viewportHeight = height;
        RenderCommand::setViewport(0, 0, width, height);
    }

    void Renderer::submit(const RenderCommandFn& renderCommand) {
        if (renderCommand) {
            s_data.renderQueue.push_back(renderCommand);
        }
    }

    void Renderer::clearQueue() {
        s_data.renderQueue.clear();
    }

    void Renderer::renderQueue() {
        for (const auto& renderCommand : s_data.renderQueue) {
            if (renderCommand) {
                renderCommand();
            }
        }
        s_data.renderQueue.clear();
    }

}