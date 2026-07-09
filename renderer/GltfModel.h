#pragma once
#include <string>
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include <tiny_gltf.h>
#include "Texture.h"
#include "VkRenderData.h"

// 顶点结构体，与我们的图形管线着色器输入布局完全一致 (location=0,1,2)
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

class GltfModel {
public:
    // 加载 glTF 模型并创建 Vulkan 缓冲与内嵌纹理
    bool loadModel(VkRenderData& renderData, const std::string& filename);
    // 在命令缓冲中录制绘制指令
    void draw(VkCommandBuffer commandBuffer);
    // 清理创建的资源
    void cleanup(VkRenderData& renderData);

    // 获取模型的三角形总数
    uint32_t getTriangleCount() const { return mIndexCount / 3; }

private:
    // 创建 GPU 本地缓冲区并上传数据的辅助函数
    bool createGPUBuffer(VkRenderData& renderData, VkBufferUsageFlags usage, const void* data, VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation);

    std::shared_ptr<tinygltf::Model> mModel = nullptr;

    // Vulkan 顶点与索引缓冲区及其分配的显存
    VkBuffer mVertexBuffer = VK_NULL_HANDLE;
    VmaAllocation mVertexBufferAlloc = VK_NULL_HANDLE;
    VkBuffer mIndexBuffer = VK_NULL_HANDLE;
    VmaAllocation mIndexBufferAlloc = VK_NULL_HANDLE;

    uint32_t mVertexCount = 0;
    uint32_t mIndexCount = 0;

    // 模型附带的单个贴图
    Texture mTex{};
};
