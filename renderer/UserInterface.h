#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "VkRenderData.h"
#include <glm/glm.hpp>

class UserInterface {
public:
    void init(VkRenderData &renderData, GLFWwindow* window);
    void createFrame(VkRenderData &renderData, const glm::mat4& viewProj = glm::mat4(1.0f));
    void render(VkCommandBuffer commandBuffer);
    void cleanup(VkRenderData &renderData);

private:
    VkDescriptorPool mImguiDescriptorPool = VK_NULL_HANDLE;
    float framesPerSecond = 0.0f;
    float averagingAlpha = 0.96f;
};
