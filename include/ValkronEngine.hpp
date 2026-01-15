#pragma once

#include "Core.hpp"
#include "Window.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanRenderer.hpp"

#include <memory>

class ValkronEngine {
public:
    ValkronEngine();
    ~ValkronEngine();

    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    Window window;
    std::unique_ptr<VulkanDevice> vulkanDevice;
    std::unique_ptr<VulkanSwapChain> vulkanSwapChain;
    std::unique_ptr<VulkanPipeline> vulkanPipeline;
    std::unique_ptr<VulkanRenderer> vulkanRenderer;
};
