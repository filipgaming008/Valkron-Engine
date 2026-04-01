// UIManager.hpp
#pragma once

#include "Application/UI/UIElement.hpp"
#include "Event/Event.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Valkron {

class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;

    UIManager(const UIManager&) = delete;
    UIManager& operator=(const UIManager&) = delete;
    UIManager(UIManager&&) noexcept = default;
    UIManager& operator=(UIManager&&) noexcept = default;

    void addElement(std::unique_ptr<UIElement> element);
    void removeAll();

    void render();
    void buildDrawData(std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices,
                       std::vector<UIDrawCommand>& outCommands);
    bool handleEvent(Event& e);
    UIElement* getFocusedElement() const {
        return m_focused;
    }

private:
    std::vector<std::unique_ptr<UIElement>> m_elements;
    UIElement* m_hovered = nullptr;
    UIElement* m_pressed = nullptr;
    UIElement* m_focused = nullptr;
    glm::vec2 m_lastMousePosition{0.0f, 0.0f};

    static void sortByLayer(std::vector<std::unique_ptr<UIElement>>& elements);
    static void collectRenderOrder(std::vector<std::unique_ptr<UIElement>>& elements, std::vector<UIElement*>& ordered);
    static void collectRenderOrderRecursive(UIElement& element, std::vector<UIElement*>& ordered);
    static bool intersectRects(const UIRect& a, const UIRect& b, UIRect& out);
    static void appendDrawDataRecursive(UIElement& element, bool clipEnabled, const UIRect& clipRect,
                                        std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices,
                                        std::vector<UIDrawCommand>& outCommands);
};

} // namespace Valkron
