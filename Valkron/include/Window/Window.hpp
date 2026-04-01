#pragma once

#include "Core/Core.hpp"
#include "Core/Log.hpp"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

namespace Valkron {

struct EngineSettings;

class VALKRON_API Window {
private:
    GLFWwindow* m_window = nullptr;

public:
    explicit Window(const EngineSettings& settings);
    virtual ~Window();

    inline bool shouldClose() const {
        return m_window && glfwWindowShouldClose(m_window) != 0;
    }

    void Update();
    inline GLFWwindow* getWindow() const {
        return m_window;
    }
};

} // namespace Valkron