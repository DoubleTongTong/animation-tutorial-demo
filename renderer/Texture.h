#pragma once
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include "VkRenderData.h"
#include <string>

class Texture {
public:
    bool create(VkRenderData& renderData, const std::string& filename = "texture/awesomeface.png");
    bool initDescriptorSet(VkRenderData& renderData);
    void cleanup(VkRenderData& renderData);

private:
    VkCommandBuffer beginSingleTimeCommands(VkRenderData& renderData);
    void endSingleTimeCommands(VkRenderData& renderData, VkCommandBuffer commandBuffer);
    void transitionImageLayout(VkRenderData& renderData, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkRenderData& renderData, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkImage mTextureImage = VK_NULL_HANDLE;
    VmaAllocation mTextureImageAlloc = VK_NULL_HANDLE;
    VkImageView mTextureImageView = VK_NULL_HANDLE;
    VkSampler mTextureSampler = VK_NULL_HANDLE;
    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
};
