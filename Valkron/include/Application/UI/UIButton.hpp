#pragma once

#include "Core/Core.hpp"
#include "Application/UI/UIElement.hpp"

#include <array>
#include <string>

namespace Valkron {
    
    class VALKRON_API UIButton : public UIElement {
        public:
            UIButton(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color,
                     const std::string& label = "",
                     const std::string& debugName = "",
                     const std::array<glm::vec2, 4>& texCoords = {
                        glm::vec2(0.0f, 0.0f),
                        glm::vec2(1.0f, 0.0f),
                        glm::vec2(1.0f, 1.0f),
                        glm::vec2(0.0f, 1.0f)
                     });

            void onRender() override;
            bool hitTest(const glm::vec2& point) override;
            bool onMouseButtonPressed(const MouseButtonEvent& event) override;
            bool onMouseButtonReleased(const MouseButtonEvent& event) override;
            bool onMouseMoved(const MouseMoveEvent& event) override;

            void setPosition(const glm::vec2& position);
            void setSize(const glm::vec2& size);
            void setColor(const glm::vec4& color);
            void setDebugName(const std::string& debugName);
            void setLabel(const std::string& label);

        protected:
            void rebuildGeometry() override;

        private:
            glm::vec4 m_color;
            glm::vec4 m_textColor{0.96f, 0.96f, 0.96f, 1.0f};
            std::array<glm::vec2, 4> m_texCoords;
            std::string m_label;
            std::string m_debugName;
            bool m_hovered = false;
            bool m_pressed = false;
    };

}