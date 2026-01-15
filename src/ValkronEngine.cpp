#include "ValkronEngine.hpp"

ValkronEngine::ValkronEngine() {
}

ValkronEngine::~ValkronEngine() {
}

void ValkronEngine::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void ValkronEngine::initWindow() {
    window.init(WIDTH, HEIGHT, "Valkron Engine");
    window.setResizeCallback([this](int width, int height) {
        if (vulkanRenderer) {
            vulkanRenderer->setFramebufferResized(true);
        }
    });
}

void ValkronEngine::initVulkan() {
    vulkanDevice = std::make_unique<VulkanDevice>(window);
    vulkanSwapChain = std::make_unique<VulkanSwapChain>(*vulkanDevice, window);
    vulkanPipeline = std::make_unique<VulkanPipeline>(*vulkanDevice, *vulkanSwapChain);
    vulkanSwapChain->createFramebuffers(vulkanPipeline->getRenderPass());
    vulkanRenderer = std::make_unique<VulkanRenderer>(*vulkanDevice, *vulkanSwapChain, *vulkanPipeline);
}

void ValkronEngine::mainLoop() {
    while (!window.shouldClose()) {
        window.pollEvents();
        vulkanRenderer->drawFrame();
    }

    vulkanRenderer->waitIdle();
}

void ValkronEngine::cleanup() {
    // Cleanup is handled by destructors of unique_ptr members
    vulkanRenderer.reset();
    vulkanPipeline.reset();
    vulkanSwapChain.reset();
    vulkanDevice.reset();
    
    glfwTerminate();
}
