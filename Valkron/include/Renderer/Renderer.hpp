#pragma once

#include "Core/Core.hpp"

#include <functional>
#include <vector>

namespace Valkron {

    class VALKRON_API Renderer {
        public:
            using RenderCommandFn = std::function<void()>;

            static void init();
            static void shutdown();
            static void beginFrame();
            static void endFrame();
            static void onWindowResize(int width, int height);

            static void submit(const RenderCommandFn& renderCommand);
            static void clearQueue();
            static void renderQueue();
    };

}