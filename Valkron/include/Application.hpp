#pragma once

#include "Core.hpp"
#include "Window.hpp"
#include "InputManager.hpp"
#include "Layer.hpp"
#include "UILayer.hpp"
#include "Log.hpp"
#include <memory>

namespace Valkron {

    class VALKRON_API Application {
        private:
            bool isRunning = true;
            std::unique_ptr<Window> m_window = nullptr;
            InputManager& m_inputManager = InputManager::getInstance();
            UILayer m_layer{0};

        public:
            Application();
            virtual ~Application();

            void Run();
    };

}