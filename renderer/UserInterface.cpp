#include "UserInterface.h"
#include "Logger.h"
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

void UserInterface::init(VkRenderData &renderData, GLFWwindow* window) {
    // Create Descriptor Pool for ImGui
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(renderData.rdVkbDevice.device, &pool_info, nullptr, &mImguiDescriptorPool) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create ImGui descriptor pool\n", __FUNCTION__);
        return;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        Logger::log(1, "%s error: Failed to init ImGui GLFW backend\n", __FUNCTION__);
        return;
    }

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = renderData.rdVkbInstance.instance;
    init_info.PhysicalDevice = renderData.rdVkbDevice.physical_device.physical_device;
    init_info.Device = renderData.rdVkbDevice.device;
    init_info.QueueFamily = renderData.rdVkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    init_info.Queue = renderData.rdGraphicsQueue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = mImguiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = static_cast<uint32_t>(renderData.rdSwapchainImages.size());
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = renderData.rdRenderpass;

    if (!ImGui_ImplVulkan_Init(&init_info)) {
        Logger::log(1, "%s error: Failed to init ImGui Vulkan backend\n", __FUNCTION__);
        return;
    }

    Logger::log(1, "%s: UserInterface successfully initialized\n", __FUNCTION__);
}

void UserInterface::createFrame(VkRenderData &renderData) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags imguiWindowFlags = 0;
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("Control", nullptr, imguiWindowFlags);

    static float newFps = 0.0f;
    if (renderData.rdFrameTime > 0.0f) {
        newFps = 1.0f / renderData.rdFrameTime;
    }
    framesPerSecond = (averagingAlpha * framesPerSecond) + (1.0f - averagingAlpha) * newFps;

    ImGui::Text("FPS:");
    ImGui::SameLine();
    ImGui::Text(std::to_string(framesPerSecond).c_str());
    ImGui::Separator();

    ImGui::Text("Triangles:");
    ImGui::SameLine();
    ImGui::Text(std::to_string(renderData.rdTriangleCount).c_str());

    std::string windowDims =
        std::to_string(renderData.rdVkbSwapchain.extent.width) + "x" +
        std::to_string(renderData.rdVkbSwapchain.extent.height);
    ImGui::Text("Window Dimensions:");
    ImGui::SameLine();
    ImGui::Text(windowDims.c_str());

    ImGui::Text("Field of View");
    ImGui::SameLine();
    ImGui::SliderInt("##FOV", &renderData.rdFieldOfView, 40, 150);

    ImGui::Separator();
    ImGui::Checkbox("Use Dual Quaternion Skinning", &renderData.rdGPUDualQuatVertexSkinning);

    ImGui::Separator();
    if (ImGui::CollapsingHeader("glTF Animation")) {
        ImGui::Text("Clip No");
        ImGui::SameLine();
        int maxClipIdx = renderData.rdAnimClipSize > 0 ? renderData.rdAnimClipSize - 1 : 0;
        ImGui::SliderInt("##Clip", &renderData.rdAnimClip, 0, maxClipIdx);

        ImGui::Text("Clip Name: %s", renderData.rdClipName.c_str());

        ImGui::Checkbox("Play Animation", &renderData.rdPlayAnimation);

        if (!renderData.rdPlayAnimation) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Speed  ");
        ImGui::SameLine();
        ImGui::SliderFloat("##ClipSpeed", &renderData.rdAnimSpeed, 0.0f, 2.0f);
        if (!renderData.rdPlayAnimation) {
            ImGui::EndDisabled();
        }

        if (renderData.rdPlayAnimation) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Timepos");
        ImGui::SameLine();
        ImGui::SliderFloat("##ClipPos", &renderData.rdAnimTimePosition, 0.0f, renderData.rdAnimEndTime);
        if (renderData.rdPlayAnimation) {
            ImGui::EndDisabled();
        }
    }

    ImGui::Text("Camera Position:");
    ImGui::SameLine();
    ImGui::Text("%s", glm::to_string(renderData.rdCameraWorldPosition).c_str());

    ImGui::Text("View Azimuth:");
    ImGui::SameLine();
    ImGui::Text("%s", std::to_string(renderData.rdViewAzimuth).c_str());
    ImGui::Text("View Elevation:");
    ImGui::SameLine();
    ImGui::Text("%s", std::to_string(renderData.rdViewElevation).c_str());

    ImGui::End();
}

void UserInterface::render(VkCommandBuffer commandBuffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void UserInterface::cleanup(VkRenderData &renderData) {
    if (mImguiDescriptorPool != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(renderData.rdVkbDevice.device);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        vkDestroyDescriptorPool(renderData.rdVkbDevice.device, mImguiDescriptorPool, nullptr);
        mImguiDescriptorPool = VK_NULL_HANDLE;
        Logger::log(1, "%s: UserInterface resources cleaned up\n", __FUNCTION__);
    }
}
