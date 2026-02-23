#pragma once

#include "Core.hpp"

namespace Valkron {

    class VALKRON_API Engine {
        private:

        public:
            Engine() = default;
            virtual ~Engine() = default;

            static void Init();
    };

}
