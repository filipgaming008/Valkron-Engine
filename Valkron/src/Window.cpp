#include "Window.hpp"

namespace Valkron {

    Window::Window() {
        if (!glfwInit()) {
            VALKRON_CORE_ASSERT(false, "Failed to initialize GLFW");
            return;
        }

        window = glfwCreateWindow(800, 600, "Valkron Engine", nullptr, nullptr);

        if (!window) {
            VALKRON_CORE_ASSERT(false, "Failed to create GLFW window");
            glfwTerminate();
            return;
        }
        
        glfwMakeContextCurrent(window);
    }

    Window::~Window() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::Update() {
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
    }
}