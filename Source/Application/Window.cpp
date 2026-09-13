#include "Application/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <mutex>
#include <stdexcept>
#include <utility>

namespace
{
std::mutex g_glfwMutex;
std::uint32_t g_windowCount = 0;
} // namespace

namespace mdssp
{
Window::Window(std::uint32_t width, std::uint32_t height, std::string title)
{
    initializeGLFW();

    // Do not create an OpenGL/OpenGL ES context.
    // The same GLFW window can later be used to create a Vulkan surface.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    handle_ = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title.c_str(),
        nullptr,
        nullptr);

    if (handle_ == nullptr)
    {
        terminateGLFW();
        throw std::runtime_error("Failed to create GLFW window.");
    }
}

Window::~Window()
{
    if (handle_ != nullptr)
    {
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
    }

    terminateGLFW();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(handle_) == GLFW_TRUE;
}

void Window::pollEvents() const
{
    glfwPollEvents();
}

GLFWwindow* Window::nativeHandle() const noexcept
{
    return handle_;
}

void Window::initializeGLFW()
{
    std::scoped_lock lock(g_glfwMutex);

    if (g_windowCount == 0)
    {
        if (glfwInit() != GLFW_TRUE)
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }
    }

    ++g_windowCount;
}

void Window::terminateGLFW()
{
    std::scoped_lock lock(g_glfwMutex);

    if (g_windowCount == 0)
    {
        return;
    }

    --g_windowCount;

    if (g_windowCount == 0)
    {
        glfwTerminate();
    }
}
} // namespace mdssp
