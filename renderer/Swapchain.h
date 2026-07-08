#pragma once
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include "VkRenderData.h"

class Swapchain {
public:
    bool init(VkRenderData& renderData, GLFWwindow* window);
    bool recreate(VkRenderData& renderData, GLFWwindow* window);
    void cleanup(VkRenderData& renderData);
};
