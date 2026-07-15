#include "GltfModel.h"
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include "Logger.h"
#include <cstring>
#include <cassert>

bool GltfModel::loadModel(VkRenderData& renderData, const std::string& filename) {
    mModelPath = filename;
    mModel = std::make_shared<tinygltf::Model>();
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    // 加载 ASCII 格式的 glTF 模型文件
    bool res = loader.LoadASCIIFromFile(mModel.get(), &err, &warn, filename);

    if (!warn.empty()) {
        Logger::log(1, "加载 glTF 模型警告: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        Logger::log(1, "加载 glTF 模型错误: %s\n", err.c_str());
    }

    if (!res) {
        Logger::log(1, "无法加载文件: %s\n", filename.c_str());
        return false;
    }

    // 简便处理：假设模型中只有单个 Mesh 和单个 Primitive
    if (mModel->meshes.empty() || mModel->meshes[0].primitives.empty()) {
        Logger::log(1, "模型中不包含有效的 mesh 或 primitive\n");
        return false;
    }

    const tinygltf::Primitive& primitive = mModel->meshes[0].primitives[0];

    // 从图元属性映射中寻找顶点位置、法线与纹理坐标的索引
    auto posIt = primitive.attributes.find("POSITION");
    auto normIt = primitive.attributes.find("NORMAL");
    auto uvIt = primitive.attributes.find("TEXCOORD_0");

    if (posIt == primitive.attributes.end()) {
        Logger::log(1, "模型不包含顶点位置数据\n");
        return false;
    }

    // 获取并设定顶点位置数据读取源
    const tinygltf::Accessor& posAccessor = mModel->accessors[posIt->second];
    const tinygltf::BufferView& posView = mModel->bufferViews[posAccessor.bufferView];
    const tinygltf::Buffer& posBuffer = mModel->buffers[posView.buffer];
    int posStride = posAccessor.ByteStride(posView);
    mVertexCount = static_cast<uint32_t>(posAccessor.count);

    // 获取法线数据读取源 (如果存在)
    bool hasNormal = (normIt != primitive.attributes.end());
    const tinygltf::Accessor* normAccessor = nullptr;
    const tinygltf::BufferView* normView = nullptr;
    const tinygltf::Buffer* normBuffer = nullptr;
    int normStride = 0;
    if (hasNormal) {
        normAccessor = &mModel->accessors[normIt->second];
        normView = &mModel->bufferViews[normAccessor->bufferView];
        normBuffer = &mModel->buffers[normView->buffer];
        normStride = normAccessor->ByteStride(*normView);
    }

    // 获取纹理坐标数据读取源 (如果存在)
    bool hasUV = (uvIt != primitive.attributes.end());
    const tinygltf::Accessor* uvAccessor = nullptr;
    const tinygltf::BufferView* uvView = nullptr;
    const tinygltf::Buffer* uvBuffer = nullptr;
    int uvStride = 0;
    if (hasUV) {
        uvAccessor = &mModel->accessors[uvIt->second];
        uvView = &mModel->bufferViews[uvAccessor->bufferView];
        uvBuffer = &mModel->buffers[uvView->buffer];
        uvStride = uvAccessor->ByteStride(*uvView);
    }

    // ----------------------------------------------------
    // 读取并解析关节索引与权重数据
    // ----------------------------------------------------
    auto jointIt = primitive.attributes.find("JOINTS_0");
    auto weightIt = primitive.attributes.find("WEIGHTS_0");

    if (jointIt != primitive.attributes.end() && weightIt != primitive.attributes.end()) {
        const tinygltf::Accessor& jointAccessor = mModel->accessors[jointIt->second];
        const tinygltf::BufferView& jointView = mModel->bufferViews[jointAccessor.bufferView];
        const tinygltf::Buffer& jointBuffer = mModel->buffers[jointView.buffer];
        mJointVec.resize(jointAccessor.count);
        std::memcpy(mJointVec.data(), &jointBuffer.data[jointView.byteOffset + jointAccessor.byteOffset], jointView.byteLength);

        const tinygltf::Accessor& weightAccessor = mModel->accessors[weightIt->second];
        const tinygltf::BufferView& weightView = mModel->bufferViews[weightAccessor.bufferView];
        const tinygltf::Buffer& weightBuffer = mModel->buffers[weightView.buffer];
        mWeightVec.resize(weightAccessor.count);
        std::memcpy(mWeightVec.data(), &weightBuffer.data[weightView.byteOffset + weightAccessor.byteOffset], weightView.byteLength);
    }

    // 交织拼装顶点结构体数组
    std::vector<Vertex> vertices(mVertexCount);
    for (size_t i = 0; i < mVertexCount; ++i) {
        // 读取位置信息
        const float* posPtr = reinterpret_cast<const float*>(
            &posBuffer.data[posAccessor.byteOffset + posView.byteOffset + i * posStride]
        );
        vertices[i].pos[0] = posPtr[0];
        vertices[i].pos[1] = posPtr[1];
        vertices[i].pos[2] = posPtr[2];

        // 读取法线信息
        if (hasNormal) {
            const float* normPtr = reinterpret_cast<const float*>(
                &normBuffer->data[normAccessor->byteOffset + normView->byteOffset + i * normStride]
            );
            vertices[i].normal[0] = normPtr[0];
            vertices[i].normal[1] = normPtr[1];
            vertices[i].normal[2] = normPtr[2];
        } else {
            vertices[i].normal[0] = 0.0f;
            vertices[i].normal[1] = 0.0f;
            vertices[i].normal[2] = 0.0f;
        }

        // 读取纹理 UV 坐标
        if (hasUV) {
            const float* uvPtr = reinterpret_cast<const float*>(
                &uvBuffer->data[uvAccessor->byteOffset + uvView->byteOffset + i * uvStride]
            );
            vertices[i].uv[0] = uvPtr[0];
            vertices[i].uv[1] = uvPtr[1];
        } else {
            vertices[i].uv[0] = 0.0f;
            vertices[i].uv[1] = 0.0f;
        }

        // 填充关节与权重
        if (i < mJointVec.size()) {
            vertices[i].jointNum[0] = mJointVec[i].idx[0];
            vertices[i].jointNum[1] = mJointVec[i].idx[1];
            vertices[i].jointNum[2] = mJointVec[i].idx[2];
            vertices[i].jointNum[3] = mJointVec[i].idx[3];
        } else {
            vertices[i].jointNum[0] = 0;
            vertices[i].jointNum[1] = 0;
            vertices[i].jointNum[2] = 0;
            vertices[i].jointNum[3] = 0;
        }

        if (i < mWeightVec.size()) {
            vertices[i].jointWeight[0] = mWeightVec[i].w[0];
            vertices[i].jointWeight[1] = mWeightVec[i].w[1];
            vertices[i].jointWeight[2] = mWeightVec[i].w[2];
            vertices[i].jointWeight[3] = mWeightVec[i].w[3];
        } else {
            vertices[i].jointWeight[0] = 1.0f;
            vertices[i].jointWeight[1] = 0.0f;
            vertices[i].jointWeight[2] = 0.0f;
            vertices[i].jointWeight[3] = 0.0f;
        }
    }

    // 备份原始顶点数据
    mOriginalVertices = vertices;
    mRenderDataPtr = &renderData;

    // 检查并解析索引缓冲
    if (primitive.indices < 0) {
        return false;
    }

    const tinygltf::Accessor& indexAccessor = mModel->accessors[primitive.indices];
    const tinygltf::BufferView& indexView = mModel->bufferViews[indexAccessor.bufferView];
    const tinygltf::Buffer& indexBuffer = mModel->buffers[indexView.buffer];
    int indexStride = indexAccessor.ByteStride(indexView);
    mIndexCount = static_cast<uint32_t>(indexAccessor.count);

    // 将索引转换为统一的 uint16_t
    std::vector<uint16_t> indices(mIndexCount);
    for (size_t i = 0; i < mIndexCount; ++i) {
        const unsigned char* indexPtr = &indexBuffer.data[indexAccessor.byteOffset + indexView.byteOffset + i * indexStride];
        if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
            indices[i] = *reinterpret_cast<const uint16_t*>(indexPtr);
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
            indices[i] = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(indexPtr));
        } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
            indices[i] = static_cast<uint16_t>(*reinterpret_cast<const uint8_t*>(indexPtr));
        }
    }

    // 上传顶点缓冲到 GPU 本地内存
    if (!createGPUBuffer(renderData, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data(), vertices.size() * sizeof(Vertex), mVertexBuffer, mVertexBufferAlloc)) {
        return false;
    }

    // 上传索引缓冲到 GPU 本地内存
    if (!createGPUBuffer(renderData, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.size() * sizeof(uint16_t), mIndexBuffer, mIndexBufferAlloc)) {
        return false;
    }

    // 检查是否有内嵌贴图，若有则触发断言，否则加载外部贴图 texture/Woman.png
    if (!mModel->images.empty()) {
        assert(false && "Embedded textures are not supported!");
        return false;
    }

    // 若无内嵌纹理，按照教程的要求加载外部的 texture/Woman.png 贴图
    if (!mTex.create(renderData, "texture/Woman.png")) {
        return false;
    }

    // 初始化纹理对应的描述符集，该过程会写入并覆盖 mRenderData.rdDescriptorSet
    if (!mTex.initDescriptorSet(renderData)) {
        return false;
    }

    // ----------------------------------------------------
    // 读取逆绑定矩阵 (Inverse Bind Matrices)
    // ----------------------------------------------------
    if (!mModel->skins.empty()) {
        const tinygltf::Skin& skin = mModel->skins[0];
        if (skin.inverseBindMatrices >= 0) {
            const tinygltf::Accessor& accessor = mModel->accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = mModel->bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = mModel->buffers[bufferView.buffer];

            mInverseBindMatrices.resize(skin.joints.size());
            std::memcpy(mInverseBindMatrices.data(),
                        &buffer.data[bufferView.byteOffset + accessor.byteOffset],
                        bufferView.byteLength);
        }

        // 初始化节点到关节的映射查找表
        mNodeToJoint.resize(mModel->nodes.size(), -1);
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int destinationNode = skin.joints[i];
            mNodeToJoint[destinationNode] = static_cast<int>(i);
        }

        // 创建骨骼关节矩阵 SSBO 缓冲和描述符 (支持多实例)
        if (!createJointSSBO(renderData)) {
            return false;
        }
    }

    // 读取动画数据
    getAnimations();

    return true;
}

