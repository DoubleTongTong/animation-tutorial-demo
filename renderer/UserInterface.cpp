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

glm::vec2 projectWorldToScreen(const glm::vec3& worldPos, const glm::mat4& viewProj, float width, float height) {
    glm::vec4 clipPos = viewProj * glm::vec4(worldPos, 1.0f);
    if (clipPos.w <= 0.0f) {
        return glm::vec2(-1.0f, -1.0f);
    }
    glm::vec3 ndcPos = glm::vec3(clipPos) / clipPos.w;
    float screenX = (ndcPos.x + 1.0f) * 0.5f * width;
    float screenY = (ndcPos.y + 1.0f) * 0.5f * height;
    return glm::vec2(screenX, screenY);
}

void UserInterface::createFrame(VkRenderData &renderData, const glm::mat4& viewProj) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 绘制 Target Position 的 3D 坐标轴投影
    if (renderData.rdIkMode != ikMode::off) {
        float screenW = static_cast<float>(renderData.rdVkbSwapchain.extent.width);
        float screenH = static_cast<float>(renderData.rdVkbSwapchain.extent.height);

        glm::vec3 center = renderData.rdIkTargetPos;
        glm::vec3 axisX = center + glm::vec3(0.2f, 0.0f, 0.0f);
        glm::vec3 axisY = center + glm::vec3(0.0f, 0.2f, 0.0f);
        glm::vec3 axisZ = center + glm::vec3(0.0f, 0.0f, 0.2f);

        glm::vec2 pCenter = projectWorldToScreen(center, viewProj, screenW, screenH);
        glm::vec2 pX = projectWorldToScreen(axisX, viewProj, screenW, screenH);
        glm::vec2 pY = projectWorldToScreen(axisY, viewProj, screenW, screenH);
        glm::vec2 pZ = projectWorldToScreen(axisZ, viewProj, screenW, screenH);

        if (pCenter.x >= 0.0f) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();

            // X 轴 - 红色
            if (pX.x >= 0.0f) {
                drawList->AddLine(ImVec2(pCenter.x, pCenter.y), ImVec2(pX.x, pX.y), IM_COL32(255, 60, 60, 255), 3.0f);
                drawList->AddText(ImVec2(pX.x + 5, pX.y - 5), IM_COL32(255, 60, 60, 255), "X");
            }
            // Y 轴 - 绿色
            if (pY.x >= 0.0f) {
                drawList->AddLine(ImVec2(pCenter.x, pCenter.y), ImVec2(pY.x, pY.y), IM_COL32(60, 255, 60, 255), 3.0f);
                drawList->AddText(ImVec2(pY.x + 5, pY.y - 5), IM_COL32(60, 255, 60, 255), "Y");
            }
            // Z 轴 - 蓝色
            if (pZ.x >= 0.0f) {
                drawList->AddLine(ImVec2(pCenter.x, pCenter.y), ImVec2(pZ.x, pZ.y), IM_COL32(60, 60, 255, 255), 3.0f);
                drawList->AddText(ImVec2(pZ.x + 5, pZ.y - 5), IM_COL32(60, 60, 255, 255), "Z");
            }

            // 中心交点绘制白点
            drawList->AddCircleFilled(ImVec2(pCenter.x, pCenter.y), 4.0f, IM_COL32(255, 255, 255, 255));
        }
    }

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
    ImGui::Text("Vertex Skinning:");
    ImGui::SameLine();
    if (ImGui::RadioButton("Linear", renderData.rdGPUDualQuatVertexSkinning == skinningMode::linear)) {
        renderData.rdGPUDualQuatVertexSkinning = skinningMode::linear;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Dual Quaternion", renderData.rdGPUDualQuatVertexSkinning == skinningMode::dualQuat)) {
        renderData.rdGPUDualQuatVertexSkinning = skinningMode::dualQuat;
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader("glTF Animation")) {
        std::string curVal = "None";
        if (renderData.rdAnimClip >= 0 && renderData.rdAnimClip < renderData.rdClipNames.size()) {
            curVal = renderData.rdClipNames.at(renderData.rdAnimClip);
        }

        ImGui::Text("Clip Combo");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##ClipCombo", curVal.c_str())) {
            for (int i = 0; i < renderData.rdClipNames.size(); ++i) {
                const bool isSelected = (renderData.rdAnimClip == i);
                std::string selVal = renderData.rdClipNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    renderData.rdAnimClip = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

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

    if (ImGui::CollapsingHeader("glTF Animation Blending")) {
        ImGui::Text("Blend Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Fade In/Out", renderData.rdBlendingMode == blendMode::fadeinout)) {
            renderData.rdBlendingMode = blendMode::fadeinout;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Crossfade", renderData.rdBlendingMode == blendMode::crossfade)) {
            renderData.rdBlendingMode = blendMode::crossfade;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Additive", renderData.rdBlendingMode == blendMode::additive)) {
            renderData.rdBlendingMode = blendMode::additive;
        }

        bool isSingle = (renderData.rdBlendingMode == blendMode::fadeinout);

        if (!isSingle) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Blend Factor");
        ImGui::SameLine();
        ImGui::SliderFloat("##BlendFactor", &renderData.rdAnimBlendFactor, 0.0f, 1.0f);
        if (!isSingle) {
            ImGui::EndDisabled();
        }

        if (isSingle) {
            ImGui::BeginDisabled();
        }

        std::string destCurVal = "None";
        if (renderData.rdCrossBlendDestAnimClip >= 0 && renderData.rdCrossBlendDestAnimClip < renderData.rdClipNames.size()) {
            destCurVal = renderData.rdClipNames.at(renderData.rdCrossBlendDestAnimClip);
        }

        ImGui::Text("Dest Clip   ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##DestClipCombo", destCurVal.c_str())) {
            for (int i = 0; i < renderData.rdClipNames.size(); ++i) {
                const bool isSelected = (renderData.rdCrossBlendDestAnimClip == i);
                std::string selVal = renderData.rdClipNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    renderData.rdCrossBlendDestAnimClip = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Cross Blend ");
        ImGui::SameLine();
        ImGui::SliderFloat("##CrossBlendFactor", &renderData.rdAnimCrossBlendFactor, 0.0f, 1.0f);

        if (isSingle) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        bool isAdditive = (renderData.rdBlendingMode == blendMode::additive);
        if (!isAdditive) {
            ImGui::BeginDisabled();
        }

        std::string splitNodeCurVal = "(invalid)";
        if (renderData.rdSkelSplitNode >= 0 && renderData.rdSkelSplitNode < renderData.rdSkelSplitNodeNames.size()) {
            splitNodeCurVal = renderData.rdSkelSplitNodeNames.at(renderData.rdSkelSplitNode);
        }

        ImGui::Text("Split Node  ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##SplitNodeCombo", splitNodeCurVal.c_str())) {
            for (int i = 0; i < renderData.rdSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (renderData.rdSkelSplitNode == i);
                std::string selVal = renderData.rdSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    renderData.rdSkelSplitNode = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (!isAdditive) {
            ImGui::EndDisabled();
        }
    }

    if (ImGui::CollapsingHeader("Inverse Kinematics")) {
        ImGui::Text("IK Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Off", renderData.rdIkMode == ikMode::off)) {
            renderData.rdIkMode = ikMode::off;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("CCD", renderData.rdIkMode == ikMode::ccd)) {
            renderData.rdIkMode = ikMode::ccd;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("FABRIK", renderData.rdIkMode == ikMode::fabrik)) {
            renderData.rdIkMode = ikMode::fabrik;
        }

        ImGui::Text("Iterations");
        ImGui::SameLine();
        ImGui::SliderInt("##IKIterations", &renderData.rdIkIterations, 1, 50);

        ImGui::Text("Target Pos");
        ImGui::SameLine();
        ImGui::SliderFloat3("##IKTargetPos", &renderData.rdIkTargetPos.x, -5.0f, 5.0f);

        // Combo box for Effector Node
        std::string effectorCurVal = "(invalid)";
        if (renderData.rdIkEffectorNode >= 0 && renderData.rdIkEffectorNode < renderData.rdSkelSplitNodeNames.size()) {
            effectorCurVal = renderData.rdSkelSplitNodeNames.at(renderData.rdIkEffectorNode);
        }
        ImGui::Text("Effector   ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##EffectorCombo", effectorCurVal.c_str())) {
            for (int i = 0; i < renderData.rdSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (renderData.rdIkEffectorNode == i);
                std::string selVal = renderData.rdSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    renderData.rdIkEffectorNode = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Combo box for Root Node
        std::string rootCurVal = "(invalid)";
        if (renderData.rdIkRootNode >= 0 && renderData.rdIkRootNode < renderData.rdSkelSplitNodeNames.size()) {
            rootCurVal = renderData.rdSkelSplitNodeNames.at(renderData.rdIkRootNode);
        }
        ImGui::Text("IK Root    ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##IKRootCombo", rootCurVal.c_str())) {
            for (int i = 0; i < renderData.rdSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (renderData.rdIkRootNode == i);
                std::string selVal = renderData.rdSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    renderData.rdIkRootNode = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
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
