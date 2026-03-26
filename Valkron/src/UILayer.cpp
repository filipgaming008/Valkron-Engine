#include "UILayer.hpp"
#include "Event.hpp"
#include "GLFW/glfw3.h"
#include "Log.hpp"

namespace Valkron { 

    void UILayer::onAttach() {
        LOG_DEBUG("UILayer attached.");
    }

    void UILayer::onDetach() {
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        LOG_DEBUG("Delta Time: " + std::to_string(deltaTime) + " seconds");
    }

    void UILayer::onEvent(Event& event) {
        if (event.type == EventType::Key) {
            auto& keyEvent = static_cast<KeyEvent&>(event);
            if (keyEvent.action == GLFW_PRESS) {
                LOG_DEBUG("Key " + std::to_string(keyEvent.key) + " pressed.");
                event.handled = true;
            }
            return;
        }

        if (event.type == EventType::MouseButton) {
            auto& mouseButtonEvent = static_cast<MouseButtonEvent&>(event);
            if (mouseButtonEvent.action == GLFW_PRESS) {
                LOG_DEBUG("Mouse button " + std::to_string(mouseButtonEvent.button) + " pressed.");
            }
            return;
        }

        if (event.type == EventType::MouseMove) {
            auto& mouseMoveEvent = static_cast<MouseMoveEvent&>(event);
            LOG_DEBUG("Mouse moved to (" + std::to_string(mouseMoveEvent.x) + ", " + std::to_string(mouseMoveEvent.y) + ").");
            return;
        }

        if (event.type == EventType::MouseScroll) {
            auto& scrollEvent = static_cast<MouseScrollEvent&>(event);
            LOG_DEBUG("Mouse scrolled (" + std::to_string(scrollEvent.xOffset) + ", " + std::to_string(scrollEvent.yOffset) + ").");
        }
    }

}