bool GltfModel::createJointSSBO(VkRenderData& renderData) {
    if (mInverseBindMatrices.empty()) {
        return true;
    }

    size_t jointCount = mInverseBindMatrices.size();
    size_t maxInstances = 200; // 实例化支持的最大数量

    // 创建骨骼关节矩阵 SSBO 缓冲 (包含所有实例的空间)
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(glm::mat4) * jointCount * maxInstances;
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(renderData.rdAllocator, &bufferInfo, &allocInfo,
                        &mJointBuffer, &mJointBufferAlloc, nullptr) != VK_SUCCESS) {
        return false;
    }

    // 创建 DQS SSBO 缓冲
    VkBufferCreateInfo dqsBufferInfo{};
    dqsBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    dqsBufferInfo.size = sizeof(glm::mat2x4) * jointCount * maxInstances;
    dqsBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    dqsBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vmaCreateBuffer(renderData.rdAllocator, &dqsBufferInfo, &allocInfo,
                        &mJointDQSBuffer, &mJointDQSBufferAlloc, nullptr) != VK_SUCCESS) {
        return false;
    }

    // 创建描述符池
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(renderData.rdVkbDevice.device, &poolInfo, nullptr, &mJointDescriptorPool) != VK_SUCCESS) {
        return false;
    }

    // 分配描述符集 1 (LBS)
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = mJointDescriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts = &renderData.rdJointDescriptorSetLayout;

    if (vkAllocateDescriptorSets(renderData.rdVkbDevice.device, &descriptorSetAllocInfo, &mJointDescriptorSet) != VK_SUCCESS) {
        return false;
    }

    // 分配描述符集 2 (DQS)
    if (vkAllocateDescriptorSets(renderData.rdVkbDevice.device, &descriptorSetAllocInfo, &mJointDQSDescriptorSet) != VK_SUCCESS) {
        return false;
    }

    // 更新描述符集绑定 1 (LBS)
    VkDescriptorBufferInfo bufferInfoDesc{};
    bufferInfoDesc.buffer = mJointBuffer;
    bufferInfoDesc.offset = 0;
    bufferInfoDesc.range = sizeof(glm::mat4) * jointCount * maxInstances;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = mJointDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfoDesc;

    vkUpdateDescriptorSets(renderData.rdVkbDevice.device, 1, &descriptorWrite, 0, nullptr);

    // 更新描述符集绑定 2 (DQS)
    VkDescriptorBufferInfo dqsBufferInfoDesc{};
    dqsBufferInfoDesc.buffer = mJointDQSBuffer;
    dqsBufferInfoDesc.offset = 0;
    dqsBufferInfoDesc.range = sizeof(glm::mat2x4) * jointCount * maxInstances;

    VkWriteDescriptorSet dqsDescriptorWrite{};
    dqsDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    dqsDescriptorWrite.dstSet = mJointDQSDescriptorSet;
    dqsDescriptorWrite.dstBinding = 0;
    dqsDescriptorWrite.dstArrayElement = 0;
    dqsDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dqsDescriptorWrite.descriptorCount = 1;
    dqsDescriptorWrite.pBufferInfo = &dqsBufferInfoDesc;

    vkUpdateDescriptorSets(renderData.rdVkbDevice.device, 1, &dqsDescriptorWrite, 0, nullptr);

    return true;
}

