#include "Application.hpp"

namespace Valkron {

    Application::Application() {
        m_window = std::make_unique<Window>();
        LOG_INFO("Application Created!");
    }

    Application::~Application() {
        LOG_INFO("Application Closed!");
    }

    void Application::Run() {
        
        LOG_INFO("Running Application...");

        while (isRunning) {
            glfwPollEvents();


            m_window->Update();

            if (m_window->shouldClose()) {
                isRunning = false;
            }
        }
    }

}