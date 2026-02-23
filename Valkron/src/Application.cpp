#include "Application.hpp"

namespace Valkron {

    Application::Application() {
        m_window = std::make_unique<Window>();
        logInfo("Application Created!");
    }

    Application::~Application() {
        logInfo("Application Closed!");
    }

    void Application::Run() {
        while (isRunning) {
            glfwPollEvents();

            logInfo("Running Application...");

            m_window->Update();

            if (m_window->shouldClose()) {
                isRunning = false;
            }
        }
    }

}