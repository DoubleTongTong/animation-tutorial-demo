#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include "VkRenderData.h"

class Pipeline {
public:
    bool init(VkRenderData& renderData);
    void cleanup(VkRenderData& renderData);

private:
    VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
    std::vector<char> readSpvFile(const std::string& filename);
};