void GltfModel::draw(VkCommandBuffer commandBuffer) {
    if (mVertexBuffer == VK_NULL_HANDLE || mIndexBuffer == VK_NULL_HANDLE) {
        return;
    }

    // 绑定顶点缓冲 (对应 binding=0)
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertexBuffer, &offset);

    // 绑定索引缓冲
    vkCmdBindIndexBuffer(commandBuffer, mIndexBuffer, 0, VK_INDEX_TYPE_UINT16);

    // 执行带索引的网格绘制
    vkCmdDrawIndexed(commandBuffer, mIndexCount, 1, 0, 0, 0);
}

void GltfModel::cleanup(VkRenderData& renderData) {
    // 销毁并重置纹理描述符和图像资源
    mTex.cleanup(renderData);

    if (mJointDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(renderData.rdVkbDevice.device, mJointDescriptorPool, nullptr);
        mJointDescriptorPool = VK_NULL_HANDLE;
    }
    mJointDescriptorSet = VK_NULL_HANDLE;
    mJointDQSDescriptorSet = VK_NULL_HANDLE;

    if (mJointBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mJointBuffer, mJointBufferAlloc);
        mJointBuffer = VK_NULL_HANDLE;
        mJointBufferAlloc = VK_NULL_HANDLE;
    }

    if (mJointDQSBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mJointDQSBuffer, mJointDQSBufferAlloc);
        mJointDQSBuffer = VK_NULL_HANDLE;
        mJointDQSBufferAlloc = VK_NULL_HANDLE;
    }

    // 销毁顶点缓冲并归还 VMA 显存
    if (mVertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mVertexBuffer, mVertexBufferAlloc);
        mVertexBuffer = VK_NULL_HANDLE;
        mVertexBufferAlloc = VK_NULL_HANDLE;
    }

    // 销毁索引缓冲并归还 VMA 显存
    if (mIndexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mIndexBuffer, mIndexBufferAlloc);
        mIndexBuffer = VK_NULL_HANDLE;
        mIndexBufferAlloc = VK_NULL_HANDLE;
    }

    mVertexCount = 0;
    mIndexCount = 0;
    mInverseBindMatrices.clear();
    mModel.reset();
}

