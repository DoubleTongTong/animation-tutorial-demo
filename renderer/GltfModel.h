#pragma once
#include <string>
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include <tiny_gltf.h>
#include "Texture.h"
#include "VkRenderData.h"

#include "GltfNode.h"
#include "GltfAnimationClip.h"
#include "IKSolver.h"

// 顶点结构体，与我们的图形管线着色器输入布局完全一致 (location=0,1,2)
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
    float jointNum[4];
    float jointWeight[4];
};

struct JointIndices {
    uint16_t idx[4];
};

struct JointWeights {
    float w[4];
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
    size_t getClipSize() const { return mAnimClips.size(); }

    // 获取关节描述符集 (常规 LBS / 双四元数 DQS)
    VkDescriptorSet getJointDescriptorSetLBS() const { return mJointDescriptorSet; }
    VkDescriptorSet getJointDescriptorSetDQS() const { return mJointDQSDescriptorSet; }

    // 获取纹理描述符集
    VkDescriptorSet getTextureDescriptorSet() const { return mTex.getDescriptorSet(); }

    std::shared_ptr<GltfNode> buildInstanceNodeTree(std::vector<std::shared_ptr<GltfNode>>& nodeList);
    void buildInstanceNodeList(std::shared_ptr<GltfNode> node, std::vector<std::shared_ptr<GltfNode>>& nodeList);

    size_t getNodeCount() const { return mModel ? mModel->nodes.size() : 0; }
    const std::vector<glm::mat4>& getInverseBindMatrices() const { return mInverseBindMatrices; }
    const std::vector<int>& getNodeToJoint() const { return mNodeToJoint; }
    const std::vector<GltfAnimationClip>& getAnimClips() const { return mAnimClips; }

    void updateGPUJointBuffers(VkRenderData& renderData, const std::vector<glm::mat4>& allJointMatrices, const std::vector<glm::mat2x4>& allJointDualQuats);

    // 递归读取并更新节点 TRS 及全局矩阵
    void getNodeData(std::shared_ptr<GltfNode> node, const glm::mat4& parentMatrix);
    // 递归遍历并创建子节点结构
    void getNodes(std::shared_ptr<GltfNode> node);

private:
    // 创建 GPU 关节矩阵 SSBO 资源及描述符
    bool createJointSSBO(VkRenderData& renderData);
    // 创建 GPU 本地缓冲区并上传数据的辅助函数
    bool createGPUBuffer(VkRenderData& renderData, VkBufferUsageFlags usage, const void* data, VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation);

    std::shared_ptr<tinygltf::Model> mModel = nullptr;

    // 逆绑定矩阵数组
    std::vector<glm::mat4> mInverseBindMatrices;

    // 蒙皮所需的顶点备份与关节权重数组
    VkRenderData* mRenderDataPtr = nullptr;
    std::vector<Vertex> mOriginalVertices;
    std::vector<JointIndices> mJointVec;
    std::vector<JointWeights> mWeightVec;
    std::vector<int> mNodeToJoint;

    // Vulkan 顶点与索引缓冲区及其分配的显存
    VkBuffer mVertexBuffer = VK_NULL_HANDLE;
    VmaAllocation mVertexBufferAlloc = VK_NULL_HANDLE;
    VkBuffer mIndexBuffer = VK_NULL_HANDLE;
    VmaAllocation mIndexBufferAlloc = VK_NULL_HANDLE;

    // GPU 骨骼矩阵 SSBO 资源及描述符资源
    VkDescriptorPool mJointDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet mJointDescriptorSet = VK_NULL_HANDLE;
    VkBuffer mJointBuffer = VK_NULL_HANDLE;
    VmaAllocation mJointBufferAlloc = VK_NULL_HANDLE;

    VkDescriptorSet mJointDQSDescriptorSet = VK_NULL_HANDLE;
    VkBuffer mJointDQSBuffer = VK_NULL_HANDLE;
    VmaAllocation mJointDQSBufferAlloc = VK_NULL_HANDLE;

    uint32_t mVertexCount = 0;
    uint32_t mIndexCount = 0;

    // 模型附带的单个贴图
    Texture mTex{};

    // 模型文件路径，用于在更新骨骼时匹配动画
    std::string mModelPath;

    // 动画片段列表与扁平化节点列表
    std::vector<GltfAnimationClip> mAnimClips{};
    void getAnimations();
};
