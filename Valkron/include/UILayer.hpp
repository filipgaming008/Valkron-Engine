#pragma once

#include "Core.hpp"
#include "InputManager.hpp"
#include "Layer.hpp"

namespace Valkron {

    class VALKRON_API UILayer : public Layer {
        public:
            UILayer(int id) : Layer(id) {}
            ~UILayer() override = default;

            void onAttach() override;
            void onDetach() override;
            void onUpdate(float deltaTime) override;
    };

}