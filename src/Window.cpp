#include "Window.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

Window::Window() : window(nullptr), width(0), height(0), title("") {}

Window::~Window() {
    if (window) {
        glfwDestroyWindow(window);
    }
}

int Window::init(int width, int height, const std::string& title) {
    this->width = width;
    this->height = height;
    this->title = title;

    #ifdef PLATFORM_LINUX
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    #endif

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    
    return 0;
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window);
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::setResizeCallback(std::function<void(int, int)> callback) {
    resizeCallback = std::move(callback);
}

int Window::getWidth() const {
    return width;
}

int Window::getHeight() const {
    return height;
}

void Window::getFramebufferSize(int& width, int& height) const {
    glfwGetFramebufferSize(window, &width, &height);
}

void Window::framebufferResizeCallback(GLFWwindow* glfwWindow, int width, int height) {
    auto* window = reinterpret_cast<Window*>(glfwGetWindowUserPointer(glfwWindow));
    if (window && window->resizeCallback) {
        window->resizeCallback(width, height);
    }
}