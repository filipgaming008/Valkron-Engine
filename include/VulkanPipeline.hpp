#pragma once

#include "Core.hpp"
#include "Helpers.hpp"

class VulkanDevice;
class VulkanSwapChain;

class VulkanPipeline {
public:
    VulkanPipeline(VulkanDevice& device, VulkanSwapChain& swapChain);
    ~VulkanPipeline();

    // Delete copy constructor and assignment operator
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;

    VkRenderPass getRenderPass() const { return renderPass; }
    VkPipeline getPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }

private:
    void createRenderPass();
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code);

    VulkanDevice& device;
    VulkanSwapChain& swapChain;
    
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
};
