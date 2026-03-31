#include "Application/UI/UIButton.hpp"

#include "Core/Log.hpp"
#include "Renderer/UIRenderer.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <string>

namespace Valkron {

    UIButton::UIButton(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color,
                       const std::string& label,
                       const std::string& debugName,
                       const std::array<glm::vec2, 4>& texCoords)
        : m_color(color)
        , m_texCoords(texCoords)
        , m_label(label)
        , m_debugName(debugName) {
        m_position = position;
        m_size = size;
        m_geometryDirty = true;

        if (m_debugName.empty()) {
            m_debugName = m_label;
        }
    }

    void UIButton::onRender() {
        if (m_geometryDirty) {
            rebuildGeometry();
            m_geometryDirty = false;
        }
    }

    bool UIButton::hitTest(const glm::vec2& point) {
        const float minX = m_position.x;
        const float minY = m_position.y;
        const float maxX = m_position.x + m_size.x;
        const float maxY = m_position.y + m_size.y;

        return point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY;
    }

    bool UIButton::onMouseButtonPressed(const MouseButtonEvent& event) {
        if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_PRESS) {
            return false;
        }

        m_pressed = true;
        m_geometryDirty = true;
        LOG_DEBUG("UIButton pressed: " + (m_debugName.empty() ? "<unnamed>" : m_debugName));
        return true;
    }

    bool UIButton::onMouseButtonReleased(const MouseButtonEvent& event) {
        if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_RELEASE) {
            return false;
        }

        if (!m_pressed) {
            return false;
        }

        m_pressed = false;
        m_geometryDirty = true;
        LOG_DEBUG("UIButton released: " + (m_debugName.empty() ? "<unnamed>" : m_debugName));
        return true;
    }

    bool UIButton::onMouseMoved(const MouseMoveEvent& event) {
        const bool hovered = hitTest(glm::vec2(static_cast<float>(event.x), static_cast<float>(event.y)));
        if (hovered != m_hovered) {
            m_hovered = hovered;
            m_geometryDirty = true;
        }

        return hovered;
    }

    void UIButton::setPosition(const glm::vec2& position) {
        m_position = position;
        m_geometryDirty = true;
    }

    void UIButton::setSize(const glm::vec2& size) {
        m_size = size;
        m_geometryDirty = true;
    }

    void UIButton::setColor(const glm::vec4& color) {
        m_color = color;
        m_geometryDirty = true;
    }

    void UIButton::setDebugName(const std::string& debugName) {
        m_debugName = debugName;
    }

    void UIButton::setLabel(const std::string& label) {
        m_label = label;
        m_geometryDirty = true;
    }

    void UIButton::rebuildGeometry() {
        m_vertices.clear();
        m_indices.clear();

        glm::vec4 fillColor = m_color;
        if (m_pressed) {
            fillColor.r *= 0.72f;
            fillColor.g *= 0.72f;
            fillColor.b *= 0.72f;
        } else if (m_hovered) {
            fillColor.r = std::min(fillColor.r * 1.18f, 1.0f);
            fillColor.g = std::min(fillColor.g * 1.18f, 1.0f);
            fillColor.b = std::min(fillColor.b * 1.18f, 1.0f);
        }

        m_vertices.reserve(4 + m_label.size() * 4);
        m_indices.reserve(6 + m_label.size() * 6);

        const float x = m_position.x;
        const float y = m_position.y;
        const float w = m_size.x;
        const float h = m_size.y;

        m_vertices.push_back({glm::vec3(x, y, 0.0f), m_texCoords[0], fillColor, 0.0f});
        m_vertices.push_back({glm::vec3(x + w, y, 0.0f), m_texCoords[1], fillColor, 0.0f});
        m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), m_texCoords[2], fillColor, 0.0f});
        m_vertices.push_back({glm::vec3(x, y + h, 0.0f), m_texCoords[3], fillColor, 0.0f});

        m_indices = {0, 1, 2, 2, 3, 0};

        if (!m_label.empty()) {
            const float textPixelHeight = std::max(18.0f, h * 0.42f);
            const float textWidth = UIRenderer::measureTextWidth(m_label, textPixelHeight);
            const float textHeight = UIRenderer::getLineHeight(textPixelHeight);

            const glm::vec2 textPos(
                x + (w - textWidth) * 0.5f,
                y + (h - textHeight) * 0.5f
            );

            UIRenderer::appendTextGeometry(m_label, textPos, textPixelHeight, m_textColor, m_vertices, m_indices);
        }
    }

}
