#include "Pipeline.h"
#include "Logger.h"
#include <fstream>
#include <cstdlib>
#include <vector>
#include <cstdint>

#ifndef SHADER_SPV_DIR
#define SHADER_SPV_DIR ""
#endif

std::vector<char> Pipeline::readSpvFile(const std::string& filename) {
    std::string filepath = filename;
    if (std::string(SHADER_SPV_DIR) != "") {
        filepath = std::string(SHADER_SPV_DIR) + filename;
    }

    std::ifstream file(filepath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        file.open(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            Logger::log(1, "Failed to open spv file %s (tried %s and %s)\n",
                        filename.c_str(), filepath.c_str(), filename.c_str());
            return {};
        }
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule Pipeline::createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create shader module\n", __FUNCTION__);
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

bool Pipeline::init(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;

    // 1. Read SPV Files (compiled by CMake at build time)
    auto vertShaderCode = readSpvFile("gltf.vert.spv");
    auto vertDqsShaderCode = readSpvFile("gltf_dqs.vert.spv");
    auto fragShaderCode = readSpvFile("gltf.frag.spv");
    if (vertShaderCode.empty() || vertDqsShaderCode.empty() || fragShaderCode.empty()) {
        return false;
    }

    // 3. Create Shader Modules
    VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
    VkShaderModule vertDqsShaderModule = createShaderModule(device, vertDqsShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);
    if (vertShaderModule == VK_NULL_HANDLE || vertDqsShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        if (vertShaderModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, vertShaderModule, nullptr);
        if (vertDqsShaderModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, vertDqsShaderModule, nullptr);
        if (fragShaderModule != VK_NULL_HANDLE) vkDestroyShaderModule(device, fragShaderModule, nullptr);
        return false;
    }

    // 4. Set Shader Stages Info
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // 5. Create Descriptor Set Layout (Set 0)
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    samplerLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &renderData.rdTextureDescriptorSetLayout) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create descriptor set layout\n", __FUNCTION__);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return false;
    }

    // 5.5 Create Joint Descriptor Set Layout (Set 1)
    VkDescriptorSetLayoutBinding jointLayoutBinding{};
    jointLayoutBinding.binding = 0;
    jointLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    jointLayoutBinding.descriptorCount = 1;
    jointLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    jointLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo jointLayoutInfo{};
    jointLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    jointLayoutInfo.bindingCount = 1;
    jointLayoutInfo.pBindings = &jointLayoutBinding;

    if (vkCreateDescriptorSetLayout(device, &jointLayoutInfo, nullptr, &renderData.rdJointDescriptorSetLayout) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create joint descriptor set layout\n", __FUNCTION__);
        vkDestroyDescriptorSetLayout(device, renderData.rdTextureDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return false;
    }

    // 6. Create Pipeline Layout with Descriptor Set Layout and Push Constants
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = 80; // sizeof(glm::mat4) + sizeof(int) plus padding (aligned to 16 bytes)

    VkDescriptorSetLayout layouts[] = { renderData.rdTextureDescriptorSetLayout, renderData.rdJointDescriptorSetLayout };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = layouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &renderData.rdPipelineLayout) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create pipeline layout\n", __FUNCTION__);
        vkDestroyDescriptorSetLayout(device, renderData.rdJointDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, renderData.rdTextureDescriptorSetLayout, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return false;
    }

    // 7. Vertex Input State
    struct Vertex {
        float pos[3];
        float normal[3];
        float uv[2];
        float jointNum[4];
        float jointWeight[4];
    };

    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[5]{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, uv);

    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, jointNum);

    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[4].offset = offsetof(Vertex, jointWeight);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 5;
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    // 8. Input Assembly State
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // 9. Viewport State (Dynamic Viewport and Scissor)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 10. Rasterization State
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // 11. Multisampling State
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // 12. Depth and Stencil State
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // 13. Color Blend State
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // 14. Dynamic States
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 15. Create Pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = renderData.rdPipelineLayout;
    pipelineInfo.renderPass = renderData.rdRenderpass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &renderData.rdPipeline) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create graphics pipeline\n", __FUNCTION__);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, vertDqsShaderModule, nullptr);
        return false;
    }

    // Create DQS pipeline by swapping vertex shader stage module
    shaderStages[0].module = vertDqsShaderModule;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &renderData.rdPipelineDQS) != VK_SUCCESS) {
        Logger::log(1, "%s error: Failed to create DQS graphics pipeline\n", __FUNCTION__);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, vertDqsShaderModule, nullptr);
        return false;
    }

    // Clean up shader modules
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, vertDqsShaderModule, nullptr);

    return true;
}

void Pipeline::cleanup(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;

    if (renderData.rdPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, renderData.rdPipeline, nullptr);
        renderData.rdPipeline = VK_NULL_HANDLE;
    }

    if (renderData.rdPipelineDQS != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, renderData.rdPipelineDQS, nullptr);
        renderData.rdPipelineDQS = VK_NULL_HANDLE;
    }

    if (renderData.rdPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, renderData.rdPipelineLayout, nullptr);
        renderData.rdPipelineLayout = VK_NULL_HANDLE;
    }

    if (renderData.rdTextureDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, renderData.rdTextureDescriptorSetLayout, nullptr);
        renderData.rdTextureDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (renderData.rdJointDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, renderData.rdJointDescriptorSetLayout, nullptr);
        renderData.rdJointDescriptorSetLayout = VK_NULL_HANDLE;
    }
}
