#include "Application/UI/UIPanel.hpp"

#include "Renderer/UIRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace Valkron {

UIPanel::UIPanel(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color, const std::string& title,
                 const std::array<glm::vec2, 4>& texCoords)
    : m_color(color), m_texCoords(texCoords), m_title(title) {
    m_position = position;
    m_size = size;
    m_geometryDirty = true;
}

void UIPanel::onRender() {
    if (m_geometryDirty) {
        rebuildGeometry();
        m_geometryDirty = false;
    }
}

bool UIPanel::hitTest(const glm::vec2& point) {
    const glm::vec2 absolute = getAbsolutePosition();
    const float minX = absolute.x;
    const float minY = absolute.y;
    const float maxX = absolute.x + m_size.x;
    const float maxY = absolute.y + m_size.y;

    return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
}

bool UIPanel::onMouseMoved(const MouseMoveEvent& event) {
    const bool hovered = hitTest(glm::vec2(static_cast<float>(event.x), static_cast<float>(event.y)));
    if (hovered != m_hovered) {
        m_hovered = hovered;
        m_geometryDirty = true;
    }

    return false;
}

bool UIPanel::onMouseScroll(const MouseScrollEvent& event) {
    if (!m_scrollingEnabled || m_layoutMode != UIPanelLayoutMode::Vertical) {
        return false;
    }

    if (m_maxScrollOffset <= 0.0f) {
        return false;
    }

    const float previous = m_scrollOffset;
    m_scrollOffset =
        std::clamp(m_scrollOffset - static_cast<float>(event.yOffset) * m_scrollSpeed, 0.0f, m_maxScrollOffset);

    if (std::abs(previous - m_scrollOffset) > 0.001f) {
        applyLayout();
        markGeometryDirty();
        return true;
    }

    return false;
}

bool UIPanel::clipsChildren() const {
    return m_clipChildren;
}

UIRect UIPanel::getChildrenClipRect() const {
    const glm::vec2 absolute = getAbsolutePosition();
    const float top = getTitleBarHeight() + m_padding;

    UIRect rect{};
    rect.x = absolute.x + m_padding;
    rect.y = absolute.y + top;
    rect.width = std::max(0.0f, m_size.x - (m_padding * 2.0f));
    rect.height = std::max(0.0f, m_size.y - top - m_padding);
    return rect;
}

void UIPanel::setPosition(const glm::vec2& position) {
    setLocalPosition(position);
}

void UIPanel::setSize(const glm::vec2& size) {
    setLocalSize(size);
    updateScrollBounds();
    applyLayout();
}

void UIPanel::setColor(const glm::vec4& color) {
    m_color = color;
    markGeometryDirty();
}

void UIPanel::setTitle(const std::string& title) {
    m_title = title;
    markGeometryDirty();
}

void UIPanel::setPadding(float padding) {
    m_padding = std::max(0.0f, padding);
    updateScrollBounds();
    applyLayout();
    markGeometryDirty(true);
}

void UIPanel::setItemSpacing(float spacing) {
    m_itemSpacing = std::max(0.0f, spacing);
    updateScrollBounds();
    applyLayout();
    markGeometryDirty(true);
}

float UIPanel::getTitleBarHeight() const {
    if (m_title.empty()) {
        return m_padding;
    }

    return std::max(24.0f, m_size.y * 0.12f);
}

void UIPanel::layoutVertical(float startOffset) {
    m_layoutMode = UIPanelLayoutMode::Vertical;
    m_layoutStartOffset = startOffset;
    updateScrollBounds();
    applyLayout();
}

void UIPanel::layoutHorizontal(float startOffset) {
    m_layoutMode = UIPanelLayoutMode::Horizontal;
    m_layoutStartOffset = startOffset;
    m_scrollOffset = 0.0f;
    m_maxScrollOffset = 0.0f;
    applyLayout();
}

void UIPanel::setScrollingEnabled(bool enabled) {
    m_scrollingEnabled = enabled;
    m_clipChildren = enabled;
    if (!m_scrollingEnabled) {
        m_scrollOffset = 0.0f;
    }
    updateScrollBounds();
    applyLayout();
}

void UIPanel::setClipChildren(bool enabled) {
    m_clipChildren = enabled;
}

void UIPanel::setScrollSpeed(float speed) {
    m_scrollSpeed = std::max(2.0f, speed);
}

void UIPanel::setScrollOffset(float offset) {
    m_scrollOffset = std::clamp(offset, 0.0f, m_maxScrollOffset);
    applyLayout();
}

