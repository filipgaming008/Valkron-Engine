#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"
#include "Application/Layer.hpp"

namespace Valkron {

    class VALKRON_API UILayer : public Layer {
        public:
            UILayer(int id) : Layer(id) {}
            ~UILayer() override = default;

            void onAttach() override;
            void onDetach() override;
            void onUpdate(float deltaTime) override;
            void onEvent(Event& event) override;
    };

}