#include "Window.hpp"
#include "Log.hpp"

namespace Valkron {

    Window::Window() {
        if (!glfwInit()) {
            logError("Failed to initialize GLFW");
            return;
        }

        window = glfwCreateWindow(800, 600, "Valkron Engine", nullptr, nullptr);

        if (!window) {
            logError("Failed to create GLFW window");
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