void UIPanel::updateScrollBounds() {
    if (m_layoutMode != UIPanelLayoutMode::Vertical || getChildren().empty()) {
        m_maxScrollOffset = 0.0f;
        m_scrollOffset = 0.0f;
        return;
    }

    float contentHeight = 0.0f;
    for (const auto& child : getChildren()) {
        contentHeight += child->getSize().y;
    }
    contentHeight += m_itemSpacing * static_cast<float>(getChildren().size() - 1);

    const float top = getTitleBarHeight() + m_padding + m_layoutStartOffset;
    const float visibleHeight = std::max(0.0f, m_size.y - top - m_padding);
    m_maxScrollOffset = std::max(0.0f, contentHeight - visibleHeight);
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, m_maxScrollOffset);
}

void UIPanel::applyLayout() {
    if (m_layoutMode == UIPanelLayoutMode::Vertical) {
        const float availableWidth = std::max(0.0f, m_size.x - (m_padding * 2.0f));
        float currentY = getTitleBarHeight() + m_padding + m_layoutStartOffset - m_scrollOffset;

        for (auto& child : getChildren()) {
            const glm::vec2 childSize = child->getSize();
            child->setLocalPosition(glm::vec2(m_padding, currentY));
            child->setLocalSize(glm::vec2(std::min(childSize.x, availableWidth), childSize.y));
            currentY += child->getSize().y + m_itemSpacing;
        }
        return;
    }

    if (m_layoutMode == UIPanelLayoutMode::Horizontal) {
        const float topY = getTitleBarHeight() + m_padding;
        const float availableHeight = std::max(0.0f, m_size.y - topY - m_padding);
        float currentX = m_padding + m_layoutStartOffset;

        for (auto& child : getChildren()) {
            const glm::vec2 childSize = child->getSize();
            child->setLocalPosition(glm::vec2(currentX, topY));
            child->setLocalSize(glm::vec2(childSize.x, std::min(childSize.y, availableHeight)));
            currentX += child->getSize().x + m_itemSpacing;
        }
    }
}

void UIPanel::rebuildGeometry() {
    m_vertices.clear();
    m_indices.clear();

    const glm::vec2 absolute = getAbsolutePosition();
    const float x = absolute.x;
    const float y = absolute.y;
    const float w = m_size.x;
    const float h = m_size.y;

    glm::vec4 fillColor = m_color;
    if (m_hovered) {
        fillColor.r = std::min(fillColor.r * 1.08f, 1.0f);
        fillColor.g = std::min(fillColor.g * 1.08f, 1.0f);
        fillColor.b = std::min(fillColor.b * 1.08f, 1.0f);
    }

    m_vertices.reserve(20 + m_title.size() * 4);
    m_indices.reserve(30 + m_title.size() * 6);

    m_vertices.push_back({glm::vec3(x, y, 0.0f), m_texCoords[0], fillColor, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y, 0.0f), m_texCoords[1], fillColor, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), m_texCoords[2], fillColor, 0.0f});
    m_vertices.push_back({glm::vec3(x, y + h, 0.0f), m_texCoords[3], fillColor, 0.0f});
    m_indices.insert(m_indices.end(), {0, 1, 2, 2, 3, 0});

    const float border = 2.0f;
    const glm::vec4 borderColor(std::min(fillColor.r * 1.3f, 1.0f), std::min(fillColor.g * 1.3f, 1.0f),
                                std::min(fillColor.b * 1.3f, 1.0f), 1.0f);

    auto addRect = [this](float rx, float ry, float rw, float rh, const glm::vec4& color) {
        const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
        m_vertices.push_back({glm::vec3(rx, ry, 0.0f), glm::vec2(0.0f, 0.0f), color, 0.0f});
        m_vertices.push_back({glm::vec3(rx + rw, ry, 0.0f), glm::vec2(1.0f, 0.0f), color, 0.0f});
        m_vertices.push_back({glm::vec3(rx + rw, ry + rh, 0.0f), glm::vec2(1.0f, 1.0f), color, 0.0f});
        m_vertices.push_back({glm::vec3(rx, ry + rh, 0.0f), glm::vec2(0.0f, 1.0f), color, 0.0f});
        m_indices.push_back(base + 0);
        m_indices.push_back(base + 1);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 2);
        m_indices.push_back(base + 3);
        m_indices.push_back(base + 0);
    };

    addRect(x, y, w, border, borderColor);
    addRect(x, y + h - border, w, border, borderColor);
    addRect(x, y, border, h, borderColor);
    addRect(x + w - border, y, border, h, borderColor);

    if (!m_title.empty()) {
        const float textPixelHeight = std::max(16.0f, std::min(h * 0.12f, 24.0f));
        const glm::vec2 titlePos(x + 12.0f, y + 8.0f);
        UIRenderer::appendTextGeometry(m_title, titlePos, textPixelHeight, m_titleColor, m_vertices, m_indices);
    }
}

} // namespace Valkron
