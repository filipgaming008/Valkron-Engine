#include "Window/Window.hpp"
#include "glad/gl.h"

#include <algorithm>

namespace Valkron {

    static constexpr int kWindowTopOffsetPixels = 32;

    struct WindowPlacement {
        int x = 0;
        int y = 0;
        int width = 1280;
        int height = 720;
    };

    int clampWindowDimension(int value, int fallbackValue) {
        return std::max(64, value > 0 ? value : fallbackValue);
    }

    int computeWindowedYPosition(int monitorHeight, int windowHeight) {
        const int centeredY = std::max(0, (monitorHeight - windowHeight) / 2);
        return std::max(kWindowTopOffsetPixels, centeredY);
    }

    WindowPlacement getPrimaryMonitorWorkAreaOrFallback(GLFWmonitor* monitor, const GLFWvidmode* videoMode, int fallbackWidth, int fallbackHeight) {
        WindowPlacement placement;
        placement.width = clampWindowDimension(fallbackWidth, 1280);
        placement.height = clampWindowDimension(fallbackHeight, 720);

        if (monitor == nullptr) {
            return placement;
        }

        int workAreaX = 0;
        int workAreaY = 0;
        int workAreaWidth = 0;
        int workAreaHeight = 0;
        glfwGetMonitorWorkarea(monitor, &workAreaX, &workAreaY, &workAreaWidth, &workAreaHeight);

        if (workAreaWidth > 0 && workAreaHeight > 0) {
            placement.x = workAreaX;
            placement.y = workAreaY;
            placement.width = workAreaWidth;
            placement.height = workAreaHeight;
            return placement;
        }

        if (videoMode != nullptr) {
            placement.width = videoMode->width;
            placement.height = videoMode->height;
        }

        return placement;
    }

