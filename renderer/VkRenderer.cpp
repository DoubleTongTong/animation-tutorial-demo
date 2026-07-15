#include "VkRenderer.h"
#include "Logger.h"
#include <vector>
#include <fstream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>

VkRenderer::VkRenderer(GLFWwindow* window) : mWindow(window) {
}

VkRenderer::~VkRenderer() {
    cleanup();
}

bool VkRenderer::init() {
    if (!initVulkan()) {
        return false;
    }
    if (!mSwapchain.init(mRenderData, mWindow)) {
        return false;
    }
    if (!initCommandPoolAndBuffer()) {
        return false;
    }
    if (!initSyncObjects()) {
        return false;
    }
    if (!initDepthBuffer()) {
        return false;
    }
    if (!mRenderPass.init(mRenderData)) {
        return false;
    }
    if (!mMesh.create(mRenderData)) {
        return false;
    }
    mRenderData.rdTriangleCount = mMesh.getVertexCount() / 3;
    if (!mPipeline.init(mRenderData)) {
        return false;
    }
    if (!mTexture.create(mRenderData)) {
        return false;
    }
    if (!mTexture.initDescriptorSet(mRenderData)) {
        return false;
    }

    // 初始化并加载 glTF 模型
    mGltfModel = std::make_shared<GltfModel>();
    std::string modelPath = "assets/Woman.gltf";

    Logger::log(1, "尝试加载 glTF 模型，路径为: %s\n", modelPath.c_str());
    if (!mGltfModel->loadModel(mRenderData, modelPath)) {
        Logger::log(1, "加载 glTF 模型失败\n");
        return false;
    }

    // 实例化 200 个模型
    std::srand(static_cast<unsigned int>(time(nullptr)));
    int numTriangles = 0;
    for (int i = 0; i < 200; ++i) {
        int xPos = std::rand() % 40 - 20;
        int zPos = std::rand() % 40 - 20;
        mGltfInstances.emplace_back(std::make_shared<GltfInstance>(mGltfModel, glm::vec2(static_cast<float>(xPos), static_cast<float>(zPos)), true));
        numTriangles += mGltfModel->getTriangleCount();
    }
    mRenderData.rdTriangleCount = numTriangles;
    mRenderData.rdNumberOfInstances = static_cast<int>(mGltfInstances.size());
    mRenderData.rdCurrentSelectedInstance = 0;

    if (!mFramebuffer.init(mRenderData)) {
        return false;
    }
    mUserInterface.init(mRenderData, mWindow);
    Logger::log(1, "%s: Vulkan renderer successfully initialized\n", __FUNCTION__);
    return true;
}

