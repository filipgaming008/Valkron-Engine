#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"
#include "Application/Layer.hpp"
#include "Application/UI/UIElement.hpp"
#include "Application/UI/UIManager.hpp"

namespace Valkron {

    class VALKRON_API UILayer : public Layer {
        private:
            UIManager m_uiManager;
        public:
            UILayer(int id) : Layer(id) {}
            ~UILayer() override = default;

            UILayer(const UILayer&) = delete;
            UILayer& operator=(const UILayer&) = delete;
            UILayer(UILayer&&) = delete;
            UILayer& operator=(UILayer&&) = delete;

            void onAttach() override;
            void onDetach() override;
            void onUpdate(float deltaTime) override;
            void onEvent(Event& event) override;
    };

}