#pragma once

#include "Core.hpp"
#include "InputManager.hpp"
#include <string>

namespace Valkron {


    class VALKRON_API Layer {
        protected: 
            int m_layerId;
        public:
            explicit Layer(int id) : m_layerId(id) {}
            virtual ~Layer() = default;
            
            virtual void onAttach() {}
            virtual void onDetach() {}
            virtual void onUpdate(float deltaTime) {}
            virtual void onEvent() {}
            
            int getLayerId() const { return m_layerId; }
    };


}