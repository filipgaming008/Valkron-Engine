#include "Application/UI/UIManager.hpp"
#include "Application/UI/UIElement.hpp"
#include "Input/InputManager.hpp"
#include "Event/Event.hpp"
#include "Core/Log.hpp"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include <algorithm>

namespace Valkron {

    static void sortByLayer(std::vector<std::unique_ptr<UIElement>>& elements) {
        std::sort(elements.begin(), elements.end(), [](const std::unique_ptr<UIElement>& a, const std::unique_ptr<UIElement>& b) {
            return a->getLayerID() < b->getLayerID();
        });
    }

    void UIManager::addElement(std::unique_ptr<UIElement> element) {
        m_elements.push_back(std::move(element));
    }

    void UIManager::removeAll() {
        m_elements.clear();
        m_hovered = nullptr;
        m_pressed = nullptr;
    }

    void UIManager::render() {
        sortByLayer(m_elements);

        for (const auto& element : m_elements) {
            element->onRender();
        }
    }

    void UIManager::buildDrawData(std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices) {
        outVertices.clear();
        outIndices.clear();

        sortByLayer(m_elements);

        for (const auto& element : m_elements) {
            element->onRender();

            const std::uint32_t baseVertex = static_cast<std::uint32_t>(outVertices.size());
            const auto& vertices = element->getVertices();
            const auto& indices = element->getIndices();

            outVertices.insert(outVertices.end(), vertices.begin(), vertices.end());

            for (const std::uint32_t index : indices) {
                outIndices.push_back(baseVertex + index);
            }
        }
    }

    bool UIManager::handleEvent(Event& e) {
        sortByLayer(m_elements);

        if (e.type == EventType::MouseMove) {
            auto& me = static_cast<MouseMoveEvent&>(e);
            m_lastMousePosition = glm::vec2(static_cast<float>(me.x), static_cast<float>(me.y));

            m_hovered = nullptr;
            bool consumed = false;
            for (auto it = m_elements.rbegin(); it != m_elements.rend(); ++it) {
                UIElement* el = it->get();
                const bool isInside = el->hitTest(m_lastMousePosition);
                const bool handled = el->onMouseMoved(me);

                if (!consumed && isInside) {
                    m_hovered = el;
                    consumed = handled;
                }
            }
            return consumed;
        }

        if (e.type == EventType::MouseButton) {
            auto& be = static_cast<MouseButtonEvent&>(e);
            m_lastMousePosition = InputManager::getInstance().getMousePosition();

            if (be.action == GLFW_PRESS) {
                for (auto it = m_elements.rbegin(); it != m_elements.rend(); ++it) {
                    UIElement* el = it->get();
                    if (!el->hitTest(m_lastMousePosition)) {
                        continue;
                    }

                    if (el->onMouseButtonPressed(be)) {
                        m_pressed = el;
                        return true;
                    }
                }
                return false;
            }

            if (be.action == GLFW_RELEASE) {
                if (m_pressed != nullptr && m_pressed->onMouseButtonReleased(be)) {
                    m_pressed = nullptr;
                    return true;
                }
                m_pressed = nullptr;
            }
        }

        return false;
    }

}