#pragma once

#include "Core.hpp"
#include "GLFW/glfw3.h"
#include "glm/glm.hpp"
#include "Layer.hpp"

namespace Valkron {


    class VALKRON_API InputManager{
        public:
            static InputManager& getInstance() {
                static InputManager instance;
                return instance;
            }

            void init(GLFWwindow* window);
            void update();
            void shutdown();

            void pushLayer(int layerId);
            void popLayer();
            void setLayerEnabled(int layerId, bool enabled);

            bool isKeyPressed(int key) const;
            bool isMouseButtonPressed(int button) const;
            glm::vec2 getMousePosition() const;
            glm::vec2 getMouseDelta() const;
            float getScrollDelta() const;

            using KeyCallback = std::function<void(int key, int scancode, int action, int mods)>;
            using MouseButtonCallback = std::function<void(int button, int action, int mods)>;
            using MouseMoveCallback = std::function<void(double x, double y)>;
            using ScrollCallback = std::function<void(double xOffset, double yOffset)>;

            void registerKeyCallback(int layerId, KeyCallback callback);
            void registerMouseButtonCallback(int layerId, MouseButtonCallback callback);
            void registerMouseMoveCallback(int layerId, MouseMoveCallback callback);
            void registerScrollCallback(int layerId, ScrollCallback callback);

        private:
            InputManager() = default;
            ~InputManager() = default;

            static void glfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
            static void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
            static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos);
            static void glfwScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

            GLFWwindow* m_window = nullptr;

            std::unordered_map<int, bool> m_keyStates;
            std::unordered_map<int, bool> m_previousKeyStates;
            std::unordered_map<int, bool> m_mouseButtonStates;
            glm::vec2 m_mousePosition{0.0f};
            glm::vec2 m_previousMousePosition{0.0f};
            glm::vec2 m_mouseDelta{0.0f};
            float m_scrollDelta = 0.0f;

            std::vector<int> m_layerStack;
            std::unordered_map<int, bool> m_layerEnabled;

            std::unordered_map<int, std::vector<KeyCallback>> m_keyCallbacks;
            std::unordered_map<int, std::vector<MouseButtonCallback>> m_mouseButtonCallbacks;
            std::unordered_map<int, std::vector<MouseMoveCallback>> m_mouseMoveCallbacks;
            std::unordered_map<int, std::vector<ScrollCallback>> m_scrollCallbacks;
    };


}