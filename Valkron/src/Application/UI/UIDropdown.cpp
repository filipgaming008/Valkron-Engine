#include "Application/UI/UIDropdown.hpp"

#include "Input/InputManager.hpp"
#include "Renderer/UIRenderer.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>

namespace Valkron {

UIDropdown::UIDropdown(const glm::vec2& position, const glm::vec2& size, const glm::vec4& headerColor,
                       const glm::vec4& menuColor, std::vector<std::string> options, std::size_t selectedIndex)
    : m_headerColor(headerColor), m_menuColor(menuColor), m_options(std::move(options)),
      m_selectedIndex(m_options.empty() ? 0U : std::min(selectedIndex, m_options.size() - 1)) {
    m_position = position;
    m_size = size;
    m_geometryDirty = true;
}

const std::string& UIDropdown::getSelectedOption() const {
    static const std::string empty = "";
    if (m_options.empty()) {
        return empty;
    }
    return m_options[m_selectedIndex];
}

void UIDropdown::onRender() {
    if (m_geometryDirty) {
        rebuildGeometry();
        m_geometryDirty = false;
    }
}

bool UIDropdown::hitTest(const glm::vec2& point) {
    const glm::vec2 absolute = getAbsolutePosition();
    const float height = m_expanded ? m_size.y * static_cast<float>(m_options.size() + 1) : m_size.y;
    return point.x >= absolute.x && point.x <= absolute.x + m_size.x && point.y >= absolute.y &&
           point.y <= absolute.y + height;
}

int UIDropdown::optionIndexAtPoint(const glm::vec2& point) const {
    if (!m_expanded) {
        return -1;
    }

    const glm::vec2 absolute = getAbsolutePosition();
    if (point.y < absolute.y + m_size.y) {
        return -1;
    }

    const float optionHeight = m_size.y;
    const int index = static_cast<int>((point.y - (absolute.y + m_size.y)) / optionHeight);
    if (index < 0 || index >= static_cast<int>(m_options.size())) {
        return -1;
    }
    return index;
}

bool UIDropdown::onMouseButtonPressed(const MouseButtonEvent& event) {
    if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_PRESS) {
        return false;
    }

    const glm::vec2 mouse = InputManager::getInstance().getMousePosition();
    const glm::vec2 absolute = getAbsolutePosition();

    if (mouse.y <= absolute.y + m_size.y) {
        m_expanded = !m_expanded;
        m_geometryDirty = true;
        return true;
    }

    const int optionIndex = optionIndexAtPoint(mouse);
    if (optionIndex >= 0) {
        m_selectedIndex = static_cast<std::size_t>(optionIndex);
        m_expanded = false;
        m_geometryDirty = true;
        if (m_onSelectionChanged) {
            m_onSelectionChanged(m_selectedIndex, getSelectedOption());
        }
        return true;
    }

    m_expanded = false;
    m_geometryDirty = true;
    return true;
}

bool UIDropdown::onMouseMoved(const MouseMoveEvent& event) {
    const glm::vec2 point(static_cast<float>(event.x), static_cast<float>(event.y));
    const bool hovered = hitTest(point);
    if (hovered != m_hovered) {
        m_hovered = hovered;
        m_geometryDirty = true;
    }

    const int option = optionIndexAtPoint(point);
    if (option != m_hoveredOption) {
        m_hoveredOption = option;
        m_geometryDirty = true;
    }

    return hovered;
}

bool UIDropdown::onMouseScroll(const MouseScrollEvent& event) {
    if (!m_expanded || m_options.empty()) {
        return false;
    }

    if (event.yOffset > 0.0) {
        if (m_selectedIndex > 0) {
            --m_selectedIndex;
            m_geometryDirty = true;
            if (m_onSelectionChanged) {
                m_onSelectionChanged(m_selectedIndex, getSelectedOption());
            }
        }
    } else if (event.yOffset < 0.0) {
        if (m_selectedIndex + 1 < m_options.size()) {
            ++m_selectedIndex;
            m_geometryDirty = true;
            if (m_onSelectionChanged) {
                m_onSelectionChanged(m_selectedIndex, getSelectedOption());
            }
        }
    }

    return true;
}

void UIDropdown::setOnSelectionChanged(std::function<void(std::size_t, const std::string&)> callback) {
    m_onSelectionChanged = std::move(callback);
}

void UIDropdown::addRect(float x, float y, float w, float h, const glm::vec4& color) {
    const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back({glm::vec3(x, y, 0.0f), glm::vec2(0.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y, 0.0f), glm::vec2(1.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), glm::vec2(1.0f, 1.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x, y + h, 0.0f), glm::vec2(0.0f, 1.0f), color, 0.0f});
    m_indices.insert(m_indices.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void UIDropdown::rebuildGeometry() {
    m_vertices.clear();
    m_indices.clear();

    const glm::vec2 absolute = getAbsolutePosition();
    const float x = absolute.x;
    const float y = absolute.y;
    const float w = m_size.x;
    const float h = m_size.y;

    glm::vec4 header = m_headerColor;
    if (m_hovered) {
        header.r = std::min(header.r * 1.08f, 1.0f);
        header.g = std::min(header.g * 1.08f, 1.0f);
        header.b = std::min(header.b * 1.08f, 1.0f);
    }
    addRect(x, y, w, h, header);

    const std::string selected = m_options.empty() ? "<empty>" : m_options[m_selectedIndex];
    const std::string label = selected + (m_expanded ? "  ^" : "  v");
    const float textHeight = std::max(14.0f, h * 0.40f);
    const glm::vec2 textPos(x + 8.0f, y + (h - UIRenderer::getLineHeight(textHeight)) * 0.5f);
    UIRenderer::appendTextGeometry(label, textPos, textHeight, m_textColor, m_vertices, m_indices);

    if (!m_expanded || m_options.empty()) {
        return;
    }

    for (std::size_t i = 0; i < m_options.size(); ++i) {
        const float optionY = y + h * static_cast<float>(i + 1);
        glm::vec4 optionColor = m_menuColor;
        if (static_cast<int>(i) == m_hoveredOption) {
            optionColor.r = std::min(optionColor.r * 1.12f, 1.0f);
            optionColor.g = std::min(optionColor.g * 1.12f, 1.0f);
            optionColor.b = std::min(optionColor.b * 1.12f, 1.0f);
        }
        addRect(x, optionY, w, h, optionColor);

        const std::string optionText = (i == m_selectedIndex ? "* " : "  ") + m_options[i];
        const glm::vec2 optionTextPos(x + 8.0f, optionY + (h - UIRenderer::getLineHeight(textHeight)) * 0.5f);
        UIRenderer::appendTextGeometry(optionText, optionTextPos, textHeight, m_textColor, m_vertices, m_indices);
    }
}

} // namespace Valkron
