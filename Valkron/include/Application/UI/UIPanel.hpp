#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <array>
#include <string>

namespace Valkron {

enum class UIPanelLayoutMode { None, Vertical, Horizontal };

class VALKRON_API UIPanel : public UIElement {
public:
    UIPanel(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, const std::string& title = "",
            const std::array<glm::vec2, 4>& texCoords = {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
                                                         glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)});

    void onRender() override;
    bool hitTest(const glm::vec2& point) override;
    bool onMouseMoved(const MouseMoveEvent& event) override;
    bool onMouseScroll(const MouseScrollEvent& event) override;
    bool clipsChildren() const override;
    UIRect getChildrenClipRect() const override;

    void setPosition(const glm::vec2& position);
    void setSize(const glm::vec2& size);
    void setColor(const glm::vec4& color);
    void setTitle(const std::string& title);
    void setPadding(float padding);
    void setItemSpacing(float spacing);

    float getPadding() const {
        return m_padding;
    }
    float getItemSpacing() const {
        return m_itemSpacing;
    }
    float getTitleBarHeight() const;

    void layoutVertical(float startOffset = 0.0f);
    void layoutHorizontal(float startOffset = 0.0f);
    void setScrollingEnabled(bool enabled);
    void setClipChildren(bool enabled);
    void setScrollSpeed(float speed);
    void setScrollOffset(float offset);
    float getScrollOffset() const {
        return m_scrollOffset;
    }

protected:
    void rebuildGeometry() override;

private:
    void applyLayout();
    void updateScrollBounds();

    glm::vec4 m_color;
    glm::vec4 m_titleColor{0.94f, 0.94f, 0.94f, 1.0f};
    std::array<glm::vec2, 4> m_texCoords;
    std::string m_title;
    bool m_hovered = false;
    float m_padding = 12.0f;
    float m_itemSpacing = 10.0f;
    bool m_scrollingEnabled = false;
    bool m_clipChildren = false;
    float m_scrollSpeed = 24.0f;
    float m_scrollOffset = 0.0f;
    float m_maxScrollOffset = 0.0f;
    float m_layoutStartOffset = 0.0f;
    UIPanelLayoutMode m_layoutMode = UIPanelLayoutMode::None;
};

} // namespace Valkron
