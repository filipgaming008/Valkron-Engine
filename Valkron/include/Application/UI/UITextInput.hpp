#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <functional>
#include <string>

namespace Valkron {

enum class UITextInputMode { Any, Integer, Decimal };

class VALKRON_API UITextInput : public UIElement {
public:
    UITextInput(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color,
                const std::string& placeholder = "", UITextInputMode mode = UITextInputMode::Any,
                const std::string& initialValue = "");

    void onRender() override;
    bool hitTest(const glm::vec2& point) override;
    bool onMouseButtonPressed(const MouseButtonEvent& event) override;
    bool onMouseMoved(const MouseMoveEvent& event) override;
    bool onKeyPressed(const KeyEvent& event) override;
    bool onCharInput(const CharInputEvent& event) override;
    void onFocusChanged(bool focused) override;

    const std::string& getValue() const {
        return m_value;
    }
    void setValue(const std::string& value);

    void setOnTextChanged(std::function<void(const std::string&)> callback);
    void setOnSubmit(std::function<void(const std::string&)> callback);

protected:
    void rebuildGeometry() override;

private:
    bool isCharAllowed(unsigned int codepoint) const;
    void emitChanged();
    void addRect(float x, float y, float w, float h, const glm::vec4& color);

    glm::vec4 m_color;
    glm::vec4 m_textColor{0.95f, 0.95f, 0.95f, 1.0f};
    glm::vec4 m_placeholderColor{0.72f, 0.72f, 0.75f, 1.0f};
    std::string m_placeholder;
    std::string m_value;
    UITextInputMode m_mode = UITextInputMode::Any;
    bool m_hovered = false;
    bool m_focused = false;
    std::function<void(const std::string&)> m_onTextChanged;
    std::function<void(const std::string&)> m_onSubmit;
};

} // namespace Valkron