bool VkRenderer::initVulkan() {
    vkb::InstanceBuilder builder;
    auto inst_ret = builder.set_app_name(mApplicationName.c_str())
                           .request_validation_layers()
                           .use_default_debug_messenger()
                           .build();
    if (!inst_ret) {
        Logger::log(1, "%s error: Failed to create Vulkan instance. Error: %s\n", __FUNCTION__, inst_ret.error().message().c_str());
        return false;
    }
    mRenderData.rdVkbInstance = inst_ret.value();

    VkResult result = glfwCreateWindowSurface(mRenderData.rdVkbInstance.instance, mWindow, nullptr, &mRenderData.rdSurface);
    if (result != VK_SUCCESS) {
        Logger::log(1, "%s error: Could not create Vulkan surface\n", __FUNCTION__);
        return false;
    }

    vkb::PhysicalDeviceSelector selector{ mRenderData.rdVkbInstance };
    auto phys_ret = selector.set_surface(mRenderData.rdSurface)
                             .select();
    if (!phys_ret) {
        Logger::log(1, "%s error: Failed to select Physical Device. Error: %s\n", __FUNCTION__, phys_ret.error().message().c_str());
        return false;
    }
    vkb::PhysicalDevice vkb_phys = phys_ret.value();

    vkb::DeviceBuilder device_builder{ vkb_phys };
    auto dev_ret = device_builder.build();
    if (!dev_ret) {
        Logger::log(1, "%s error: Failed to create Logical Device. Error: %s\n", __FUNCTION__, dev_ret.error().message().c_str());
        return false;
    }
    mRenderData.rdVkbDevice = dev_ret.value();

    auto gq_ret = mRenderData.rdVkbDevice.get_queue(vkb::QueueType::graphics);
    if (!gq_ret) {
        Logger::log(1, "%s error: Failed to get Graphics Queue. Error: %s\n", __FUNCTION__, gq_ret.error().message().c_str());
        return false;
    }
    mRenderData.rdGraphicsQueue = gq_ret.value();
    mGraphicsQueue = mRenderData.rdGraphicsQueue;
    mPresentQueue = mRenderData.rdGraphicsQueue;

    // Initialize VMA Allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = mRenderData.rdVkbDevice.physical_device.physical_device;
    allocatorInfo.device = mRenderData.rdVkbDevice.device;
    allocatorInfo.instance = mRenderData.rdVkbInstance.instance;
    VkResult allocResult = vmaCreateAllocator(&allocatorInfo, &mRenderData.rdAllocator);
    if (allocResult != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create VMA Allocator\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool VkRenderer::initSyncObjects() {
    mRenderData.rdRenderFences.resize(MAX_FRAMES_IN_FLIGHT);
    mRenderData.rdPresentSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

    // We need one render finished semaphore per swapchain image as suggested by spec to prevent reuse issues
    size_t swapchainImageCount = mRenderData.rdSwapchainImages.size();
    mRenderData.rdRenderSemaphores.resize(swapchainImageCount);

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateFence(mRenderData.rdVkbDevice.device, &fenceInfo, nullptr, &mRenderData.rdRenderFences[i]) != VK_SUCCESS ||
            vkCreateSemaphore(mRenderData.rdVkbDevice.device, &semaphoreInfo, nullptr, &mRenderData.rdPresentSemaphores[i]) != VK_SUCCESS) {
            Logger::log(1, "%s error: Failed to create sync objects for frame %zu\n", __FUNCTION__, i);
            return false;
        }
    }


    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(mRenderData.rdVkbDevice.device, &semaphoreInfo, nullptr, &mRenderData.rdRenderSemaphores[i]) != VK_SUCCESS) {
            Logger::log(1, "%s error: Failed to create render semaphore for image %zu\n", __FUNCTION__, i);
            return false;
        }
    }


    return true;
}

bool VkRenderer::initCommandPoolAndBuffer() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = mRenderData.rdVkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    if (vkCreateCommandPool(mRenderData.rdVkbDevice.device, &poolInfo, nullptr, &mRenderData.rdCommandPool) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create command pool\n", __FUNCTION__);
        return false;
    }

    mRenderData.rdCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mRenderData.rdCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(mRenderData.rdVkbDevice.device, &allocInfo, mRenderData.rdCommandBuffers.data()) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to allocate command buffers\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool VkRenderer::initDepthBuffer() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = mRenderData.rdVkbSwapchain.extent.width;
    imageInfo.extent.height = mRenderData.rdVkbSwapchain.extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(mRenderData.rdAllocator, &imageInfo, &allocInfo,
                       &mRenderData.rdDepthImage, &mRenderData.rdDepthImageAlloc, nullptr) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create depth image\n", __FUNCTION__);
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = mRenderData.rdDepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(mRenderData.rdVkbDevice.device, &viewInfo, nullptr, &mRenderData.rdDepthImageView) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create depth image view\n", __FUNCTION__);
        return false;
    }

    return true;
}