    Window::Window(const EngineSettings& settings) {
        if (!glfwInit()) {
            LOG_ERROR("Failed to initialize GLFW");
            VALKRON_CORE_ASSERT(false, "Failed to initialize GLFW");
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = primaryMonitor != nullptr ? glfwGetVideoMode(primaryMonitor) : nullptr;

        int initialWidth = clampWindowDimension(settings.windowWidth, 1280);
        int initialHeight = clampWindowDimension(settings.windowHeight, 720);
        if (settings.autoDetectMonitorSize && videoMode != nullptr) {
            initialWidth = videoMode->width;
            initialHeight = videoMode->height;
        }

        const bool useDockedFullscreenWindowed = !settings.fullscreen && settings.dockedFullscreenWindowed;
        if (useDockedFullscreenWindowed) {
            const WindowPlacement dockedPlacement = getPrimaryMonitorWorkAreaOrFallback(primaryMonitor, videoMode, initialWidth, initialHeight);
            initialWidth = dockedPlacement.width;
            initialHeight = dockedPlacement.height;
        }

        m_windowTitle = settings.windowTitle.empty() ? "Valkron Engine" : settings.windowTitle;
        m_isFullscreen = settings.fullscreen;
        m_isDockedFullscreenWindowed = useDockedFullscreenWindowed;
        m_windowWidth = initialWidth;
        m_windowHeight = initialHeight;
        m_windowedWidth = initialWidth;
        m_windowedHeight = initialHeight;

        m_window = glfwCreateWindow(
            initialWidth,
            initialHeight,
            m_windowTitle.c_str(),
            m_isFullscreen ? primaryMonitor : nullptr,
            nullptr
        );

        if (!m_window) {
            LOG_ERROR("Failed to create GLFW window");
            VALKRON_CORE_ASSERT(false, "Failed to create GLFW window");
            glfwTerminate();
            return;
        }

        if (!m_isFullscreen) {
            if (m_isDockedFullscreenWindowed) {
                const WindowPlacement dockedPlacement = getPrimaryMonitorWorkAreaOrFallback(primaryMonitor, videoMode, initialWidth, initialHeight);
                glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
                glfwSetWindowMonitor(m_window, nullptr, dockedPlacement.x, dockedPlacement.y, dockedPlacement.width, dockedPlacement.height, GLFW_DONT_CARE);
                glfwMaximizeWindow(m_window);

                glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
                glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
                m_windowWidth = m_windowedWidth;
                m_windowHeight = m_windowedHeight;
            } else if (videoMode != nullptr) {
                const int centeredX = std::max(0, (videoMode->width - initialWidth) / 2);
                const int centeredY = computeWindowedYPosition(videoMode->height, initialHeight);
                glfwSetWindowPos(m_window, centeredX, centeredY);
                m_windowedX = centeredX;
                m_windowedY = centeredY;
            }
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

        LOG_INFO("Created window " + std::to_string(initialWidth) + "x" + std::to_string(initialHeight) + (m_isFullscreen ? " (fullscreen)" : " (windowed)"));
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

    void Window::applySettings(const EngineSettings& settings) {
        VALKRON_CORE_ASSERT(m_window != nullptr, "Window::applySettings called with null GLFW window");

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = primaryMonitor != nullptr ? glfwGetVideoMode(primaryMonitor) : nullptr;

        int targetWidth = clampWindowDimension(settings.windowWidth, m_windowWidth);
        int targetHeight = clampWindowDimension(settings.windowHeight, m_windowHeight);
        if (settings.autoDetectMonitorSize && videoMode != nullptr) {
            targetWidth = videoMode->width;
            targetHeight = videoMode->height;
        }

        const bool wantDockedFullscreenWindowed = !settings.fullscreen && settings.dockedFullscreenWindowed;

        const std::string targetTitle = settings.windowTitle.empty() ? "Valkron Engine" : settings.windowTitle;
        if (targetTitle != m_windowTitle) {
            glfwSetWindowTitle(m_window, targetTitle.c_str());
            m_windowTitle = targetTitle;
        }

        if (settings.fullscreen) {
            if (!m_isFullscreen) {
                glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
                glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
            }

            const int refreshRate = videoMode != nullptr ? videoMode->refreshRate : GLFW_DONT_CARE;
            glfwSetWindowMonitor(m_window, primaryMonitor, 0, 0, targetWidth, targetHeight, refreshRate);
            m_isFullscreen = true;
            m_isDockedFullscreenWindowed = false;
        } else if (wantDockedFullscreenWindowed) {
            const WindowPlacement dockedPlacement = getPrimaryMonitorWorkAreaOrFallback(primaryMonitor, videoMode, targetWidth, targetHeight);

            if (m_isFullscreen) {
                glfwSetWindowMonitor(m_window, nullptr, dockedPlacement.x, dockedPlacement.y, dockedPlacement.width, dockedPlacement.height, GLFW_DONT_CARE);
            }

            glfwRestoreWindow(m_window);
            glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
            glfwSetWindowPos(m_window, dockedPlacement.x, dockedPlacement.y);
            glfwSetWindowSize(m_window, dockedPlacement.width, dockedPlacement.height);
            glfwMaximizeWindow(m_window);

            m_isFullscreen = false;
            m_isDockedFullscreenWindowed = true;

            glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
            glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
            m_windowWidth = m_windowedWidth;
            m_windowHeight = m_windowedHeight;
        } else {
            if (m_isFullscreen) {
                glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, targetWidth, targetHeight, GLFW_DONT_CARE);
            } else {
                if (m_isDockedFullscreenWindowed) {
                    glfwRestoreWindow(m_window);
                }
                glfwSetWindowSize(m_window, targetWidth, targetHeight);
            }

            m_isFullscreen = false;
            m_isDockedFullscreenWindowed = false;
            m_windowedWidth = targetWidth;
            m_windowedHeight = targetHeight;

            if (videoMode != nullptr) {
                const int centeredX = std::max(0, (videoMode->width - targetWidth) / 2);
                const int centeredY = computeWindowedYPosition(videoMode->height, targetHeight);
                glfwSetWindowPos(m_window, centeredX, centeredY);
                m_windowedX = centeredX;
                m_windowedY = centeredY;
            }
        }

        m_windowWidth = targetWidth;
        m_windowHeight = targetHeight;
    }

    EngineSettings Window::getCurrentSettings() const {
        EngineSettings settings;
        settings.windowWidth = m_windowWidth;
        settings.windowHeight = m_windowHeight;
        settings.fullscreen = m_isFullscreen;
        settings.dockedFullscreenWindowed = m_isDockedFullscreenWindowed;
        settings.autoDetectMonitorSize = false;
        settings.windowTitle = m_windowTitle;
        return settings;
    }

    std::pair<int, int> Window::getPrimaryMonitorSize() const {
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* videoMode = primaryMonitor != nullptr ? glfwGetVideoMode(primaryMonitor) : nullptr;
        if (videoMode == nullptr) {
            return {m_windowWidth, m_windowHeight};
        }

        return {videoMode->width, videoMode->height};
    }

    void Window::getFramebufferSize(int& width, int& height) const {
        VALKRON_CORE_ASSERT(m_window != nullptr, "Window::getFramebufferSize called with null GLFW window");
        glfwGetFramebufferSize(m_window, &width, &height);
    }
}