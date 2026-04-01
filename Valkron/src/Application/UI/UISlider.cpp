#include "Application/UI/UISlider.hpp"

#include "Renderer/UIRenderer.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Valkron {

UISlider::UISlider(const glm::vec2& position, const glm::vec2& size, const glm::vec4& trackColor,
                   const glm::vec4& fillColor, const std::string& label, float value)
    : m_trackColor(trackColor), m_fillColor(fillColor), m_label(label), m_value(std::clamp(value, 0.0f, 1.0f)) {
    m_position = position;
    m_size = size;
    m_geometryDirty = true;
}

void UISlider::onRender() {
    if (m_geometryDirty) {
        rebuildGeometry();
        m_geometryDirty = false;
    }
}

bool UISlider::hitTest(const glm::vec2& point) {
    const glm::vec2 absolute = getAbsolutePosition();
    return point.x >= absolute.x && point.x <= absolute.x + m_size.x && point.y >= absolute.y &&
           point.y <= absolute.y + m_size.y;
}

bool UISlider::onMouseButtonPressed(const MouseButtonEvent& event) {
    if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_PRESS) {
        return false;
    }

    m_dragging = true;
    return true;
}

bool UISlider::onMouseButtonReleased(const MouseButtonEvent& event) {
    if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_RELEASE) {
        return false;
    }

    if (!m_dragging) {
        return false;
    }

    m_dragging = false;
    return true;
}

bool UISlider::onMouseMoved(const MouseMoveEvent& event) {
    const glm::vec2 point(static_cast<float>(event.x), static_cast<float>(event.y));
    const bool hovered = hitTest(point);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        m_geometryDirty = true;
    }

    if (m_dragging) {
        updateValueFromMouse(point.x);
        return true;
    }

    return hovered;
}

void UISlider::setValue(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    if (std::abs(clamped - m_value) > 0.0001f) {
        m_value = clamped;
        m_geometryDirty = true;
        if (m_onValueChanged) {
            m_onValueChanged(m_value);
        }
    }
}

void UISlider::setOnValueChanged(std::function<void(float)> callback) {
    m_onValueChanged = std::move(callback);
}

void UISlider::updateValueFromMouse(float mouseX) {
    const glm::vec2 absolute = getAbsolutePosition();
    const float t = (mouseX - absolute.x) / std::max(1.0f, m_size.x);
    setValue(t);
}

void UISlider::addRect(float x, float y, float w, float h, const glm::vec4& color) {
    const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back({glm::vec3(x, y, 0.0f), glm::vec2(0.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y, 0.0f), glm::vec2(1.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), glm::vec2(1.0f, 1.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x, y + h, 0.0f), glm::vec2(0.0f, 1.0f), color, 0.0f});
    m_indices.insert(m_indices.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void UISlider::rebuildGeometry() {
    m_vertices.clear();
    m_indices.clear();

    const glm::vec2 absolute = getAbsolutePosition();
    const float x = absolute.x;
    const float y = absolute.y;
    const float w = m_size.x;
    const float h = m_size.y;

    glm::vec4 track = m_trackColor;
    if (m_hovered) {
        track.r = std::min(track.r * 1.08f, 1.0f);
        track.g = std::min(track.g * 1.08f, 1.0f);
        track.b = std::min(track.b * 1.08f, 1.0f);
    }

    addRect(x, y, w, h, track);
    addRect(x, y, w * m_value, h, m_fillColor);

    const float knobX = x + w * m_value;
    const float knobW = std::max(8.0f, h * 0.35f);
    addRect(knobX - knobW * 0.5f, y, knobW, h, glm::vec4(0.96f, 0.96f, 0.98f, 1.0f));

    std::ostringstream valueStream;
    valueStream << m_label << " " << std::fixed << std::setprecision(2) << m_value;
    const std::string text = valueStream.str();
    const float textPixelHeight = std::max(14.0f, h * 0.40f);
    const glm::vec2 textPos(x + 8.0f, y + (h - UIRenderer::getLineHeight(textPixelHeight)) * 0.5f);
    UIRenderer::appendTextGeometry(text, textPos, textPixelHeight, m_textColor, m_vertices, m_indices);
}

} // namespace Valkron
