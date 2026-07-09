#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include <GLFW/glfw3.h>
#include "VkRenderData.h"
#include "RenderPass.h"
#include "Mesh.h"
#include "Pipeline.h"
#include "Texture.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "UserInterface.h"
#include "Timer.h"
#include "Camera.h"
#include "GltfModel.h"

class VkRenderer {
public:
    VkRenderer(GLFWwindow* window);
    ~VkRenderer();

    bool init();
    bool draw();
    void cleanup();

    void setFramebufferResized() { mFramebufferResized = true; }

    void handleMouseButtonEvents(int button, int action, int mods);
    void handleMousePositionEvents(double xPos, double yPos);

private:
    bool initVulkan();
    bool initSyncObjects();
    bool initCommandPoolAndBuffer();
    bool initDepthBuffer();
    bool recreateSwapchain(VkRenderData& renderData);
    void handleMovementKeys();

    GLFWwindow* mWindow = nullptr;
    std::string mApplicationName = "AnimationTutorialDemo";

    VkRenderData mRenderData;

    Swapchain mSwapchain;
    RenderPass mRenderPass;
    Mesh mMesh;
    Pipeline mPipeline;
    Texture mTexture;
    Framebuffer mFramebuffer;
    UserInterface mUserInterface;
    Camera mCamera{};
    // glTF 模型智能指针成员变量
    std::shared_ptr<GltfModel> mGltfModel = nullptr;
    bool mMouseLock = false;
    int mMouseXPos = 0;
    int mMouseYPos = 0;
    double mLastTickTime = 0.0;

    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    VkQueue mPresentQueue = VK_NULL_HANDLE;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t mCurrentFrame = 0;
    bool mFramebufferResized = false;
};
