#pragma once

#include "Core.hpp"
#include <string>

namespace Valkron {


    class VALKRON_API Layer {
        protected: 
            int LayerId;
        public:
            explicit Layer(int layerId) : LayerId(layerId) {}
            virtual ~Layer() = default;

            virtual void OnAttach() {}
            virtual void OnDetach() {}
            virtual void OnUpdate() {}
            virtual void OnEvent() {}

            int getLayerId() const { return LayerId; }
            void setEnabled(bool enabled) {  }
    };


}