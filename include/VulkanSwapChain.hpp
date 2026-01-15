#pragma once

#include "Core.hpp"
#include "Helpers.hpp"

class VulkanDevice;
class Window;

class VulkanSwapChain {
public:
    VulkanSwapChain(VulkanDevice& device, Window& window);
    ~VulkanSwapChain();

    // Delete copy constructor and assignment operator
    VulkanSwapChain(const VulkanSwapChain&) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;

    void recreate();
    void cleanup();

    VkSwapchainKHR getSwapChain() const { return swapChain; }
    VkFormat getImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getExtent() const { return swapChainExtent; }
    const std::vector<VkImageView>& getImageViews() const { return swapChainImageViews; }
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers; }
    size_t getImageCount() const { return swapChainImages.size(); }

    void createFramebuffers(VkRenderPass renderPass);

private:
    void createSwapChain();
    void createImageViews();

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    VulkanDevice& device;
    Window& window;
    
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
};
