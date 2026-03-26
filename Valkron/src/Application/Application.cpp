#include "Application/Application.hpp"
#include "Renderer/Renderer.hpp"

namespace Valkron {

    void Application::onEvent(Event& event) {
        if (event.type == EventType::WindowClose) {
            LOG_INFO("Window close event received. Stopping application loop.");
            isRunning = false;
            event.handled = true;
            return;
        }

        if (event.type == EventType::WindowResize) {
            auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
            LOG_INFO("Window resized to " + std::to_string(resizeEvent.width) + "x" + std::to_string(resizeEvent.height));
            Renderer::onWindowResize(resizeEvent.width, resizeEvent.height);
        }
    }

    Application::Application() {
        m_window = std::make_unique<Window>();
        m_inputManager.init(m_window->getWindow());
        m_inputManager.setEventCallback([this](Event& event) {
            onEvent(event);
        });

        Renderer::init();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window->getWindow(), &width, &height);
        Renderer::onWindowResize(width, height);

        m_inputManager.pushLayer(&m_layer);
        m_layer.onAttach();
        LOG_DEBUG("Application Created!");
    }

    Application::~Application() {
        m_layer.onDetach();
        Renderer::shutdown();
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

            Renderer::beginFrame();
            m_layer.onUpdate(delta_time);
            Renderer::endFrame();

            m_window->Update();
        }
    }

}