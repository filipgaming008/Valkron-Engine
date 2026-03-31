// UIManager.hpp
#pragma once

#include "Application/UI/UIElement.hpp"
#include "Event/Event.hpp"

#include "glm/glm.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace Valkron {

    class UIManager {
        public:
            void addElement(std::unique_ptr<UIElement> element);
            void removeAll();

            void render();
            void buildDrawData(std::vector<UIVertex>& outVertices, std::vector<std::uint32_t>& outIndices);
            bool handleEvent(Event& e);

        private:
            std::vector<std::unique_ptr<UIElement>> m_elements;
            UIElement* m_hovered = nullptr;
            UIElement* m_pressed = nullptr;
            glm::vec2 m_lastMousePosition{0.0f, 0.0f};
    };

}