bool GltfModel::createGPUBuffer(VkRenderData& renderData, VkBufferUsageFlags usage, const void* data, VkDeviceSize size, VkBuffer& buffer, VmaAllocation& allocation) {
    // 1. 创建 CPU 可映射的暂存缓冲区
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingBufferAlloc = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    if (vmaCreateBuffer(renderData.rdAllocator, &stagingBufferInfo, &stagingAllocInfo,
                        &stagingBuffer, &stagingBufferAlloc, nullptr) != VK_SUCCESS) {
        return false;
    }

    // 映射显存并拷贝顶点/索引数据
    void* mappedData;
    vmaMapMemory(renderData.rdAllocator, stagingBufferAlloc, &mappedData);
    std::memcpy(mappedData, data, size);
    vmaUnmapMemory(renderData.rdAllocator, stagingBufferAlloc);

    // 2. 创建 GPU 设备本地的目标缓冲区
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(renderData.rdAllocator, &bufferInfo, &allocInfo,
                        &buffer, &allocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);
        return false;
    }

    // 3. 提交单次拷贝命令，将数据从暂存区传输至 GPU 本地核心区
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = renderData.rdCommandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(renderData.rdVkbDevice.device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, buffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(renderData.rdGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderData.rdGraphicsQueue);
    vkFreeCommandBuffers(renderData.rdVkbDevice.device, renderData.rdCommandPool, 1, &commandBuffer);

    // 销毁无用的暂存缓冲
    vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);
    return true;
}

