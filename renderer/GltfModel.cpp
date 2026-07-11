#include "GltfModel.h"
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

        Logger::log(1, "成功加载关节索引与权重，顶点数: %zu\n", mJointVec.size());
    } else {
        Logger::log(1, "警告：模型不包含 JOINTS_0 或 WEIGHTS_0 属性！\n");
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
        Logger::log(1, "此模型不包含索引数据\n");
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
        Logger::log(1, "顶点缓冲区创建或上传失败\n");
        return false;
    }

    // 上传索引缓冲到 GPU 本地内存
    if (!createGPUBuffer(renderData, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indices.size() * sizeof(uint16_t), mIndexBuffer, mIndexBufferAlloc)) {
        Logger::log(1, "索引缓冲区创建或上传失败\n");
        return false;
    }

    // 检查是否有内嵌贴图，若有则触发断言，否则加载外部贴图 texture/Woman.png
    if (!mModel->images.empty()) {
        Logger::log(1, "错误：不支持加载内嵌贴图的模型！\n");
        assert(false && "Embedded textures are not supported!");
        return false;
    }

    // 若无内嵌纹理，按照教程的要求加载外部的 texture/Woman.png 贴图
    if (!mTex.create(renderData, "texture/Woman.png")) {
        Logger::log(1, "加载默认贴图 texture/Woman.png 失败\n");
        return false;
    }

    // 初始化纹理对应的描述符集，该过程会写入并覆盖 mRenderData.rdDescriptorSet
    if (!mTex.initDescriptorSet(renderData)) {
        Logger::log(1, "描述符集初始化/绑定失败\n");
        return false;
    }

    // ----------------------------------------------------
    // 构建骨骼节点树
    // ----------------------------------------------------
    if (!mModel->scenes.empty() && !mModel->scenes[0].nodes.empty()) {
        int rootNodeIndex = mModel->scenes[0].nodes[0];
        mRootNode = GltfNode::createRoot(rootNodeIndex);
        getNodeData(mRootNode, glm::mat4(1.0f));
        getNodes(mRootNode);

        // 打印骨骼树结构以验证
        mRootNode->printTree();
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

            Logger::log(1, "成功加载逆绑定矩阵，关节数: %zu\n", mInverseBindMatrices.size());
        }

        // 初始化节点到关节的映射查找表
        mNodeToJoint.resize(mModel->nodes.size(), -1);
        mJointMatrices.resize(skin.joints.size(), glm::mat4(1.0f));
        for (size_t i = 0; i < skin.joints.size(); ++i) {
            int destinationNode = skin.joints[i];
            mNodeToJoint[destinationNode] = static_cast<int>(i);
        }

        // 创建骨骼关节矩阵 SSBO 缓冲和描述符
        if (!createJointSSBO(renderData)) {
            return false;
        }
    }

    return true;
}

bool GltfModel::createJointSSBO(VkRenderData& renderData) {
    if (mJointMatrices.empty()) {
        return true;
    }

    // 创建骨骼关节矩阵 SSBO 缓冲
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(glm::mat4) * mJointMatrices.size();
    bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (vmaCreateBuffer(renderData.rdAllocator, &bufferInfo, &allocInfo,
                        &mJointBuffer, &mJointBufferAlloc, nullptr) != VK_SUCCESS) {
        Logger::log(1, "创建关节矩阵 SSBO 缓冲失败\n");
        return false;
    }

    // 创建描述符池
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(renderData.rdVkbDevice.device, &poolInfo, nullptr, &mJointDescriptorPool) != VK_SUCCESS) {
        Logger::log(1, "创建关节描述符池失败\n");
        return false;
    }

    // 分配描述符集
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.descriptorPool = mJointDescriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = 1;
    descriptorSetAllocInfo.pSetLayouts = &renderData.rdJointDescriptorSetLayout;

    if (vkAllocateDescriptorSets(renderData.rdVkbDevice.device, &descriptorSetAllocInfo, &mJointDescriptorSet) != VK_SUCCESS) {
        Logger::log(1, "分配关节描述符集失败\n");
        return false;
    }

    // 更新描述符集绑定
    VkDescriptorBufferInfo bufferInfoDesc{};
    bufferInfoDesc.buffer = mJointBuffer;
    bufferInfoDesc.offset = 0;
    bufferInfoDesc.range = sizeof(glm::mat4) * mJointMatrices.size();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = mJointDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfoDesc;

    vkUpdateDescriptorSets(renderData.rdVkbDevice.device, 1, &descriptorWrite, 0, nullptr);

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

    if (mJointBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(renderData.rdAllocator, mJointBuffer, mJointBufferAlloc);
        mJointBuffer = VK_NULL_HANDLE;
        mJointBufferAlloc = VK_NULL_HANDLE;
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
    mRootNode.reset();
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
        Logger::log(1, "创建暂存缓冲区失败\n");
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
        Logger::log(1, "创建本地核心缓冲区失败\n");
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
        node->mTranslation = glm::vec3(
            static_cast<float>(gltfNode.translation[0]),
            static_cast<float>(gltfNode.translation[1]),
            static_cast<float>(gltfNode.translation[2])
        );
    } else {
        node->mTranslation = glm::vec3(0.0f);
    }

    // 读取 rotation (glTF 顺序为 [x, y, z, w]，glm::quat 构造函数顺序为 (w, x, y, z))
    if (gltfNode.rotation.size() == 4) {
        node->mRotation = glm::quat(
            static_cast<float>(gltfNode.rotation[3]), // w
            static_cast<float>(gltfNode.rotation[0]), // x
            static_cast<float>(gltfNode.rotation[1]), // y
            static_cast<float>(gltfNode.rotation[2])  // z
        );
    } else {
        node->mRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // 读取 scale
    if (gltfNode.scale.size() == 3) {
        node->mScale = glm::vec3(
            static_cast<float>(gltfNode.scale[0]),
            static_cast<float>(gltfNode.scale[1]),
            static_cast<float>(gltfNode.scale[2])
        );
    } else {
        node->mScale = glm::vec3(1.0f);
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

        node->mChildNodes.push_back(childNode);

        // 递归构建子树
        getNodes(childNode);
    }
}

