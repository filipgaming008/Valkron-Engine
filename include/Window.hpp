#pragma once

#include <GLFW/glfw3.h>

#include <string>
#include <functional>

class Window {
public:

    Window();
    ~Window();

    int init(int width, int height, const std::string& title);

    bool shouldClose() const;
    void pollEvents() const;
    GLFWwindow* getWindow() const { return window; }
    
    // Set callback for resize events
    void setResizeCallback(std::function<void(int, int)> callback);
    
    int getWidth() const;
    int getHeight() const;
    void getFramebufferSize(int& width, int& height) const;

private:
    GLFWwindow* window;
    int width;
    int height;
    std::string title;
    std::function<void(int, int)> resizeCallback;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
};