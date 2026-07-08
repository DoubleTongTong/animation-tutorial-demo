#pragma once
#include <string>
#include <vector>
#include <memory>
#include <vulkan/vulkan.h>
#include "renderer/VkRenderer.h"

class Window {
public:
    bool init(int width, int height, const char* title);
    void mainLoop();
    void cleanup();
    void handleKeyEvents(int key, int scancode, int action, int mods);

private:
    void handleWindowCloseEvents();
    void handleMouseButtonEvents(int button, int action, int mods);
    void handleCursorPosEvents(double xpos, double ypos);
    void handleCursorEnterEvents(int entered);

    GLFWwindow *mWindow = nullptr;
    std::string mApplicationName;
    std::unique_ptr<VkRenderer> mRenderer;
};
