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
        m_engineConfig.load();

        window = std::make_unique<Window>(m_engineConfig.getSettings());
        VALKRON_CORE_ASSERT(window != nullptr, "Failed to allocate Window object");
        VALKRON_CORE_ASSERT(window->getWindow() != nullptr, "Window initialization failed");

        m_inputManager.init(window->getWindow());
        m_inputManager.setEventCallback([this](Event& event) {
            onEvent(event);
        });

        Renderer::init(window->getWindow());
        int width = 0;
        int height = 0;
        window->getFramebufferSize(width, height);
        Renderer::onWindowResize(width, height);

        m_inputManager.pushLayer(&m_layer);
        m_layer.bindEngineSettings(window.get(), &m_engineConfig);
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
        VALKRON_CORE_ASSERT(window != nullptr && window->getWindow() != nullptr, "Application run requires a valid window");

        float lastTime = 0.0f;

        LOG_DEBUG("Running Application...");

        while (isRunning) {
            float current_time = static_cast<float>(glfwGetTime());
            float delta_time = current_time - lastTime;
            lastTime = current_time;

            glfwPollEvents();

            m_inputManager.update();

            Renderer::beginFrame();
            m_layer.onUpdate(delta_time);
            Renderer::endFrame();

            window->Update();
        }
    }

}