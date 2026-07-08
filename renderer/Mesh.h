#pragma once
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include "VkRenderData.h"

class Mesh {
public:
    bool create(VkRenderData& renderData);
    void bind(VkCommandBuffer commandBuffer);
    void cleanup(VkRenderData& renderData);
    uint32_t getVertexCount() const { return mVertexCount; }

private:
    VkCommandBuffer beginSingleTimeCommands(VkRenderData& renderData);
    void endSingleTimeCommands(VkRenderData& renderData, VkCommandBuffer commandBuffer);

    VkBuffer mVertexBuffer = VK_NULL_HANDLE;
    VmaAllocation mVertexBufferAlloc = VK_NULL_HANDLE;
    uint32_t mVertexCount = 0;
};
