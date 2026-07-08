#pragma once
#include <vulkan/vulkan.h>
#include "VkRenderData.h"

class RenderPass {
public:
    bool init(VkRenderData& renderData);
    void cleanup(VkRenderData& renderData);
};
