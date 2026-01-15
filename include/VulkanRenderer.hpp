#pragma once

#include "Core.hpp"

class VulkanDevice;
class VulkanSwapChain;
class VulkanPipeline;

class VulkanRenderer {
public:
    VulkanRenderer(VulkanDevice& device, VulkanSwapChain& swapChain, VulkanPipeline& pipeline);
    ~VulkanRenderer();

    // Delete copy constructor and assignment operator
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    void drawFrame();
    void waitIdle();
    bool getFramebufferResized() const { return framebufferResized; }
    void setFramebufferResized(bool resized) { framebufferResized = resized; }

private:
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    VulkanDevice& device;
    VulkanSwapChain& swapChain;
    VulkanPipeline& pipeline;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    bool framebufferResized = false;
};
