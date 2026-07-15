#include "UserInterface.h"
#include "Logger.h"
#include <string>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include "ModelSettings.h"

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

void UserInterface::createFrame(VkRenderData &renderData, ModelSettings &settings, const glm::mat4& viewProj) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // 绘制 Target Position 的 3D 坐标轴投影
    if (settings.msIkMode != ikMode::off) {
        float screenW = static_cast<float>(renderData.rdVkbSwapchain.extent.width);
        float screenH = static_cast<float>(renderData.rdVkbSwapchain.extent.height);

        glm::vec3 center = settings.msIkTargetWorldPos;
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
    if (ImGui::RadioButton("Linear", settings.msVertexSkinningMode == skinningMode::linear)) {
        settings.msVertexSkinningMode = skinningMode::linear;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Dual Quaternion", settings.msVertexSkinningMode == skinningMode::dualQuat)) {
        settings.msVertexSkinningMode = skinningMode::dualQuat;
    }

    ImGui::Separator();

    // glTF 实例切换控制栏
    if (ImGui::CollapsingHeader("glTF Instances")) {
        ImGui::Text("Selected Instance");
        ImGui::SameLine();
        if (ImGui::ArrowButton("##Prev", ImGuiDir_Left)) {
            if (renderData.rdCurrentSelectedInstance > 0) {
                renderData.rdCurrentSelectedInstance--;
            }
        }
        ImGui::SameLine();
        ImGui::Text("%d", renderData.rdCurrentSelectedInstance);
        ImGui::SameLine();
        if (ImGui::ArrowButton("##Next", ImGuiDir_Right)) {
            if (renderData.rdCurrentSelectedInstance < renderData.rdNumberOfInstances - 1) {
                renderData.rdCurrentSelectedInstance++;
            }
        }

        ImGui::SliderFloat2("World Position", &settings.msWorldPosition.x, -20.0f, 20.0f);
        ImGui::SliderFloat3("World Rotation", &settings.msWorldRotation.x, -180.0f, 180.0f);
        ImGui::Checkbox("Draw Model", &settings.msDrawModel);
    }

    if (ImGui::CollapsingHeader("glTF Animation")) {
        std::string curVal = "None";
        if (settings.msAnimClip >= 0 && settings.msAnimClip < settings.msClipNames.size()) {
            curVal = settings.msClipNames.at(settings.msAnimClip);
        }

        ImGui::Text("Clip Combo");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##ClipCombo", curVal.c_str())) {
            for (int i = 0; i < settings.msClipNames.size(); ++i) {
                const bool isSelected = (settings.msAnimClip == i);
                std::string selVal = settings.msClipNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    settings.msAnimClip = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Checkbox("Play Animation", &settings.msPlayAnimation);

        if (!settings.msPlayAnimation) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Speed  ");
        ImGui::SameLine();
        ImGui::SliderFloat("##ClipSpeed", &settings.msAnimSpeed, 0.0f, 2.0f);
        if (!settings.msPlayAnimation) {
            ImGui::EndDisabled();
        }

        if (settings.msPlayAnimation) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Timepos");
        ImGui::SameLine();
        ImGui::SliderFloat("##ClipPos", &settings.msAnimTimePosition, 0.0f, settings.msAnimEndTime);
        if (settings.msPlayAnimation) {
            ImGui::EndDisabled();
        }
    }

    if (ImGui::CollapsingHeader("glTF Animation Blending")) {
        ImGui::Text("Blend Mode:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Fade In/Out", settings.msBlendingMode == blendMode::fadeinout)) {
            settings.msBlendingMode = blendMode::fadeinout;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Crossfade", settings.msBlendingMode == blendMode::crossfade)) {
            settings.msBlendingMode = blendMode::crossfade;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Additive", settings.msBlendingMode == blendMode::additive)) {
            settings.msBlendingMode = blendMode::additive;
        }

        bool isSingle = (settings.msBlendingMode == blendMode::fadeinout);

        if (!isSingle) {
            ImGui::BeginDisabled();
        }
        ImGui::Text("Blend Factor");
        ImGui::SameLine();
        ImGui::SliderFloat("##BlendFactor", &settings.msAnimBlendFactor, 0.0f, 1.0f);
        if (!isSingle) {
            ImGui::EndDisabled();
        }

        if (isSingle) {
            ImGui::BeginDisabled();
        }

        std::string destCurVal = "None";
        if (settings.msCrossBlendDestAnimClip >= 0 && settings.msCrossBlendDestAnimClip < settings.msClipNames.size()) {
            destCurVal = settings.msClipNames.at(settings.msCrossBlendDestAnimClip);
        }

        ImGui::Text("Dest Clip   ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##DestClipCombo", destCurVal.c_str())) {
            for (int i = 0; i < settings.msClipNames.size(); ++i) {
                const bool isSelected = (settings.msCrossBlendDestAnimClip == i);
                std::string selVal = settings.msClipNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    settings.msCrossBlendDestAnimClip = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Cross Blend ");
        ImGui::SameLine();
        ImGui::SliderFloat("##CrossBlendFactor", &settings.msAnimCrossBlendFactor, 0.0f, 1.0f);

        if (isSingle) {
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        bool isAdditive = (settings.msBlendingMode == blendMode::additive);
        if (!isAdditive) {
            ImGui::BeginDisabled();
        }

        std::string splitNodeCurVal = "(invalid)";
        if (settings.msSkelSplitNode >= 0 && settings.msSkelSplitNode < settings.msSkelSplitNodeNames.size()) {
            splitNodeCurVal = settings.msSkelSplitNodeNames.at(settings.msSkelSplitNode);
        }

        ImGui::Text("Split Node  ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##SplitNodeCombo", splitNodeCurVal.c_str())) {
            for (int i = 0; i < settings.msSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (settings.msSkelSplitNode == i);
                std::string selVal = settings.msSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    settings.msSkelSplitNode = i;
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
        if (ImGui::RadioButton("Off", settings.msIkMode == ikMode::off)) {
            settings.msIkMode = ikMode::off;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("CCD", settings.msIkMode == ikMode::ccd)) {
            settings.msIkMode = ikMode::ccd;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("FABRIK", settings.msIkMode == ikMode::fabrik)) {
            settings.msIkMode = ikMode::fabrik;
        }

        ImGui::Text("Iterations");
        ImGui::SameLine();
        ImGui::SliderInt("##IKIterations", &settings.msIkIterations, 1, 50);

        ImGui::Text("Target Pos");
        ImGui::SameLine();
        ImGui::SliderFloat3("##IKTargetPos", &settings.msIkTargetPos.x, -5.0f, 5.0f);

        // Combo box for Effector Node
        std::string effectorCurVal = "(invalid)";
        if (settings.msIkEffectorNode >= 0 && settings.msIkEffectorNode < settings.msSkelSplitNodeNames.size()) {
            effectorCurVal = settings.msSkelSplitNodeNames.at(settings.msIkEffectorNode);
        }
        ImGui::Text("Effector   ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##EffectorCombo", effectorCurVal.c_str())) {
            for (int i = 0; i < settings.msSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (settings.msIkEffectorNode == i);
                std::string selVal = settings.msSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    settings.msIkEffectorNode = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Combo box for Root Node
        std::string rootCurVal = "(invalid)";
        if (settings.msIkRootNode >= 0 && settings.msIkRootNode < settings.msSkelSplitNodeNames.size()) {
            rootCurVal = settings.msSkelSplitNodeNames.at(settings.msIkRootNode);
        }
        ImGui::Text("IK Root    ");
        ImGui::SameLine();
        if (ImGui::BeginCombo("##IKRootCombo", rootCurVal.c_str())) {
            for (int i = 0; i < settings.msSkelSplitNodeNames.size(); ++i) {
                const bool isSelected = (settings.msIkRootNode == i);
                std::string selVal = settings.msSkelSplitNodeNames.at(i);
                if (ImGui::Selectable(selVal.c_str(), isSelected)) {
                    settings.msIkRootNode = i;
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
