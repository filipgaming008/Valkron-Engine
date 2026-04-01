#pragma once

#include "Core/Core.hpp"
#include "Core/Log.hpp"
#include "Engine/EngineConfig.hpp"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#include <string>
#include <utility>

namespace Valkron {


    class VALKRON_API Window {
        private:
            GLFWwindow* m_window = nullptr;
            bool m_isFullscreen = false;
            bool m_isDockedFullscreenWindowed = false;
            int m_windowedX = 100;
            int m_windowedY = 100;
            int m_windowedWidth = 1280;
            int m_windowedHeight = 720;
            int m_windowWidth = 1280;
            int m_windowHeight = 720;
            std::string m_windowTitle = "Valkron Engine";
        public:
            explicit Window(const EngineSettings& settings = EngineSettings{});
            virtual ~Window();

            inline bool shouldClose() const {
                return m_window && glfwWindowShouldClose(m_window) != 0;
            }

            void Update();
            void applySettings(const EngineSettings& settings);
            EngineSettings getCurrentSettings() const;
            std::pair<int, int> getPrimaryMonitorSize() const;
            void getFramebufferSize(int& width, int& height) const;
            bool isFullscreen() const { return m_isFullscreen; }
            bool isDockedFullscreenWindowed() const { return m_isDockedFullscreenWindowed; }
            inline GLFWwindow* getWindow() const { return m_window; }
    };


}