#include "Window/Window.hpp"
#include "Core/Settings.hpp"
#include "glad/gl.h"

namespace Valkron {

Window::Window(const EngineSettings& settings) {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        VALKRON_CORE_ASSERT(false, "Failed to initialize GLFW");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int windowWidth = 1280;
    int windowHeight = 720;
    int windowPosX = 0;
    int windowPosY = 0;

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor != nullptr) {
        int monitorWidth = windowWidth;
        int monitorHeight = windowHeight;
        if (const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor)) {
            monitorWidth = videoMode->width;
            monitorHeight = videoMode->height;
        }
        glfwGetMonitorPos(primaryMonitor, &windowPosX, &windowPosY);

        if (settings.windowUseDesktopResolution) {
            windowWidth = monitorWidth;
            windowHeight = monitorHeight;
        } else {
            windowWidth = settings.windowWidth;
            windowHeight = settings.windowHeight;
            windowPosX += (monitorWidth - windowWidth) / 2;
            windowPosY += (monitorHeight - windowHeight) / 2;
        }
    } else {
        windowWidth = settings.windowWidth;
        windowHeight = settings.windowHeight;
    }

    m_window = glfwCreateWindow(windowWidth, windowHeight, "Valkron Engine", nullptr, nullptr);

    if (!m_window) {
        LOG_ERROR("Failed to create GLFW window");
        VALKRON_CORE_ASSERT(false, "Failed to create GLFW window");
        glfwTerminate();
        return;
    }

    glfwSetWindowPos(m_window, windowPosX, windowPosY);

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
} // namespace Valkron