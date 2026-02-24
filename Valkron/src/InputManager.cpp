#include "InputManager.hpp"

namespace Valkron {


    void InputManager::init(GLFWwindow* window) {
        m_window = window;
        
        glfwSetKeyCallback(window, glfwKeyCallback);
        glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);
        glfwSetCursorPosCallback(window, glfwCursorPosCallback);
        glfwSetScrollCallback(window, glfwScrollCallback);
        
        glfwSetWindowUserPointer(window, this);
    }

    void InputManager::update() {
        m_previousKeyStates = m_keyStates;
        m_previousMousePosition = m_mousePosition;
        
        m_mouseDelta = m_mousePosition - m_previousMousePosition;
        
        m_scrollDelta = 0.0f;
    }

    void InputManager::shutdown() {
        m_layerStack.clear();
        m_keyCallbacks.clear();
        m_mouseButtonCallbacks.clear();
        m_mouseMoveCallbacks.clear();
        m_scrollCallbacks.clear();
    }

    void InputManager::pushLayer(int layerId) {
        m_layerStack.push_back(layerId);
        m_layerEnabled[layerId] = true;
    }

    void InputManager::popLayer() {
        if (!m_layerStack.empty()) {
            int layerId = m_layerStack.back();
            m_layerStack.pop_back();
            m_layerEnabled.erase(layerId);
        }
    }

    void InputManager::setLayerEnabled(int layerId, bool enabled) {
        m_layerEnabled[layerId] = enabled;
    }

    bool InputManager::isKeyPressed(int key) const {
        auto it = m_keyStates.find(key);
        return it != m_keyStates.end() && it->second;
    }

    bool InputManager::isMouseButtonPressed(int button) const {
        auto it = m_mouseButtonStates.find(button);
        return it != m_mouseButtonStates.end() && it->second;
    }

    glm::vec2 InputManager::getMousePosition() const {
        return m_mousePosition;
    }

    glm::vec2 InputManager::getMouseDelta() const {
        return m_mouseDelta;
    }

    float InputManager::getScrollDelta() const {
        return m_scrollDelta;
    }

    // Callback registration
    void InputManager::registerKeyCallback(int layerId, KeyCallback callback) {
        m_keyCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerMouseButtonCallback(int layerId, MouseButtonCallback callback) {
        m_mouseButtonCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerMouseMoveCallback(int layerId, MouseMoveCallback callback) {
        m_mouseMoveCallbacks[layerId].push_back(callback);
    }

    void InputManager::registerScrollCallback(int layerId, ScrollCallback callback) {
        m_scrollCallbacks[layerId].push_back(callback);
    }

    void InputManager::glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        auto* manager = static_cast<InputManager*>(glfwGetWindowUserPointer(window));
        if (!manager) return;
        
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
        if (!manager) return;
        
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
        if (!manager) return;
        
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
        if (!manager) return;
        
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