void GltfModel::getNodeData(std::shared_ptr<GltfNode> node, const glm::mat4& parentMatrix) {
    if (!mModel || node->mNodeIndex < 0 || node->mNodeIndex >= static_cast<int>(mModel->nodes.size())) {
        return;
    }

    const tinygltf::Node& gltfNode = mModel->nodes[node->mNodeIndex];
    node->mName = gltfNode.name;

    // 读取 translation
    if (gltfNode.translation.size() == 3) {
        node->setTranslation(glm::vec3(
            static_cast<float>(gltfNode.translation[0]),
            static_cast<float>(gltfNode.translation[1]),
            static_cast<float>(gltfNode.translation[2])
        ));
    } else {
        node->setTranslation(glm::vec3(0.0f));
    }

    // 读取 rotation (glTF 顺序为 [x, y, z, w]，glm::quat 构造函数顺序为 (w, x, y, z))
    if (gltfNode.rotation.size() == 4) {
        node->setRotation(glm::quat(
            static_cast<float>(gltfNode.rotation[3]), // w
            static_cast<float>(gltfNode.rotation[0]), // x
            static_cast<float>(gltfNode.rotation[1]), // y
            static_cast<float>(gltfNode.rotation[2])  // z
        ));
    } else {
        node->setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    }

    // 读取 scale
    if (gltfNode.scale.size() == 3) {
        node->setScale(glm::vec3(
            static_cast<float>(gltfNode.scale[0]),
            static_cast<float>(gltfNode.scale[1]),
            static_cast<float>(gltfNode.scale[2])
        ));
    } else {
        node->setScale(glm::vec3(1.0f));
    }

    // 计算局部 TRS 矩阵和全局节点矩阵
    node->calculateLocalTRSMatrix();
    node->mNodeMatrix = parentMatrix * node->mLocalTRSMatrix;
}

void GltfModel::getNodes(std::shared_ptr<GltfNode> node) {
    if (!mModel || node->mNodeIndex < 0 || node->mNodeIndex >= static_cast<int>(mModel->nodes.size())) {
        return;
    }

    const tinygltf::Node& gltfNode = mModel->nodes[node->mNodeIndex];

    for (int childIndex : gltfNode.children) {
        auto childNode = GltfNode::createRoot(childIndex);

        // 递归设置子节点数据（传入当前节点的全局矩阵作为父矩阵）
        getNodeData(childNode, node->mNodeMatrix);

        childNode->mParentNode = node;
        node->mChildNodes.push_back(childNode);

        // 递归构建子树
        getNodes(childNode);
    }
}

void GltfModel::getAnimations() {
    for (const auto& anim : mModel->animations) {
        GltfAnimationClip clip(anim.name);
        for (const auto& channel : anim.channels) {
            clip.addChannel(mModel, anim, channel);
        }
        mAnimClips.push_back(clip);
    }
    Logger::log(1, "成功加载动画片段数: %zu\n", mAnimClips.size());
}

std::shared_ptr<GltfNode> GltfModel::buildInstanceNodeTree(std::vector<std::shared_ptr<GltfNode>>& nodeList) {
    if (!mModel || mModel->scenes.empty() || mModel->scenes[0].nodes.empty()) {
        return nullptr;
    }
    int rootNodeIndex = mModel->scenes[0].nodes[0];
    auto rootNode = GltfNode::createRoot(rootNodeIndex);
    getNodeData(rootNode, glm::mat4(1.0f));
    getNodes(rootNode);

    nodeList.clear();
    nodeList.resize(mModel->nodes.size(), nullptr);
    buildInstanceNodeList(rootNode, nodeList);
    return rootNode;
}

void GltfModel::buildInstanceNodeList(std::shared_ptr<GltfNode> node, std::vector<std::shared_ptr<GltfNode>>& nodeList) {
    if (!node) return;
    if (node->mNodeIndex >= 0 && node->mNodeIndex < static_cast<int>(nodeList.size())) {
        nodeList[node->mNodeIndex] = node;
    }
    for (const auto& child : node->mChildNodes) {
        buildInstanceNodeList(child, nodeList);
    }
}

void GltfModel::updateGPUJointBuffers(VkRenderData& renderData, const std::vector<glm::mat4>& allJointMatrices, const std::vector<glm::mat2x4>& allJointDualQuats) {
    void* mappedData = nullptr;
    if (!allJointMatrices.empty() && mJointBuffer != VK_NULL_HANDLE) {
        if (vmaMapMemory(renderData.rdAllocator, mJointBufferAlloc, &mappedData) == VK_SUCCESS) {
            std::memcpy(mappedData, allJointMatrices.data(), allJointMatrices.size() * sizeof(glm::mat4));
            vmaUnmapMemory(renderData.rdAllocator, mJointBufferAlloc);
        }
    }
    void* mappedDataDQS = nullptr;
    if (!allJointDualQuats.empty() && mJointDQSBuffer != VK_NULL_HANDLE) {
        if (vmaMapMemory(renderData.rdAllocator, mJointDQSBufferAlloc, &mappedDataDQS) == VK_SUCCESS) {
            std::memcpy(mappedDataDQS, allJointDualQuats.data(), allJointDualQuats.size() * sizeof(glm::mat2x4));
            vmaUnmapMemory(renderData.rdAllocator, mJointDQSBufferAlloc);
        }
    }
}
