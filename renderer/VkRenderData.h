#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "vk_mem_alloc.h"
#include "VkBootstrap.h"



struct VkRenderData {
    VmaAllocator rdAllocator = VK_NULL_HANDLE;
    vkb::Instance rdVkbInstance{};
    vkb::Device rdVkbDevice{};
    vkb::Swapchain rdVkbSwapchain{};
    std::vector<VkImage> rdSwapchainImages;
    std::vector<VkImageView> rdSwapchainImageViews;
    std::vector<VkFramebuffer> rdFramebuffers;

    // Additional Vulkan fields needed to pass around
    VkSurfaceKHR rdSurface = VK_NULL_HANDLE;
    VkQueue rdGraphicsQueue = VK_NULL_HANDLE;

    // Synchronization objects
    std::vector<VkFence> rdRenderFences;
    std::vector<VkSemaphore> rdPresentSemaphores;
    std::vector<VkSemaphore> rdRenderSemaphores;

    // Command buffer and rendering pipeline objects
    VkCommandPool rdCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> rdCommandBuffers;
    VkRenderPass rdRenderpass = VK_NULL_HANDLE;
    VkPipeline rdPipeline = VK_NULL_HANDLE;
    VkPipeline rdPipelineDQS = VK_NULL_HANDLE;
    VkPipelineLayout rdPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rdTextureDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout rdJointDescriptorSetLayout = VK_NULL_HANDLE;

    // Depth buffer objects
    VkImage rdDepthImage = VK_NULL_HANDLE;
    VmaAllocation rdDepthImageAlloc = VK_NULL_HANDLE;
    VkImageView rdDepthImageView = VK_NULL_HANDLE;

    // ImGui specific dynamic data
    bool rdGPUDualQuatVertexSkinning = false;
    uint32_t rdTriangleCount = 0;
    float rdFrameTime = 0.0f;
    int rdFieldOfView = 90;
    float rdViewAzimuth = 320.0f;
    float rdViewElevation = -15.0f;
    int rdMoveForward = 0;
    int rdMoveRight = 0;
    int rdMoveUp = 0;
    float rdTickDiff = 0.0f;
    glm::vec3 rdCameraWorldPosition = glm::vec3(0.5f, 0.25f, 1.0f);

    // Animation control variables
    bool rdPlayAnimation = true;
    std::string rdClipName = "None";
    int rdAnimClip = 0;
    int rdAnimClipSize = 0;
    float rdAnimSpeed = 1.0f;
    float rdAnimTimePosition = 0.0f;
    float rdAnimEndTime = 0.0f;
    float rdAnimBlendFactor = 1.0f;

    bool rdCrossBlending = false;
    int rdCrossBlendDestAnimClip = 0;
    std::string rdCrossBlendDestClipName = "None";
    float rdAnimCrossBlendFactor = 0.0f;

    int rdModelNodeCount = 0;
    bool rdAdditiveBlending = false;
    int rdSkelSplitNode = 0;
    std::string rdSkelSplitNodeName = "None";
};
