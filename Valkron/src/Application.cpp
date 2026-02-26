#include "Application.hpp"

namespace Valkron {

    Application::Application() {
        m_window = std::make_unique<Window>();
        m_inputManager.init(m_window->getWindow());
        m_inputManager.pushLayer(m_layer.getLayerId());
        LOG_DEBUG("Application Created!");
    }

    Application::~Application() {
        m_layer.onDetach();
        m_inputManager.shutdown();

        LOG_DEBUG("Application Closed!");
    }

    void Application::Run() {

        float lastTime = 0.0f;
        
        LOG_DEBUG("Running Application...");

        while (isRunning) {
            float current_time = glfwGetTime();
            float delta_time = current_time - lastTime;
            lastTime = current_time;

            glfwPollEvents();

            m_inputManager.update();

            m_layer.onUpdate(delta_time);

            m_window->Update();

            if (m_window->shouldClose()) {
                isRunning = false;
            }
        }
    }

}