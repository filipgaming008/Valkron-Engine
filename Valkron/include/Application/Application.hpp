#pragma once

#include "Core/Core.hpp"
#include "Event/Event.hpp"
#include "Window/Window.hpp"
#include "Input/InputManager.hpp"
#include "Application/Layer.hpp"
#include "Application/UILayer.hpp"
#include "Core/Log.hpp"
#include <memory>

namespace Valkron {

    class VALKRON_API Application {
        private:
            bool isRunning = true;
            std::unique_ptr<Window> window = nullptr;
            InputManager& m_inputManager = InputManager::getInstance();
            UILayer m_layer{0};

        public:
            Application();
            virtual ~Application();

            void Run();

        private:
            void onEvent(Event& event);
    };

}