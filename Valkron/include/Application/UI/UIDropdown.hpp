#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Valkron {

class VALKRON_API UIDropdown : public UIElement {
public:
    UIDropdown(const glm::vec2& position, const glm::vec2& size, const glm::vec4& headerColor,
               const glm::vec4& menuColor, std::vector<std::string> options, std::size_t selectedIndex = 0);

    void onRender() override;
    bool hitTest(const glm::vec2& point) override;
    bool onMouseButtonPressed(const MouseButtonEvent& event) override;
    bool onMouseMoved(const MouseMoveEvent& event) override;
    bool onMouseScroll(const MouseScrollEvent& event) override;

    std::size_t getSelectedIndex() const {
        return m_selectedIndex;
    }
    const std::string& getSelectedOption() const;
    void setOnSelectionChanged(std::function<void(std::size_t, const std::string&)> callback);

protected:
    void rebuildGeometry() override;

private:
    void addRect(float x, float y, float w, float h, const glm::vec4& color);
    int optionIndexAtPoint(const glm::vec2& point) const;

    glm::vec4 m_headerColor;
    glm::vec4 m_menuColor;
    glm::vec4 m_textColor{0.94f, 0.94f, 0.96f, 1.0f};
    std::vector<std::string> m_options;
    std::size_t m_selectedIndex = 0;
    bool m_expanded = false;
    bool m_hovered = false;
    int m_hoveredOption = -1;
    std::function<void(std::size_t, const std::string&)> m_onSelectionChanged;
};

} // namespace Valkron
