#include "UILayer.hpp"
#include "InputManager.hpp"
#include "Layer.hpp"

namespace Valkron { 

    void UILayer::onAttach() {
        LOG_DEBUG("UILayer attached.");
    }

    void UILayer::onDetach() {
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        auto& inputManager = InputManager::getInstance();

        LOG_DEBUG("Delta Time: " + std::to_string(deltaTime) + " seconds");

        for(int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
            if (inputManager.isKeyPressed(key)) {
                LOG_DEBUG("Key " + std::to_string(key) + " is pressed.");
            }
        }
    }

}