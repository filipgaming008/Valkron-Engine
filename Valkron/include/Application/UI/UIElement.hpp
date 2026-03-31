#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <vector>

namespace Valkron {

    struct UIVertex {
        glm::vec3 position;
        glm::vec2 texCoord;
        glm::vec4 color;
        float sdfMode;
    };

    class VALKRON_API UIElement {
        protected:
            int m_layerId = 0;
            glm::vec2 m_position;
            glm::vec2 m_size;
            std::vector<UIVertex> m_vertices;
            std::vector<std::uint32_t> m_indices;
            bool m_geometryDirty = true;
            
        public:
            virtual ~UIElement() = default;
            virtual void onRender() = 0;

            virtual bool  hitTest(const glm::vec2& point) = 0;

            virtual bool onMouseButtonPressed(const MouseButtonEvent&) { return false; }
            virtual bool onMouseButtonReleased(const MouseButtonEvent&) { return false; }
            virtual bool onMouseMoved(const MouseMoveEvent&) { return false; }

            const std::vector<UIVertex>& getVertices() const { return m_vertices; }
            const std::vector<std::uint32_t>& getIndices() const { return m_indices; }

            int getLayerID() const { return m_layerId; }
            void setLayerID(int id) { m_layerId = id; }

        protected:
            virtual void rebuildGeometry() = 0;
    };

}