void GltfModel::updateVertexBuffer(VkRenderData& renderData, const void* data, VkDeviceSize size) {
    if (mVertexBuffer == VK_NULL_HANDLE) return;

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
        Logger::log(1, "创建暂存缓冲区失败\n");
        return;
    }

    // 映射并拷贝顶点数据
    void* mappedData = nullptr;
    vmaMapMemory(renderData.rdAllocator, stagingBufferAlloc, &mappedData);
    std::memcpy(mappedData, data, size);
    vmaUnmapMemory(renderData.rdAllocator, stagingBufferAlloc);

    // 2. 提交单次拷贝命令，将数据从暂存区传输至 GPU 本地核心区
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
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, mVertexBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(renderData.rdGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(renderData.rdGraphicsQueue);
    vkFreeCommandBuffers(renderData.rdVkbDevice.device, renderData.rdCommandPool, 1, &commandBuffer);

    // 销毁暂存缓冲
    vmaDestroyBuffer(renderData.rdAllocator, stagingBuffer, stagingBufferAlloc);
}

void GltfModel::updateJoints(std::shared_ptr<GltfNode> node, const glm::mat4& parentMatrix, float time) {
    if (!node) return;

    // 根据模型路径显式匹配并播放骨骼动画
    if (mModelPath.find("dq.gltf") != std::string::npos) {
        // Cube (dq.gltf) 的“拧麻花”动画：让 Bone.001 绕其局部 Y 轴（纵向）正弦旋转
        if (node->mName == "Bone.001" || node->mNodeIndex == 0) {
            node->mRotation = glm::angleAxis(glm::sin(time * 1.5f) * 1.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        }
    } else if (mModelPath.find("Woman.gltf") != std::string::npos) {
        // Woman.gltf 的摆动动画：让 Spine 关节（节点索引为 29）随时间做左右周期摆动旋转
        if (node->mNodeIndex == 29) {
            node->mRotation = glm::angleAxis(glm::sin(time * 2.0f) * 0.4f, glm::vec3(0.0f, 0.0f, 1.0f));
        }
    }

    node->calculateLocalTRSMatrix();
    node->mNodeMatrix = parentMatrix * node->mLocalTRSMatrix;

    // 如果此节点是骨架关节中的一部分，更新其蒙皮矩阵
    if (node->mNodeIndex >= 0 && node->mNodeIndex < static_cast<int>(mNodeToJoint.size())) {
        int jointIdx = mNodeToJoint[node->mNodeIndex];
        if (jointIdx != -1) {
            mJointMatrices[jointIdx] = node->mNodeMatrix * mInverseBindMatrices[jointIdx];
        }
    }

    for (const auto& child : node->mChildNodes) {
        updateJoints(child, node->mNodeMatrix, time);
    }
}

void GltfModel::applyVertexSkinning(float time) {
    if (!mRootNode || mJointMatrices.empty() || mJointBuffer == VK_NULL_HANDLE || !mRenderDataPtr) {
        return;
    }

    // 1. 递归更新骨骼的全局矩阵与关节变换矩阵
    updateJoints(mRootNode, glm::mat4(1.0f), time);

    // 2. 将矩阵直接上传到 GPU 存储缓冲区 (SSBO)
    void* mappedData = nullptr;
    if (vmaMapMemory(mRenderDataPtr->rdAllocator, mJointBufferAlloc, &mappedData) == VK_SUCCESS) {
        std::memcpy(mappedData, mJointMatrices.data(), mJointMatrices.size() * sizeof(glm::mat4));
        vmaUnmapMemory(mRenderDataPtr->rdAllocator, mJointBufferAlloc);
    }
}