bool VkRenderer::recreateSwapchain(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;
    vkDeviceWaitIdle(device);

    // Cleanup Framebuffers
    mFramebuffer.cleanup(renderData);

    // Cleanup Depth Buffer
    if (renderData.rdDepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, renderData.rdDepthImageView, nullptr);
        renderData.rdDepthImageView = VK_NULL_HANDLE;
    }
    if (renderData.rdDepthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(renderData.rdAllocator, renderData.rdDepthImage, renderData.rdDepthImageAlloc);
        renderData.rdDepthImage = VK_NULL_HANDLE;
        renderData.rdDepthImageAlloc = VK_NULL_HANDLE;
    }

    // Cleanup old render semaphores
    for (size_t i = 0; i < renderData.rdRenderSemaphores.size(); ++i) {
        if (renderData.rdRenderSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderData.rdRenderSemaphores[i], nullptr);
        }
    }
    renderData.rdRenderSemaphores.clear();

    // Recreate Swapchain
    if (!mSwapchain.recreate(renderData, mWindow)) {
        Logger::log(1, "%s error: Failed to recreate swapchain\n", __FUNCTION__);
        return false;
    }

    // Recreate Depth Buffer
    if (!initDepthBuffer()) {
        Logger::log(1, "%s error: Failed to recreate depth buffer\n", __FUNCTION__);
        return false;
    }

    // Recreate Render Semaphores (since the swapchain image count might have changed)
    size_t swapchainImageCount = renderData.rdSwapchainImages.size();
    renderData.rdRenderSemaphores.resize(swapchainImageCount);
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (size_t i = 0; i < swapchainImageCount; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderData.rdRenderSemaphores[i]) != VK_SUCCESS) {
            Logger::log(1, "%s error: Failed to recreate render semaphore for image %zu\n", __FUNCTION__, i);
            return false;
        }
    }

    // Recreate Framebuffers
    if (!mFramebuffer.init(renderData)) {
        Logger::log(1, "%s error: Failed to recreate framebuffers\n", __FUNCTION__);
        return false;
    }

    mFramebufferResized = false;
    Logger::log(1, "%s: Swapchain successfully recreated\n", __FUNCTION__);
    return true;
}

