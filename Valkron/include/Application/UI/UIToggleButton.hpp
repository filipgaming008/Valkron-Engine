#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <array>
#include <functional>
#include <string>

namespace Valkron {

class VALKRON_API UIToggleButton : public UIElement {
public:
    UIToggleButton(const glm::vec2& position, const glm::vec2& size, const glm::vec4& offColor,
                   const glm::vec4& onColor, const std::string& label = "Toggle", bool initialState = false,
                   const std::array<glm::vec2, 4>& texCoords = {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f),
                                                                glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)});

    void onRender() override;
    bool hitTest(const glm::vec2& point) override;
    bool onMouseButtonPressed(const MouseButtonEvent& event) override;
    bool onMouseMoved(const MouseMoveEvent& event) override;

    bool isToggled() const {
        return m_toggled;
    }
    void setToggled(bool toggled);
    void setOnToggleChanged(std::function<void(bool)> callback);

protected:
    void rebuildGeometry() override;

private:
    glm::vec4 m_offColor;
    glm::vec4 m_onColor;
    glm::vec4 m_textColor{0.96f, 0.96f, 0.96f, 1.0f};
    std::array<glm::vec2, 4> m_texCoords;
    std::string m_label;
    bool m_toggled = false;
    bool m_hovered = false;
    std::function<void(bool)> m_onToggleChanged;
};

} // namespace Valkron
