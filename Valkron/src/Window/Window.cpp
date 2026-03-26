#include "Window/Window.hpp"
#include "glad/gl.h"

namespace Valkron {

    Window::Window() {
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            VALKRON_CORE_ASSERT(false, "Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_window = glfwCreateWindow(800, 600, "Valkron Engine", nullptr, nullptr);

        if (!m_window) {
            LOG_ERROR("Failed to create GLFW window");
            VALKRON_CORE_ASSERT(false, "Failed to create GLFW window");
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(m_window);

        int gladVersion = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
        if (gladVersion == 0) {
            LOG_ERROR("Failed to initialize GLAD");
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            glfwTerminate();
        }
        VALKRON_CORE_ASSERT(gladVersion != 0, "Failed to initialize GLAD");
    }

    Window::~Window() {
        if (m_window) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

    void Window::Update() {
        VALKRON_CORE_ASSERT(m_window != nullptr, "Window::Update called with null GLFW window");
        glfwSwapBuffers(m_window);
    }
}