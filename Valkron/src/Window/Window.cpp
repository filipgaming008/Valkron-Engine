#include "Window/Window.hpp"
#include "glad/gl.h"

namespace Valkron {

    Window::Window() {
        if (!glfwInit()) {
            VALKRON_CORE_ASSERT(false, "Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        window = glfwCreateWindow(800, 600, "Valkron Engine", nullptr, nullptr);

        if (!window) {
            VALKRON_CORE_ASSERT(false, "Failed to create GLFW window");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(window);

        int gladVersion = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
        VALKRON_CORE_ASSERT(gladVersion != 0, "Failed to initialize GLAD");
    }

    Window::~Window() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void Window::Update() {
        glfwSwapBuffers(window);
    }
}