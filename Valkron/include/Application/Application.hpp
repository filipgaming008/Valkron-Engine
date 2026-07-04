#pragma once

#include "Core/Core.hpp"
#include "Core/Log.hpp"
#include "Window/Window.hpp"
#include "Input/InputManager.hpp"
#include "Renderer/Renderer.hpp"
#include "Application/Layer.hpp"
#include "Scene/Scene.hpp"

#include <memory>
#include <vector>

namespace Valkron {

    class VALKRON_API Application {
        public:
            Application();
            virtual ~Application();

            int run();

            Window* getWindow() const { return m_window.get(); }
            InputManager& getInputManager() { return m_inputManager; }
            bool isRunning() const { return m_isRunning; }
            void quit() { m_isRunning = false; }

            void pushLayer(Layer* layer);
            void popLayer();

        protected:
            virtual void onInit() {}
            virtual void onShutdown() {}
            virtual void onUpdate(float deltaTime) { (void)deltaTime; }
            virtual void onRender() {}
            virtual void onEvent(Event& event);
            virtual void onWindowResize(int width, int height);

            void updateLayers(float deltaTime);
            void renderLayers();
            Scene* getScene() { return m_scene.get(); }

        private:
            bool m_isRunning = true;
            std::unique_ptr<Window> m_window;
            InputManager& m_inputManager = InputManager::getInstance();
            std::vector<Layer*> m_layers;
            std::unique_ptr<Scene> m_scene;
    };

}