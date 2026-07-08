#pragma once
#include <vulkan/vulkan.h>
#include "VkRenderData.h"

class Framebuffer {
public:
    bool init(VkRenderData& renderData);
    void cleanup(VkRenderData& renderData);
};
