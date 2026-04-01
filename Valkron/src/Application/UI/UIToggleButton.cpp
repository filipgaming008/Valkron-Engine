#include "Application/UI/UIToggleButton.hpp"

#include "Renderer/UIRenderer.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>

namespace Valkron {

UIToggleButton::UIToggleButton(const glm::vec2& position, const glm::vec2& size, const glm::vec4& offColor,
                               const glm::vec4& onColor, const std::string& label, bool initialState,
                               const std::array<glm::vec2, 4>& texCoords)
    : m_offColor(offColor), m_onColor(onColor), m_texCoords(texCoords), m_label(label), m_toggled(initialState) {
    m_position = position;
    m_size = size;
    m_geometryDirty = true;
}

void UIToggleButton::onRender() {
    if (m_geometryDirty) {
        rebuildGeometry();
        m_geometryDirty = false;
    }
}

bool UIToggleButton::hitTest(const glm::vec2& point) {
    const glm::vec2 absolute = getAbsolutePosition();
    return point.x >= absolute.x && point.x <= absolute.x + m_size.x && point.y >= absolute.y &&
           point.y <= absolute.y + m_size.y;
}

bool UIToggleButton::onMouseButtonPressed(const MouseButtonEvent& event) {
    if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_PRESS) {
        return false;
    }

    m_toggled = !m_toggled;
    m_geometryDirty = true;
    if (m_onToggleChanged) {
        m_onToggleChanged(m_toggled);
    }
    return true;
}

bool UIToggleButton::onMouseMoved(const MouseMoveEvent& event) {
    const bool hovered = hitTest(glm::vec2(static_cast<float>(event.x), static_cast<float>(event.y)));
    if (hovered != m_hovered) {
        m_hovered = hovered;
        m_geometryDirty = true;
    }
    return hovered;
}

void UIToggleButton::setToggled(bool toggled) {
    if (m_toggled != toggled) {
        m_toggled = toggled;
        m_geometryDirty = true;
        if (m_onToggleChanged) {
            m_onToggleChanged(m_toggled);
        }
    }
}

void UIToggleButton::setOnToggleChanged(std::function<void(bool)> callback) {
    m_onToggleChanged = std::move(callback);
}

void UIToggleButton::rebuildGeometry() {
    m_vertices.clear();
    m_indices.clear();

    glm::vec4 fill = m_toggled ? m_onColor : m_offColor;
    if (m_hovered) {
        fill.r = std::min(fill.r * 1.1f, 1.0f);
        fill.g = std::min(fill.g * 1.1f, 1.0f);
        fill.b = std::min(fill.b * 1.1f, 1.0f);
    }

    const glm::vec2 absolute = getAbsolutePosition();
    const float x = absolute.x;
    const float y = absolute.y;
    const float w = m_size.x;
    const float h = m_size.y;

    m_vertices.push_back({glm::vec3(x, y, 0.0f), m_texCoords[0], fill, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y, 0.0f), m_texCoords[1], fill, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), m_texCoords[2], fill, 0.0f});
    m_vertices.push_back({glm::vec3(x, y + h, 0.0f), m_texCoords[3], fill, 0.0f});
    m_indices.insert(m_indices.end(), {0, 1, 2, 2, 3, 0});

    const std::string text = m_label + (m_toggled ? " : ON" : " : OFF");
    const float textPixelHeight = std::max(16.0f, h * 0.45f);
    const float textWidth = UIRenderer::measureTextWidth(text, textPixelHeight);
    const float textHeight = UIRenderer::getLineHeight(textPixelHeight);
    const glm::vec2 textPos(x + (w - textWidth) * 0.5f, y + (h - textHeight) * 0.5f);
    UIRenderer::appendTextGeometry(text, textPos, textPixelHeight, m_textColor, m_vertices, m_indices);
}

} // namespace Valkron
