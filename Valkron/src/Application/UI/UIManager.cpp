#include "Application/UI/UIManager.hpp"
#include "Application/UI/UIElement.hpp"
#include "Input/InputManager.hpp"
#include "Event/Event.hpp"
#include "Core/Log.hpp"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include <algorithm>

namespace Valkron {

void UIManager::sortByLayer(std::vector<std::unique_ptr<UIElement>>& elements) {
    std::sort(elements.begin(), elements.end(),
              [](const std::unique_ptr<UIElement>& a, const std::unique_ptr<UIElement>& b) {
                  return a->getLayerID() < b->getLayerID();
              });
}

void UIManager::collectRenderOrderRecursive(UIElement& element, std::vector<UIElement*>& ordered) {
    ordered.push_back(&element);

    auto& children = element.getChildren();
    sortByLayer(children);
    for (auto& child : children) {
        collectRenderOrderRecursive(*child, ordered);
    }
}

void UIManager::collectRenderOrder(std::vector<std::unique_ptr<UIElement>>& elements,
                                   std::vector<UIElement*>& ordered) {
    sortByLayer(elements);
    for (auto& element : elements) {
        collectRenderOrderRecursive(*element, ordered);
    }
}

bool UIManager::intersectRects(const UIRect& a, const UIRect& b, UIRect& out) {
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.width, b.x + b.width);
    const float y1 = std::min(a.y + a.height, b.y + b.height);

    out.x = x0;
    out.y = y0;
    out.width = std::max(0.0f, x1 - x0);
    out.height = std::max(0.0f, y1 - y0);
    return out.width > 0.0f && out.height > 0.0f;
}

void UIManager::appendDrawDataRecursive(UIElement& element, bool clipEnabled, const UIRect& clipRect,
                                        std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices,
                                        std::vector<UIDrawCommand>& outCommands) {
    element.onRender();

    const std::uint32_t baseVertex = static_cast<std::uint32_t>(outVertices.size());
    const std::uint32_t firstIndex = static_cast<std::uint32_t>(outIndices.size());
    const auto& vertices = element.getVertices();
    const auto& indices = element.getIndices();

    outVertices.insert(outVertices.end(), vertices.begin(), vertices.end());
    for (const std::uint32_t index : indices) {
        outIndices.push_back(baseVertex + index);
    }

    if (!indices.empty()) {
        UIDrawCommand command{};
        command.firstIndex = firstIndex;
        command.indexCount = static_cast<std::uint32_t>(indices.size());
        command.clipEnabled = clipEnabled;
        command.clipRect = clipRect;
        outCommands.push_back(command);
    }

    bool childClipEnabled = clipEnabled;
    UIRect childClipRect = clipRect;
    if (element.clipsChildren()) {
        const UIRect elementClip = element.getChildrenClipRect();
        if (childClipEnabled) {
            intersectRects(childClipRect, elementClip, childClipRect);
        } else {
            childClipEnabled = true;
            childClipRect = elementClip;
        }
    }

    auto& children = element.getChildren();
    sortByLayer(children);
    for (auto& child : children) {
        appendDrawDataRecursive(*child, childClipEnabled, childClipRect, outVertices, outIndices, outCommands);
    }
}

void UIManager::addElement(std::unique_ptr<UIElement> element) {
    m_elements.push_back(std::move(element));
}

void UIManager::removeAll() {
    m_elements.clear();
    m_hovered = nullptr;
    m_pressed = nullptr;
    m_focused = nullptr;
}

void UIManager::render() {
    std::vector<UIElement*> ordered;
    ordered.reserve(m_elements.size());
    collectRenderOrder(m_elements, ordered);

    for (UIElement* element : ordered) {
        element->onRender();
    }
}

void UIManager::buildDrawData(std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices,
                              std::vector<UIDrawCommand>& outCommands) {
    outVertices.clear();
    outIndices.clear();
    outCommands.clear();

    sortByLayer(m_elements);
    for (auto& element : m_elements) {
        appendDrawDataRecursive(*element, false, {}, outVertices, outIndices, outCommands);
    }
}

bool UIManager::handleEvent(Event& e) {
    std::vector<UIElement*> ordered;
    ordered.reserve(m_elements.size());
    collectRenderOrder(m_elements, ordered);

    if (e.type == EventType::MouseMove) {
        auto& me = static_cast<MouseMoveEvent&>(e);
        m_lastMousePosition = glm::vec2(static_cast<float>(me.x), static_cast<float>(me.y));

        m_hovered = nullptr;
        bool consumed = false;
        for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
            UIElement* el = *it;
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
            for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
                UIElement* el = *it;
                if (!el->hitTest(m_lastMousePosition)) {
                    continue;
                }

                if (el->onMouseButtonPressed(be)) {
                    m_pressed = el;
                    if (m_focused != el) {
                        if (m_focused != nullptr) {
                            m_focused->onFocusChanged(false);
                        }
                        m_focused = el;
                        m_focused->onFocusChanged(true);
                    }
                    return true;
                }
            }
            if (m_focused != nullptr) {
                m_focused->onFocusChanged(false);
            }
            m_focused = nullptr;
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

    if (e.type == EventType::MouseScroll) {
        auto& se = static_cast<MouseScrollEvent&>(e);
        m_lastMousePosition = InputManager::getInstance().getMousePosition();

        for (auto it = ordered.rbegin(); it != ordered.rend(); ++it) {
            UIElement* el = *it;
            if (!el->hitTest(m_lastMousePosition)) {
                continue;
            }

            if (el->onMouseScroll(se)) {
                return true;
            }
        }
    }

    if (e.type == EventType::Key) {
        if (m_focused != nullptr) {
            return m_focused->onKeyPressed(static_cast<KeyEvent&>(e));
        }
        return false;
    }

    if (e.type == EventType::CharInput) {
        if (m_focused != nullptr) {
            return m_focused->onCharInput(static_cast<CharInputEvent&>(e));
        }
        return false;
    }

    return false;
}

} // namespace Valkron