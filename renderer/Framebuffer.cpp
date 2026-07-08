#include "Framebuffer.h"
#include "Logger.h"

bool Framebuffer::init(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;
    renderData.rdFramebuffers.resize(renderData.rdSwapchainImageViews.size());

    for (size_t i = 0; i < renderData.rdSwapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
            renderData.rdSwapchainImageViews[i],
            renderData.rdDepthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderData.rdRenderpass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = renderData.rdVkbSwapchain.extent.width;
        framebufferInfo.height = renderData.rdVkbSwapchain.extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &renderData.rdFramebuffers[i]) != VK_SUCCESS) {
            Logger::log(1, "%s error: Failed to create framebuffer %zu\n", __FUNCTION__, i);
            return false;
        }
    }

    return true;
}

void Framebuffer::cleanup(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;
    if (device == VK_NULL_HANDLE) {
        return;
    }

    for (auto framebuffer : renderData.rdFramebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
    }
    renderData.rdFramebuffers.clear();
}
