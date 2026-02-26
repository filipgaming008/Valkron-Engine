#pragma once

#include "Core.hpp"
#include "Log.hpp"
#include "GLFW/glfw3.h"

namespace Valkron {


    class VALKRON_API Window {
        private:
            GLFWwindow* window = nullptr;
        public:
            Window();
            virtual ~Window();

            inline bool shouldClose() const {
                return window && glfwWindowShouldClose(window) != 0;
            }

            void Update();
            inline GLFWwindow* getWindow() const { return window; }
};


}