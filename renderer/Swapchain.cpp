#include "Swapchain.h"
#include "Logger.h"

bool Swapchain::init(VkRenderData& renderData, GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    vkb::SwapchainBuilder swapchain_builder{ renderData.rdVkbDevice };
    auto swap_ret = swapchain_builder.set_desired_extent(width, height)
                                     .build();
    if (!swap_ret) {
        Logger::log(1, "%s error: Failed to create Swapchain. Error: %s\n", __FUNCTION__, swap_ret.error().message().c_str());
        return false;
    }
    renderData.rdVkbSwapchain = swap_ret.value();

    auto images_ret = renderData.rdVkbSwapchain.get_images();
    if (images_ret) {
        renderData.rdSwapchainImages = images_ret.value();
    } else {
        Logger::log(1, "%s error: Failed to get swapchain images\n", __FUNCTION__);
        return false;
    }

    auto views_ret = renderData.rdVkbSwapchain.get_image_views();
    if (views_ret) {
        renderData.rdSwapchainImageViews = views_ret.value();
    } else {
        Logger::log(1, "%s error: Failed to get swapchain image views\n", __FUNCTION__);
        return false;
    }

    return true;
}

bool Swapchain::recreate(VkRenderData& renderData, GLFWwindow* window) {
    VkDevice device = renderData.rdVkbDevice.device;

    // Destroy old swapchain image views
    for (auto imageView : renderData.rdSwapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    renderData.rdSwapchainImageViews.clear();
    renderData.rdSwapchainImages.clear();

    vkb::Swapchain oldSwapchain = renderData.rdVkbSwapchain;

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkb::SwapchainBuilder swapchain_builder{ renderData.rdVkbDevice };
    auto swap_ret = swapchain_builder.set_desired_extent(width, height)
                                     .set_old_swapchain(oldSwapchain)
                                     .build();
    if (!swap_ret) {
        Logger::log(1, "%s error: Failed to recreate Swapchain. Error: %s\n", __FUNCTION__, swap_ret.error().message().c_str());
        return false;
    }

    // Destroy old swapchain
    vkb::destroy_swapchain(oldSwapchain);

    renderData.rdVkbSwapchain = swap_ret.value();

    auto images_ret = renderData.rdVkbSwapchain.get_images();
    if (images_ret) {
        renderData.rdSwapchainImages = images_ret.value();
    } else {
        Logger::log(1, "%s error: Failed to get swapchain images during recreation\n", __FUNCTION__);
        return false;
    }

    auto views_ret = renderData.rdVkbSwapchain.get_image_views();
    if (views_ret) {
        renderData.rdSwapchainImageViews = views_ret.value();
    } else {
        Logger::log(1, "%s error: Failed to get swapchain image views during recreation\n", __FUNCTION__);
        return false;
    }

    return true;
}

void Swapchain::cleanup(VkRenderData& renderData) {
    VkDevice device = renderData.rdVkbDevice.device;

    for (auto imageView : renderData.rdSwapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    renderData.rdSwapchainImageViews.clear();
    renderData.rdSwapchainImages.clear();

    if (renderData.rdVkbSwapchain.swapchain != VK_NULL_HANDLE) {
        vkb::destroy_swapchain(renderData.rdVkbSwapchain);
        renderData.rdVkbSwapchain.swapchain = VK_NULL_HANDLE;
    }
}
