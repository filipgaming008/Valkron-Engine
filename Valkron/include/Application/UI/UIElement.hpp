#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"

#include "glm/glm.hpp"

namespace Valkron {

    struct UIVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec4 color;
    };

    class VALKRON_API UIElement {
        public:
            virtual ~UIElement() = default;
            virtual void onRender() = 0;

            virtual bool hitTest(const glm::vec2& point) = 0;

            virtual bool onMouseButtonPressed(const MouseButtonEvent& e) { return false; }
            virtual bool onMouseButtonReleased(const MouseButtonEvent& e) { return false; }
            virtual bool onMouseMoved(const MouseMoveEvent& e) { return false; }

            int getLayerID() const { return m_layerId; }
            void setLayerID(int id) { m_layerId = id; }
        protected:
            int m_layerId = 0;
            glm::vec2 m_position;
            glm::vec2 m_size;
    };

}