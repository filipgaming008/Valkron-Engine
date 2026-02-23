#pragma once

#include "Core.hpp"
#include "Window.hpp"
#include "Log.hpp"
#include <memory>

namespace Valkron {

    class VALKRON_API Application {
        private:
            bool isRunning = true;
            std::unique_ptr<Window> m_window = nullptr;
        public:
            Application();
            virtual ~Application();

            void Run();
    };

}