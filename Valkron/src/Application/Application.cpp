#include "Application/Application.hpp"
#include "Renderer/Renderer.hpp"

namespace Valkron {

    Application::Application() {
        m_window = std::make_unique<Window>();
        VALKRON_CORE_ASSERT(m_window != nullptr, "Failed to allocate Window object");
        VALKRON_CORE_ASSERT(m_window->getWindow() != nullptr, "Window initialization failed");

        m_inputManager.init(m_window->getWindow());
        m_inputManager.setEventCallback([this](Event& event) {
            onEvent(event);
        });

        Renderer::init();
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_window->getWindow(), &width, &height);
        Renderer::onWindowResize(width, height);

        m_scene = std::make_unique<Scene>();

        LOG_DEBUG("Application Created!");
    }

    Application::~Application() {
        while (!m_layers.empty()) {
            popLayer();
        }

        Renderer::shutdown();
        m_inputManager.shutdown();

        LOG_DEBUG("Application Closed!");
    }

    int Application::run() {
        VALKRON_CORE_ASSERT(m_window != nullptr && m_window->getWindow() != nullptr, "Application run requires a valid window");

        onInit();

        float lastTime = 0.0f;

        LOG_DEBUG("Running Application...");

        while (m_isRunning && !m_window->shouldClose()) {
            float currentTime = glfwGetTime();
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;

            glfwPollEvents();
            m_inputManager.update();

            onUpdate(deltaTime);
            updateLayers(deltaTime);
            if (m_scene) {
                m_scene->update(deltaTime);
            }

            Renderer::beginFrame();
            onRender();
            renderLayers();
            if (m_scene) {
                m_scene->render();
            }
            Renderer::endFrame();

            m_window->Update();
        }

        onShutdown();
        return 0;
    }

    void Application::onEvent(Event& event) {
        if (event.type == EventType::WindowClose) {
            LOG_INFO("Window close event received. Stopping application loop.");
            m_isRunning = false;
            event.handled = true;
            return;
        }

        if (event.type == EventType::WindowResize) {
            auto& resizeEvent = static_cast<WindowResizeEvent&>(event);
            LOG_INFO("Window resized to " + std::to_string(resizeEvent.width) + "x" + std::to_string(resizeEvent.height));
            onWindowResize(resizeEvent.width, resizeEvent.height);
        }
    }

    void Application::onWindowResize(int width, int height) {
        Renderer::onWindowResize(width, height);
    }

    void Application::pushLayer(Layer* layer) {
        VALKRON_CORE_ASSERT(layer != nullptr, "Layer pointer must not be null");

        m_layers.push_back(layer);
        m_inputManager.pushLayer(layer);
        layer->onAttach();
    }

    void Application::popLayer() {
        if (m_layers.empty()) {
            return;
        }

        Layer* layer = m_layers.back();
        layer->onDetach();
        m_inputManager.popLayer();
        m_layers.pop_back();
    }

    void Application::updateLayers(float deltaTime) {
        for (Layer* layer : m_layers) {
            if (layer != nullptr) {
                layer->onUpdate(deltaTime);
            }
        }
    }

    void Application::renderLayers() {
        for (Layer* layer : m_layers) {
            if (layer != nullptr) {
                layer->onRender();
            }
        }
    }

}