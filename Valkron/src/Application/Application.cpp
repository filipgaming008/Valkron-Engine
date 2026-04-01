#include "Application/Application.hpp"
#include "Renderer/Renderer.hpp"
#include "Core/Settings.hpp"

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
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        if (window != nullptr && window->getWindow() != nullptr) {
            glfwGetFramebufferSize(window->getWindow(), &framebufferWidth, &framebufferHeight);
        }

        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            framebufferWidth = resizeEvent.width;
            framebufferHeight = resizeEvent.height;
        }

        Renderer::onWindowResize(framebufferWidth, framebufferHeight);
    }
}

Application::Application() {
    m_settings = Settings::load();

    window = std::make_unique<Window>(m_settings);
    VALKRON_CORE_ASSERT(window != nullptr, "Failed to allocate Window object");
    VALKRON_CORE_ASSERT(window->getWindow() != nullptr, "Window initialization failed");

    m_inputManager.init(window->getWindow());
    m_inputManager.setEventCallback([this](Event& event) { onEvent(event); });

    Renderer::init();
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window->getWindow(), &width, &height);
    Renderer::onWindowResize(width, height);

        UILayer::UIActionHandlers uiHandlers{};
        uiHandlers.onPlay = []() {
            LOG_INFO("Action handler: Play");
        };
        uiHandlers.onSettings = []() {
            LOG_INFO("Action handler: Settings");
        };
        uiHandlers.onQuit = [this]() {
            LOG_INFO("Action handler: Quit");
            isRunning = false;
            if (window != nullptr && window->getWindow() != nullptr) {
                glfwSetWindowShouldClose(window->getWindow(), GLFW_TRUE);
            }
        };
        uiHandlers.onBloomChanged = [](bool enabled) {
            LOG_INFO("Action handler: Bloom " + std::string(enabled ? "ON" : "OFF"));
        };
        uiHandlers.onExposureChanged = [](float value) {
            LOG_INFO("Action handler: Exposure " + std::to_string(value));
        };
        uiHandlers.onQualityChanged = [](std::size_t index, const std::string& quality) {
            LOG_INFO("Action handler: Quality [" + std::to_string(index) + "] " + quality);
        };
        uiHandlers.onProfileNameSubmitted = [](const std::string& name) {
            LOG_INFO("Action handler: Profile name submitted -> " + name);
        };
        uiHandlers.onGammaChanged = [](float gamma) {
            LOG_INFO("Action handler: Gamma " + std::to_string(gamma));
        };
        m_layer.setActionHandlers(std::move(uiHandlers));

    m_layer.setUIScale(m_settings.uiScale);
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

} // namespace Valkron