#pragma once

#include "Core/Core.hpp"
#include "Core/Log.hpp"
#include "Event/Event.hpp"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include <functional>
#include <unordered_map>
#include <vector>

namespace Valkron {

    class Layer;


    class VALKRON_API InputManager {
        public:
            static InputManager& getInstance() {
                static InputManager instance;
                return instance;
            }

            void init(GLFWwindow* window);
            void update();
            void shutdown();

            using EventCallback = std::function<void(Event&)>;
            void setEventCallback(EventCallback callback);

            void pushLayer(Layer* layer);
            void popLayer();
            void setLayerEnabled(Layer* layer, bool enabled);

            bool isKeyPressed(int key) const;
            bool isMouseButtonPressed(int button) const;
            glm::vec2 getMousePosition() const;
            glm::vec2 getMouseDelta() const;
            float getScrollDelta() const;

        private:
            InputManager() = default;
            ~InputManager() = default;

            void dispatchEvent(Event& event);

            static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
            static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
            static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
            static void glfwWindowCloseCallback(GLFWwindow* window);
            static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

            GLFWwindow* m_window = nullptr;

            std::unordered_map<int, bool> m_keyStates;
            std::unordered_map<int, bool> m_previousKeyStates;
            std::unordered_map<int, bool> m_mouseButtonStates;
            glm::vec2 m_mousePosition{0.0f};
            glm::vec2 m_previousMousePosition{0.0f};
            glm::vec2 m_mouseDelta{0.0f};
            float m_scrollDelta = 0.0f;

            std::vector<Layer*> m_layerStack;
            std::unordered_map<Layer*, bool> m_layerEnabled;
            EventCallback m_eventCallback = nullptr;
    };


}