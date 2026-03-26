#pragma once

#include "Core/Core.hpp"

namespace Valkron {

    class VALKRON_API Renderer {
        public:
            static void init();
            static void shutdown();
            static void beginFrame();
            static void endFrame();
            static void onWindowResize(int width, int height);
    };

}