bool VkRenderer::draw() {
    double tickTime = glfwGetTime();
    mRenderData.rdTickDiff = mLastTickTime > 0.0 ? static_cast<float>(tickTime - mLastTickTime) : 0.0f;
    mLastTickTime = tickTime;

    // Check if we should lock or unlock mouse cursor (Unreal Engine style controls)
    bool rmbHeld = glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    ImGuiIO& io = ImGui::GetIO();
    if (rmbHeld) {
        if (!mMouseLock) {
            if (!io.WantCaptureMouse) {
                mMouseLock = true;
                double x, y;
                glfwGetCursorPos(mWindow, &x, &y);
                mMouseXPos = static_cast<int>(x);
                mMouseYPos = static_cast<int>(y);

                // Commented out to prevent cursor locking/jumping issues in Remote Desktop (RDP) environments.
                // glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                // if (glfwRawMouseMotionSupported()) {
                //     glfwSetInputMode(mWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
                // }
            }
        }
    } else {
        if (mMouseLock) {
            mMouseLock = false;
            // glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    handleMovementKeys();

    static float prevFrameStartTime = 0.0f;
    float frameStartTime = static_cast<float>(tickTime);
    mRenderData.rdFrameTime = frameStartTime - prevFrameStartTime;
    prevFrameStartTime = frameStartTime;



    if (mFramebufferResized) {
        mFramebufferResized = false;
        if (!recreateSwapchain(mRenderData)) {
            return false;
        }
    }

    if (vkWaitForFences(mRenderData.rdVkbDevice.device, 1,
        &mRenderData.rdRenderFences[mCurrentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        Logger::log(1, "%s error: waiting for fence failed\n", __FUNCTION__);
        return false;
    }

    if (vkResetFences(mRenderData.rdVkbDevice.device, 1,
        &mRenderData.rdRenderFences[mCurrentFrame]) != VK_SUCCESS) {
        Logger::log(1, "%s error:  fence reset failed\n", __FUNCTION__);
        return false;
    }

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(mRenderData.rdVkbDevice.device,
        mRenderData.rdVkbSwapchain.swapchain, UINT64_MAX,
        mRenderData.rdPresentSemaphores[mCurrentFrame], VK_NULL_HANDLE,
        &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(mRenderData);
        return true;
    } else {
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            Logger::log(1, "%s error: failed to acquire swapchain image. Error is '%i'\n", __FUNCTION__, result);
            return false;
        }
    }

    VkCommandBuffer commandBuffer = mRenderData.rdCommandBuffers[mCurrentFrame];

    if (vkResetCommandBuffer(commandBuffer, 0) != VK_SUCCESS) {
        Logger::log(1, "%s error: failed to reset command buffer\n", __FUNCTION__);
        return false;
    }

    VkCommandBufferBeginInfo cmdBeginInfo{};
    cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &cmdBeginInfo) != VK_SUCCESS) {
        Logger::log(1, "%s error: failed to begin command buffer\n", __FUNCTION__);
        return false;
    }

    VkClearValue colorClearValue;
    colorClearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };

    VkClearValue depthValue;
    depthValue.depthStencil.depth = 1.0f;

    VkClearValue clearValues[] = { colorClearValue, depthValue };

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = mRenderData.rdRenderpass;
    rpInfo.renderArea.offset.x = 0;
    rpInfo.renderArea.offset.y = 0;
    rpInfo.renderArea.extent = mRenderData.rdVkbSwapchain.extent;
    rpInfo.framebuffer = mRenderData.rdFramebuffers[imageIndex];
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(mRenderData.rdVkbSwapchain.extent.width);
    viewport.height = static_cast<float>(mRenderData.rdVkbSwapchain.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = mRenderData.rdVkbSwapchain.extent;

    vkCmdBeginRenderPass(commandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    for (auto &instance : mGltfInstances) {
        instance->checkForUpdates();
        instance->updateAnimation();
    }

    std::vector<glm::mat4> allJointMatrices{};
    std::vector<glm::mat2x4> allJointDualQuats{};
    size_t jointCount = mGltfModel->getInverseBindMatrices().size();
    
    allJointMatrices.resize(mGltfInstances.size() * jointCount, glm::mat4(1.0f));
    allJointDualQuats.resize(mGltfInstances.size() * jointCount, glm::mat2x4(0.0f));

    int currentTriangles = 0;
    for (size_t i = 0; i < mGltfInstances.size(); ++i) {
        auto &instance = mGltfInstances[i];
        ModelSettings settings = instance->getInstanceSettings();
        if (settings.msDrawModel) {
            currentTriangles += mGltfModel->getTriangleCount();
        }
        const auto &joints = instance->getJointMatrices();
        const auto &dqs = instance->getJointDualQuats();
        if (!joints.empty()) {
            std::memcpy(&allJointMatrices[i * jointCount], joints.data(), joints.size() * sizeof(glm::mat4));
        }
        if (!dqs.empty()) {
            std::memcpy(&allJointDualQuats[i * jointCount], dqs.data(), dqs.size() * sizeof(glm::mat2x4));
        }
    }
    mRenderData.rdTriangleCount = currentTriangles;

    mGltfModel->updateGPUJointBuffers(mRenderData, allJointMatrices, allJointDualQuats);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    float width = static_cast<float>(mRenderData.rdVkbSwapchain.extent.width);
    float height = static_cast<float>(mRenderData.rdVkbSwapchain.extent.height);
    float aspect = width / height;
    
    glm::mat4 proj = glm::perspective(glm::radians(static_cast<float>(mRenderData.rdFieldOfView)), aspect, 0.1f, 100.0f);
    proj[1][1] *= -1;
    
    glm::mat4 view = mCamera.getViewMatrix(mRenderData);
    glm::mat4 viewProj = proj * view;

    ModelSettings selectedSettings = mGltfInstances.at(mRenderData.rdCurrentSelectedInstance)->getInstanceSettings();
    mUserInterface.createFrame(mRenderData, selectedSettings, viewProj);
    mGltfInstances.at(mRenderData.rdCurrentSelectedInstance)->setInstanceSettings(selectedSettings);

    for (size_t i = 0; i < mGltfInstances.size(); ++i) {
        auto &instance = mGltfInstances[i];
        ModelSettings settings = instance->getInstanceSettings();
        if (!settings.msDrawModel) {
            continue;
        }

        bool useDQS = (settings.msVertexSkinningMode == skinningMode::dualQuat);
        VkPipeline activePipeline = useDQS ? mRenderData.rdPipelineDQS : mRenderData.rdPipeline;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, activePipeline);

        VkDescriptorSet activeJointSet = useDQS ? mGltfModel->getJointDescriptorSetDQS() : mGltfModel->getJointDescriptorSetLBS();
        VkDescriptorSet descriptorSets[] = {
            mGltfModel->getTextureDescriptorSet(),
            activeJointSet
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mRenderData.rdPipelineLayout, 0, 2, descriptorSets, 0, nullptr);

        struct PushConstants {
            glm::mat4 mvp;
            int aModelStride;
        } pc;
        pc.mvp = viewProj;
        pc.aModelStride = static_cast<int>(i * jointCount);

        vkCmdPushConstants(
            commandBuffer,
            mRenderData.rdPipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(pc),
            &pc
        );

        mGltfModel->draw(commandBuffer);
    }

    mUserInterface.render(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        Logger::log(1, "%s error: failed to end command buffer\n", __FUNCTION__);
        return false;
    }


    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mRenderData.rdPresentSemaphores[mCurrentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &mRenderData.rdRenderSemaphores[imageIndex];
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, mRenderData.rdRenderFences[mCurrentFrame]) != VK_SUCCESS) {
        Logger::log(1, "%s error: failed to submit draw command buffer\n", __FUNCTION__);
        return false;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mRenderData.rdRenderSemaphores[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mRenderData.rdVkbSwapchain.swapchain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(mPresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return recreateSwapchain(mRenderData);
    } else {
        if (result != VK_SUCCESS) {
            Logger::log(1, "%s error: failed to present swapchain image\n", __FUNCTION__);
            return false;
        }
    }

    mCurrentFrame = (mCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return true;
}

void VkRenderer::cleanup() {
    Logger::log(1, "%s: Terminating Vulkan Renderer\n", __FUNCTION__);

    mUserInterface.cleanup(mRenderData);

    VkDevice device = mRenderData.rdVkbDevice.device;

    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    mTexture.cleanup(mRenderData);
    mPipeline.cleanup(mRenderData);
    mMesh.cleanup(mRenderData);
    mGltfInstances.clear();
    if (mGltfModel != nullptr) {
        mGltfModel->cleanup(mRenderData);
        mGltfModel.reset();
    }
    mRenderPass.cleanup(mRenderData);

    mFramebuffer.cleanup(mRenderData);

    if (mRenderData.rdDepthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, mRenderData.rdDepthImageView, nullptr);
        mRenderData.rdDepthImageView = VK_NULL_HANDLE;
    }
    if (mRenderData.rdDepthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(mRenderData.rdAllocator, mRenderData.rdDepthImage, mRenderData.rdDepthImageAlloc);
        mRenderData.rdDepthImage = VK_NULL_HANDLE;
        mRenderData.rdDepthImageAlloc = VK_NULL_HANDLE;
    }

    if (!mRenderData.rdCommandBuffers.empty()) {
        vkFreeCommandBuffers(device, mRenderData.rdCommandPool, static_cast<uint32_t>(mRenderData.rdCommandBuffers.size()), mRenderData.rdCommandBuffers.data());
        mRenderData.rdCommandBuffers.clear();
    }
    if (mRenderData.rdCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, mRenderData.rdCommandPool, nullptr);
        mRenderData.rdCommandPool = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < mRenderData.rdPresentSemaphores.size(); ++i) {
        if (mRenderData.rdPresentSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, mRenderData.rdPresentSemaphores[i], nullptr);
        }
        if (mRenderData.rdRenderFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(device, mRenderData.rdRenderFences[i], nullptr);
        }
    }
    mRenderData.rdPresentSemaphores.clear();
    mRenderData.rdRenderFences.clear();

    for (size_t i = 0; i < mRenderData.rdRenderSemaphores.size(); ++i) {
        if (mRenderData.rdRenderSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, mRenderData.rdRenderSemaphores[i], nullptr);
        }
    }
    mRenderData.rdRenderSemaphores.clear();

    if (mRenderData.rdAllocator) {
        vmaDestroyAllocator(mRenderData.rdAllocator);
        mRenderData.rdAllocator = VK_NULL_HANDLE;
    }

    mSwapchain.cleanup(mRenderData);

    if (device) {
        vkDestroyDevice(device, nullptr);
        mRenderData.rdVkbDevice.device = VK_NULL_HANDLE;
    }

    if (mRenderData.rdSurface) {
        vkDestroySurfaceKHR(mRenderData.rdVkbInstance.instance, mRenderData.rdSurface, nullptr);
        mRenderData.rdSurface = VK_NULL_HANDLE;
    }

    if (mRenderData.rdVkbInstance.debug_messenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(mRenderData.rdVkbInstance.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(mRenderData.rdVkbInstance.instance, mRenderData.rdVkbInstance.debug_messenger, nullptr);
        }
        mRenderData.rdVkbInstance.debug_messenger = VK_NULL_HANDLE;
    }

    if (mRenderData.rdVkbInstance.instance) {
        vkDestroyInstance(mRenderData.rdVkbInstance.instance, nullptr);
        mRenderData.rdVkbInstance.instance = VK_NULL_HANDLE;
    }
}

