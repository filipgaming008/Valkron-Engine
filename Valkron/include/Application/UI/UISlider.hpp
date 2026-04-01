#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <functional>
#include <string>

namespace Valkron {

class VALKRON_API UISlider : public UIElement {
public:
    UISlider(const glm::vec2& position, const glm::vec2& size, const glm::vec4& trackColor, const glm::vec4& fillColor,
             const std::string& label = "Slider", float value = 0.5f);

    void onRender() override;
    bool hitTest(const glm::vec2& point) override;
    bool onMouseButtonPressed(const MouseButtonEvent& event) override;
    bool onMouseButtonReleased(const MouseButtonEvent& event) override;
    bool onMouseMoved(const MouseMoveEvent& event) override;

    float getValue() const {
        return m_value;
    }
    void setValue(float value);
    void setOnValueChanged(std::function<void(float)> callback);

protected:
    void rebuildGeometry() override;

private:
    void updateValueFromMouse(float mouseX);
    void addRect(float x, float y, float w, float h, const glm::vec4& color);

    glm::vec4 m_trackColor;
    glm::vec4 m_fillColor;
    glm::vec4 m_textColor{0.95f, 0.95f, 0.95f, 1.0f};
    std::string m_label;
    float m_value = 0.5f;
    bool m_hovered = false;
    bool m_dragging = false;
    std::function<void(float)> m_onValueChanged;
};

} // namespace Valkron
