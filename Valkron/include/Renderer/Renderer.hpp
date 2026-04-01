#pragma once

#include "Core/Core.hpp"
#include "Renderer/Camera.hpp"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace Valkron {

    using GLFWwindow = ::GLFWwindow;

    class VALKRON_API Renderer {
        public:
            static void init(GLFWwindow* window);
            static void shutdown();
            static void beginFrame();
            static void endFrame();
            static void setCameraType(CameraType type);
            static void onWindowResize(int width, int height);
    };

}