void VkRenderer::handleMouseButtonEvents(int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && button < ImGuiMouseButton_COUNT) {
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }
}

void VkRenderer::handleMousePositionEvents(double xPos, double yPos) {
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)xPos, (float)yPos);

    if (io.WantCaptureMouse) {
        return;
    }

    int mouseMoveRelX = static_cast<int>(xPos) - mMouseXPos;
    int mouseMoveRelY = static_cast<int>(yPos) - mMouseYPos;

    if (mMouseLock) {
        mRenderData.rdViewAzimuth += mouseMoveRelX / 10.0f;
        if (mRenderData.rdViewAzimuth < 0.0f) {
            mRenderData.rdViewAzimuth += 360.0f;
        }
        if (mRenderData.rdViewAzimuth >= 360.0f) {
            mRenderData.rdViewAzimuth -= 360.0f;
        }

        mRenderData.rdViewElevation -= mouseMoveRelY / 10.0f;
        if (mRenderData.rdViewElevation > 89.0f) {
            mRenderData.rdViewElevation = 89.0f;
        }
        if (mRenderData.rdViewElevation < -89.0f) {
            mRenderData.rdViewElevation = -89.0f;
        }
    }

    mMouseXPos = static_cast<int>(xPos);
    mMouseYPos = static_cast<int>(yPos);
}

void VkRenderer::handleMovementKeys() {
    mRenderData.rdMoveForward = 0;
    mRenderData.rdMoveRight = 0;
    mRenderData.rdMoveUp = 0;

    if (mMouseLock) {
        if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS) {
            mRenderData.rdMoveForward += 1;
        }
        if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS) {
            mRenderData.rdMoveForward -= 1;
        }

        if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS) {
            mRenderData.rdMoveRight -= 1;
        }
        if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS) {
            mRenderData.rdMoveRight += 1;
        }

        if (glfwGetKey(mWindow, GLFW_KEY_E) == GLFW_PRESS) {
            mRenderData.rdMoveUp += 1;
        }
        if (glfwGetKey(mWindow, GLFW_KEY_Q) == GLFW_PRESS) {
            mRenderData.rdMoveUp -= 1;
        }
    }
}
