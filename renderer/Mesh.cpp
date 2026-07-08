#include "Mesh.h"
#include "Logger.h"
#include <cstring>

bool Mesh::create(VkRenderData& renderData) {
    struct Vertex {
        float pos[3];
        float uv[2];
    };

    Vertex vertices[] = {
        // Front Face (z = 0.5)
        {{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }},

        // Back Face (z = -0.5)
        {{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{  0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }},

        // Left Face (x = -0.5)
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { 1.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }},

        // Right Face (x = 0.5)
        {{  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 0.0f, 1.0f }},

        // Top Face (y = 0.5)
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f,  0.5f }, { 1.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f,  0.5f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f, -0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, -0.5f }, { 0.0f, 1.0f }},

        // Bottom Face (y = -0.5)
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, -0.5f }, { 1.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f, -0.5f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f,  0.5f }, { 1.0f, 1.0f }},
        {{ -0.5f, -0.5f,  0.5f }, { 0.0f, 1.0f }}
    };

    mVertexCount = sizeof(vertices) / sizeof(vertices[0]);
    VkDeviceSize bufferSize = sizeof(vertices);

    // 1. Create Staging Buffer (CPU mapped only)
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAlloc = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(renderData.rdAllocator, &stagingBufferInfo, &stagingAllocInfo,
                        &stagingBuffer, &stagingBufferAlloc, nullptr) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create staging buffer\n", __FUNCTION__);
        return false;
    }

    // Copy vertex data to Staging Buffer
    void* data;
    if (vmaMapMemory(renderData.rdAllocator, stagingBufferAlloc, &data) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to map staging buffer memory\n", __FUNCTION__);
        vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);
        return false;
    }
    std::memcpy(data, vertices, bufferSize);
    vmaUnmapMemory(renderData.rdAllocator, stagingBufferAlloc);

    // 2. Create GPU-only Device Local Vertex Buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(renderData.rdAllocator, &bufferInfo, &allocInfo,
                        &mVertexBuffer, &mVertexBufferAlloc, nullptr) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create GPU vertex buffer\n", __FUNCTION__);
        vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);
        return false;
    }

    // 3. Copy Staging Buffer to GPU-only Buffer
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(renderData);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, mVertexBuffer, 1, &copyRegion);

    endSingleTimeCommands(renderData, commandBuffer);

    // Destroy staging buffer
    vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);

    return true;
}

void Mesh::bind(VkCommandBuffer commandBuffer) {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertexBuffer, &offset);
}

void Mesh::cleanup(VkRenderData& renderData) {
    if (mVertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mVertexBuffer, mVertexBufferAlloc);
        mVertexBuffer = VK_NULL_HANDLE;
        mVertexBufferAlloc = VK_NULL_HANDLE;
    }
    mVertexCount = 0;
}

VkCommandBuffer Mesh::beginSingleTimeCommands(VkRenderData& renderData) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = renderData.rdCommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(renderData.rdVkbDevice.device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void Mesh::endSingleTimeCommands(VkRenderData& renderData, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(renderData.rdGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderData.rdGraphicsQueue);
    vkFreeCommandBuffers(renderData.rdVkbDevice.device, renderData.rdCommandPool, 1, &commandBuffer);
}
