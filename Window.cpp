#include "Window.h"
#include "Logger.h"
#include <vector>
#include "VkBootstrap.h"

bool Window::init(int width, int height, const char* title) {
    mApplicationName = title;

    if (!glfwInit()) return false;

    if (!glfwVulkanSupported()) {
        glfwTerminate();
        Logger::log(1, "%s: Vulkan is not supported\n", __FUNCTION__);
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Do not create an OpenGL context
    mWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!mWindow) {
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(mWindow, this);
    glfwSetFramebufferSizeCallback(mWindow, [](GLFWwindow* win, int width, int height) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (thisWindow && thisWindow->mRenderer) {
            thisWindow->mRenderer->setFramebufferResized();
        }
    });
    glfwSetWindowCloseCallback(mWindow, [](GLFWwindow *win) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        thisWindow->handleWindowCloseEvents();
    });

    glfwSetKeyCallback(mWindow, [](GLFWwindow *win, int key, int scancode, int action, int mods) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        thisWindow->handleKeyEvents(key, scancode, action, mods);
    });

    glfwSetMouseButtonCallback(mWindow, [](GLFWwindow *win, int button, int action, int mods) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        thisWindow->handleMouseButtonEvents(button, action, mods);
    });

    glfwSetCursorPosCallback(mWindow, [](GLFWwindow *win, double xpos, double ypos) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        thisWindow->handleCursorPosEvents(xpos, ypos);
    });

    glfwSetCursorEnterCallback(mWindow, [](GLFWwindow *win, int entered) {
        auto thisWindow = static_cast<Window*>(glfwGetWindowUserPointer(win));
        thisWindow->handleCursorEnterEvents(entered);
    });

    mRenderer = std::make_unique<VkRenderer>(mWindow);
    if (!mRenderer->init()) {
        Logger::log(1, "%s: Could not init Vulkan renderer\n", __FUNCTION__);
        glfwDestroyWindow(mWindow);
        glfwTerminate();
        return false;
    }

    Logger::log(1, "%s: Window successfully initialized\n", __FUNCTION__);
    return true;
}

void Window::handleWindowCloseEvents() {
    Logger::log(2, "%s: Window got close event... bye!\n", __FUNCTION__);
}

void Window::handleKeyEvents(int key, int scancode, int action, int mods) {
    std::string actionName;
    switch (action) {
        case GLFW_PRESS:
            actionName = "pressed";
            break;
        case GLFW_RELEASE:
            actionName = "released";
            break;
        case GLFW_REPEAT:
            actionName = "repeated";
            break;
        default:
            actionName = "invalid";
            break;
    }

    const char* keyName = glfwGetKeyName(key, 0);
    Logger::log(2, "%s: key %s (key %i, scancode %i) %s\n",
                __FUNCTION__, keyName ? keyName : "null", key, scancode, actionName.c_str());
}

void Window::handleMouseButtonEvents(int button, int action, int mods) {
    if (mRenderer) {
        mRenderer->handleMouseButtonEvents(button, action, mods);
    }

    std::string actionName;
    switch (action) {
        case GLFW_PRESS:
            actionName = "pressed";
            break;
        case GLFW_RELEASE:
            actionName = "released";
            break;
        default:
            actionName = "invalid";
            break;
    }

    std::string mouseButtonName;
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            mouseButtonName = "left";
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            mouseButtonName = "middle";
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            mouseButtonName = "right";
            break;
        default:
            mouseButtonName = "other";
            break;
    }

    Logger::log(2, "%s: %s mouse button (%i) %s\n",
                __FUNCTION__, mouseButtonName.c_str(), button, actionName.c_str());
}

void Window::handleCursorPosEvents(double xpos, double ypos) {
    if (mRenderer) {
        mRenderer->handleMousePositionEvents(xpos, ypos);
    }
    Logger::log(2, "%s: cursor moved to %f/%f\n", __FUNCTION__, xpos, ypos);
}

void Window::handleCursorEnterEvents(int entered) {
    Logger::log(2, "%s: cursor %s window\n", __FUNCTION__, entered ? "entered" : "left");
}

void Window::mainLoop() {
    while (!glfwWindowShouldClose(mWindow)) {
        if (!mRenderer->draw()) {
            break;
        }
        glfwPollEvents();
    }
}

void Window::cleanup() {
    Logger::log(1, "%s: Terminating Window\n", __FUNCTION__);

    mRenderer.reset();

    if (mWindow) {
        glfwDestroyWindow(mWindow);
        mWindow = nullptr;
    }
    glfwTerminate();
}
