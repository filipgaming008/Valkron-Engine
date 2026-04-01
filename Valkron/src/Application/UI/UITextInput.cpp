#include "Application/UI/UITextInput.hpp"

#include "Renderer/UIRenderer.hpp"
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cctype>

namespace Valkron {

UITextInput::UITextInput(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color,
                         const std::string& placeholder, UITextInputMode mode, const std::string& initialValue)
    : m_color(color), m_placeholder(placeholder), m_value(initialValue), m_mode(mode) {
    m_position = position;
    m_size = size;
    m_geometryDirty = true;
}

void UITextInput::onRender() {
    if (m_geometryDirty) {
        rebuildGeometry();
        m_geometryDirty = false;
    }
}

bool UITextInput::hitTest(const glm::vec2& point) {
    const glm::vec2 absolute = getAbsolutePosition();
    return point.x >= absolute.x && point.x <= absolute.x + m_size.x && point.y >= absolute.y &&
           point.y <= absolute.y + m_size.y;
}

bool UITextInput::onMouseButtonPressed(const MouseButtonEvent& event) {
    if (event.button != GLFW_MOUSE_BUTTON_LEFT || event.action != GLFW_PRESS) {
        return false;
    }

    return true;
}

void UITextInput::onFocusChanged(bool focused) {
    if (m_focused != focused) {
        m_focused = focused;
        m_geometryDirty = true;
    }
}

bool UITextInput::onMouseMoved(const MouseMoveEvent& event) {
    const bool hovered = hitTest(glm::vec2(static_cast<float>(event.x), static_cast<float>(event.y)));
    if (hovered != m_hovered) {
        m_hovered = hovered;
        m_geometryDirty = true;
    }
    return hovered;
}

bool UITextInput::onKeyPressed(const KeyEvent& event) {
    if (event.action != GLFW_PRESS && event.action != GLFW_REPEAT) {
        return false;
    }

    if (event.key == GLFW_KEY_BACKSPACE) {
        if (!m_value.empty()) {
            m_value.pop_back();
            emitChanged();
            m_geometryDirty = true;
        }
        return true;
    }

    if (event.key == GLFW_KEY_ENTER || event.key == GLFW_KEY_KP_ENTER) {
        if (m_onSubmit) {
            m_onSubmit(m_value);
        }
        return true;
    }

    return false;
}

bool UITextInput::isCharAllowed(unsigned int codepoint) const {
    if (codepoint < 32 || codepoint > 126) {
        return false;
    }

    const char ch = static_cast<char>(codepoint);
    if (m_mode == UITextInputMode::Any) {
        return true;
    }

    if (m_mode == UITextInputMode::Integer) {
        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return true;
        }
        return ch == '-' && m_value.empty();
    }

    if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
        return true;
    }
    if (ch == '.' && m_value.find('.') == std::string::npos) {
        return true;
    }
    return ch == '-' && m_value.empty();
}

bool UITextInput::onCharInput(const CharInputEvent& event) {
    unsigned int codepoint = event.codepoint;
    if (m_mode == UITextInputMode::Decimal && codepoint == static_cast<unsigned int>(',')) {
        codepoint = static_cast<unsigned int>('.');
    }

    if (!isCharAllowed(codepoint)) {
        return false;
    }

    m_value.push_back(static_cast<char>(codepoint));
    emitChanged();
    m_geometryDirty = true;
    return true;
}

void UITextInput::setValue(const std::string& value) {
    m_value = value;
    m_geometryDirty = true;
    emitChanged();
}

void UITextInput::setOnTextChanged(std::function<void(const std::string&)> callback) {
    m_onTextChanged = std::move(callback);
}

void UITextInput::setOnSubmit(std::function<void(const std::string&)> callback) {
    m_onSubmit = std::move(callback);
}

void UITextInput::emitChanged() {
    if (m_onTextChanged) {
        m_onTextChanged(m_value);
    }
}

void UITextInput::addRect(float x, float y, float w, float h, const glm::vec4& color) {
    const std::uint32_t base = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back({glm::vec3(x, y, 0.0f), glm::vec2(0.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y, 0.0f), glm::vec2(1.0f, 0.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x + w, y + h, 0.0f), glm::vec2(1.0f, 1.0f), color, 0.0f});
    m_vertices.push_back({glm::vec3(x, y + h, 0.0f), glm::vec2(0.0f, 1.0f), color, 0.0f});
    m_indices.insert(m_indices.end(), {base + 0, base + 1, base + 2, base + 2, base + 3, base + 0});
}

void UITextInput::rebuildGeometry() {
    m_vertices.clear();
    m_indices.clear();

    glm::vec4 fill = m_color;
    if (m_hovered) {
        fill.r = std::min(fill.r * 1.06f, 1.0f);
        fill.g = std::min(fill.g * 1.06f, 1.0f);
        fill.b = std::min(fill.b * 1.06f, 1.0f);
    }
    if (m_focused) {
        fill.r = std::min(fill.r * 1.08f, 1.0f);
        fill.g = std::min(fill.g * 1.08f, 1.0f);
        fill.b = std::min(fill.b * 1.08f, 1.0f);
    }

    const glm::vec2 absolute = getAbsolutePosition();
    const float x = absolute.x;
    const float y = absolute.y;
    const float w = m_size.x;
    const float h = m_size.y;

    addRect(x, y, w, h, fill);

    const glm::vec4 borderColor =
        m_focused ? glm::vec4(0.94f, 0.78f, 0.28f, 1.0f) : glm::vec4(0.32f, 0.35f, 0.40f, 1.0f);
    const float border = 2.0f;
    addRect(x, y, w, border, borderColor);
    addRect(x, y + h - border, w, border, borderColor);
    addRect(x, y, border, h, borderColor);
    addRect(x + w - border, y, border, h, borderColor);

    const std::string displayText = m_value.empty() ? m_placeholder : m_value;
    const glm::vec4 textColor = m_value.empty() ? m_placeholderColor : m_textColor;
    const float textPixelHeight = std::max(14.0f, h * 0.42f);
    const float textY = y + (h - UIRenderer::getLineHeight(textPixelHeight)) * 0.5f;
    UIRenderer::appendTextGeometry(displayText, glm::vec2(x + 8.0f, textY), textPixelHeight, textColor, m_vertices,
                                   m_indices);
}

} // namespace Valkron
