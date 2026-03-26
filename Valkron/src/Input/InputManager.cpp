#include "Input/InputManager.hpp"
#include "Application/Layer.hpp"
#include "Core/Log.hpp"

namespace Valkron {


    void InputManager::init(GLFWwindow* window) {
        LOG_INFO("Initializing InputManager...");
        VALKRON_CORE_ASSERT(window != nullptr, "InputManager initialization failed: GLFW window is null");
        m_window = window;
        LOG_DEBUG("Setting GLFW callbacks for InputManager...");
        glfwSetKeyCallback(window, glfwKeyCallback);
        glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
        glfwSetCursorPosCallback(window, glfwCursorPosCallback);
        glfwSetScrollCallback(window, glfwScrollCallback);
        glfwSetWindowCloseCallback(window, glfwWindowCloseCallback);
        glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);
        glfwSetWindowUserPointer(window, this);
        LOG_INFO("InputManager initialized successfully!");
    }

    void InputManager::setEventCallback(EventCallback callback) {
        m_eventCallback = callback;
    }

    void InputManager::update() {
        m_previousKeyStates = m_keyStates;
        m_mouseDelta = m_mousePosition - m_previousMousePosition;
        m_previousMousePosition = m_mousePosition;
        m_scrollDelta = 0.0f;
    }

    void InputManager::shutdown() {
        LOG_INFO("InputManager shutdown called.");
        m_layerStack.clear();
        m_layerEnabled.clear();
        m_eventCallback = nullptr;
    }

    void InputManager::pushLayer(Layer* layer) {
        VALKRON_ASSERT(layer != nullptr, "Layer pointer must not be null");
        LOG_DEBUG("Pushing layer: " + std::to_string(layer->getLayerId()));
        m_layerStack.push_back(layer);
        m_layerEnabled[layer] = true;
        LOG_INFO("Pushed layer " + std::to_string(layer->getLayerId()) + ". Total layers: " + std::to_string(m_layerStack.size()));
    }

    void InputManager::popLayer() {
        LOG_DEBUG("Popping top layer.");
        if (!m_layerStack.empty()) {
            Layer* layer = m_layerStack.back();
            m_layerStack.pop_back();
            m_layerEnabled.erase(layer);
            LOG_INFO("Popped layer " + std::to_string(layer->getLayerId()) + ". Remaining layers: " + std::to_string(m_layerStack.size()));
        } else {
            LOG_WARN("Tried to pop layer from empty stack.");
        }
    }

    void InputManager::setLayerEnabled(Layer* layer, bool enabled) {
        VALKRON_ASSERT(layer != nullptr, "Layer pointer must not be null");
        LOG_DEBUG("Setting layer " + std::to_string(layer->getLayerId()) + " enabled: " + (enabled ? "true" : "false"));
        m_layerEnabled[layer] = enabled;
        LOG_INFO("Set layer " + std::to_string(layer->getLayerId()) + " enabled: " + (enabled ? "true" : "false"));
    }

    bool InputManager::isKeyPressed(int key) const {
        VALKRON_ASSERT(key >= 0, "Key code must be non-negative");
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second;
    }

    bool InputManager::isMouseButtonPressed(int button) const {
        VALKRON_ASSERT(button >= 0, "Mouse button code must be non-negative");
        auto it = m_mouseButtonStates.find(button);
        return it != m_mouseButtonStates.end() && it->second;
    }

    glm::vec2 InputManager::getMousePosition() const {
        LOG_DEBUG("Getting mouse position: (" + std::to_string(m_mousePosition.x) + ", " + std::to_string(m_mousePosition.y) + ")");
        return m_mousePosition;
    }

    glm::vec2 InputManager::getMouseDelta() const {
        LOG_DEBUG("Getting mouse delta: (" + std::to_string(m_mouseDelta.x) + ", " + std::to_string(m_mouseDelta.y) + ")");
        return m_mouseDelta;
    }

    float InputManager::getScrollDelta() const {
        LOG_DEBUG("Getting scroll delta: " + std::to_string(m_scrollDelta));
        return m_scrollDelta;
    }

    void InputManager::dispatchEvent(Event& event) {
        if (m_eventCallback) {
            m_eventCallback(event);
        }

        if (event.handled) {
            return;
        }

        for (auto it = m_layerStack.rbegin(); it != m_layerStack.rend(); ++it) {
            Layer* layer = *it;
            auto enabledIt = m_layerEnabled.find(layer);
            if (enabledIt != m_layerEnabled.end() && !enabledIt->second) {
                continue;
            }

            layer->onEvent(event);
            if (event.handled) {
                break;
            }
        }
    }

    void InputManager::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW key callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW key callback: key=" + std::to_string(key) + ", action=" + std::to_string(action));
        manager->m_keyStates[key] = (action != GLFW_RELEASE);

        KeyEvent event(key, scancode, action, mods);
        manager->dispatchEvent(event);
    }

    void InputManager::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW mouse button callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW mouse button callback: button=" + std::to_string(button) + ", action=" + std::to_string(action));
        manager->m_mouseButtonStates[button] = (action != GLFW_RELEASE);

        MouseButtonEvent event(button, action, mods);
        manager->dispatchEvent(event);
    }

    void InputManager::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW cursor pos callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW cursor pos callback: x=" + std::to_string(xpos) + ", y=" + std::to_string(ypos));
        manager->m_mousePosition = glm::vec2(xpos, ypos);

        MouseMoveEvent event(xpos, ypos);
        manager->dispatchEvent(event);
    }

    void InputManager::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW scroll callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW scroll callback: xoffset=" + std::to_string(xoffset) + ", yoffset=" + std::to_string(yoffset));
        manager->m_scrollDelta = static_cast<float>(yoffset);

        MouseScrollEvent event(xoffset, yoffset);
        manager->dispatchEvent(event);
    }

    void InputManager::glfwWindowCloseCallback(GLFWwindow* window) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW window close callback: InputManager pointer is null");
            return;
        }

        LOG_DEBUG("GLFW window close callback triggered");
        WindowCloseEvent event;
        manager->dispatchEvent(event);
    }

    void InputManager::glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW framebuffer size callback: InputManager pointer is null");
            return;
        }

        LOG_DEBUG("GLFW framebuffer size callback: width=" + std::to_string(width) + ", height=" + std::to_string(height));
        WindowResizeEvent event(width, height);
        manager->dispatchEvent(event);
    }

}