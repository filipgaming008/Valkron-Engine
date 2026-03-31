#include "Application/UI/UILayer.hpp"
#include "Application/UI/UIButton.hpp"
#include "Event/Event.hpp"
#include "Core/Log.hpp"
#include "Renderer/Renderer.hpp"

#include "GLFW/glfw3.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Valkron {

    void UILayer::onAttach() {
        auto playButton = std::make_unique<UIButton>(
            glm::vec2(40.0f, 40.0f),
            glm::vec2(220.0f, 80.0f),
            glm::vec4(0.15f, 0.65f, 0.95f, 0.95f),
            "Play"
        );
        playButton->setLayerID(10);
        m_uiManager.addElement(std::move(playButton));

        auto settingsButton = std::make_unique<UIButton>(
            glm::vec2(40.0f, 140.0f),
            glm::vec2(220.0f, 80.0f),
            glm::vec4(0.20f, 0.80f, 0.40f, 0.95f),
            "Settings"
        );
        settingsButton->setLayerID(10);
        m_uiManager.addElement(std::move(settingsButton));

        auto quitButton = std::make_unique<UIButton>(
            glm::vec2(40.0f, 240.0f),
            glm::vec2(220.0f, 80.0f),
            glm::vec4(0.90f, 0.30f, 0.30f, 0.95f),
            "Quit"
        );
        quitButton->setLayerID(10);
        m_uiManager.addElement(std::move(quitButton));

        auto overlayButton = std::make_unique<UIButton>(
            glm::vec2(170.0f, 78.0f),
            glm::vec2(180.0f, 68.0f),
            glm::vec4(0.98f, 0.85f, 0.25f, 0.95f),
            "TopOverlay"
        );
        overlayButton->setLayerID(50);
        m_uiManager.addElement(std::move(overlayButton));

        LOG_DEBUG("UILayer attached with test buttons.");
    }

    void UILayer::onDetach() {
        LOG_DEBUG("UILayer detached.");
    }

    void UILayer::onUpdate(float deltaTime) {
        (void)deltaTime;

        std::vector<UIVertex> vertices;
        std::vector<std::uint32_t> indices;
        m_uiManager.buildDrawData(vertices, indices);
        static bool loggedBatch = false;
        if (!loggedBatch) {
            LOG_INFO("UILayer batch stats: vertices=" + std::to_string(vertices.size()) + ", indices=" + std::to_string(indices.size()));
            loggedBatch = true;
        }
        Renderer::submitUIBatch(vertices, indices);
    }

    void UILayer::onEvent(Event& event) {
        if (m_uiManager.handleEvent(event)) {
            event.handled = true;
            return;
        }

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