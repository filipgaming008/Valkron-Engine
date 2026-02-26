
#include "InputManager.hpp"
#include "Log.hpp"

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
        glfwSetWindowUserPointer(window, this);
        LOG_INFO("InputManager initialized successfully!");
    }

    void InputManager::update() {
        m_previousKeyStates = m_keyStates;
        m_previousMousePosition = m_mousePosition;
        m_mouseDelta = m_mousePosition - m_previousMousePosition;
        m_scrollDelta = 0.0f;
    }

    void InputManager::shutdown() {
        LOG_INFO("InputManager shutdown called.");
        m_layerStack.clear();
        m_keyCallbacks.clear();
        m_mouseButtonCallbacks.clear();
        m_mouseMoveCallbacks.clear();
        m_scrollCallbacks.clear();
    }

    void InputManager::pushLayer(int layerId) {
        LOG_DEBUG("Pushing layer: " + std::to_string(layerId));
        m_layerStack.push_back(layerId);
        m_layerEnabled[layerId] = true;
        LOG_INFO("Pushed layer " + std::to_string(layerId) + ". Total layers: " + std::to_string(m_layerStack.size()));
    }

    void InputManager::popLayer() {
        LOG_DEBUG("Popping top layer.");
        if (!m_layerStack.empty()) {
            int layerId = m_layerStack.back();
            m_layerStack.pop_back();
            m_layerEnabled.erase(layerId);
            LOG_INFO("Popped layer " + std::to_string(layerId) + ". Remaining layers: " + std::to_string(m_layerStack.size()));
        } else {
            LOG_WARN("Tried to pop layer from empty stack.");
        }
    }

    void InputManager::setLayerEnabled(int layerId, bool enabled) {
        LOG_DEBUG("Setting layer " + std::to_string(layerId) + " enabled: " + (enabled ? "true" : "false"));
        m_layerEnabled[layerId] = enabled;
        LOG_INFO("Set layer " + std::to_string(layerId) + " enabled: " + (enabled ? "true" : "false"));
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

    // Callback registration
    void InputManager::registerKeyCallback(int layerId, KeyCallback callback) {
        LOG_DEBUG("Registering key callback for layer: " + std::to_string(layerId));
        VALKRON_ASSERT(callback != nullptr, "KeyCallback must not be null");
        m_keyCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerMouseButtonCallback(int layerId, MouseButtonCallback callback) {
        LOG_DEBUG("Registering mouse button callback for layer: " + std::to_string(layerId));
        VALKRON_ASSERT(callback != nullptr, "MouseButtonCallback must not be null");
        m_mouseButtonCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerMouseMoveCallback(int layerId, MouseMoveCallback callback) {
        LOG_DEBUG("Registering mouse move callback for layer: " + std::to_string(layerId));
        VALKRON_ASSERT(callback != nullptr, "MouseMoveCallback must not be null");
        m_mouseMoveCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerScrollCallback(int layerId, ScrollCallback callback) {
        LOG_DEBUG("Registering scroll callback for layer: " + std::to_string(layerId));
        VALKRON_ASSERT(callback != nullptr, "ScrollCallback must not be null");
        m_scrollCallbacks[layerId].push_back(callback);
    }

    void InputManager::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW key callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW key callback: key=" + std::to_string(key) + ", action=" + std::to_string(action));
        manager->m_keyStates[key] = (action != GLFW_RELEASE);
        for (auto it = manager->m_layerStack.rbegin(); it != manager->m_layerStack.rend(); ++it) {
            int layerId = *it;
            // Skip disabled layers
            auto enabledIt = manager->m_layerEnabled.find(layerId);
            if (enabledIt != manager->m_layerEnabled.end() && !enabledIt->second)
                continue;
            auto callbacksIt = manager->m_keyCallbacks.find(layerId);
            if (callbacksIt != manager->m_keyCallbacks.end()) {
                for (auto& callback : callbacksIt->second) {
                    callback(key, scancode, action, mods);
                }
            }
        }
    }

    void InputManager::glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW mouse button callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW mouse button callback: button=" + std::to_string(button) + ", action=" + std::to_string(action));
        manager->m_mouseButtonStates[button] = (action != GLFW_RELEASE);
        for (auto it = manager->m_layerStack.rbegin(); it != manager->m_layerStack.rend(); ++it) {
            int layerId = *it;
            auto enabledIt = manager->m_layerEnabled.find(layerId);
            if (enabledIt != manager->m_layerEnabled.end() && !enabledIt->second)
                continue;
            auto callbacksIt = manager->m_mouseButtonCallbacks.find(layerId);
            if (callbacksIt != manager->m_mouseButtonCallbacks.end()) {
                for (auto& callback : callbacksIt->second) {
                    callback(button, action, mods);
                }
            }
        }
    }

    void InputManager::glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW cursor pos callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW cursor pos callback: x=" + std::to_string(xpos) + ", y=" + std::to_string(ypos));
        manager->m_mousePosition = glm::vec2(xpos, ypos);
        for (auto it = manager->m_layerStack.rbegin(); it != manager->m_layerStack.rend(); ++it) {
            int layerId = *it;
            auto enabledIt = manager->m_layerEnabled.find(layerId);
            if (enabledIt != manager->m_layerEnabled.end() && !enabledIt->second)
                continue;
            auto callbacksIt = manager->m_mouseMoveCallbacks.find(layerId);
            if (callbacksIt != manager->m_mouseMoveCallbacks.end()) {
                for (auto& callback : callbacksIt->second) {
                    callback(xpos, ypos);
                }
            }
        }
    }

    void InputManager::glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) {
            LOG_ERROR("GLFW scroll callback: InputManager pointer is null");
            return;
        }
        LOG_DEBUG("GLFW scroll callback: xoffset=" + std::to_string(xoffset) + ", yoffset=" + std::to_string(yoffset));
        manager->m_scrollDelta = static_cast<float>(yoffset);
        for (auto it = manager->m_layerStack.rbegin(); it != manager->m_layerStack.rend(); ++it) {
            int layerId = *it;
            auto enabledIt = manager->m_layerEnabled.find(layerId);
            if (enabledIt != manager->m_layerEnabled.end() && !enabledIt->second)
                continue;
            auto callbacksIt = manager->m_scrollCallbacks.find(layerId);
            if (callbacksIt != manager->m_scrollCallbacks.end()) {
                for (auto& callback : callbacksIt->second) {
                    callback(xoffset, yoffset);
                }
            }
        }
    }

}