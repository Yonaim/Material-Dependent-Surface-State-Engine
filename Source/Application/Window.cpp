#include "Application/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <mutex>
#include <stdexcept>

namespace MDSS
{
    namespace
    {
        std::mutex    GLFWMutex;
        std::uint32_t WindowCount = 0;
    } // namespace

    Window::Window(std::uint32_t Width, std::uint32_t Height, std::string Title)
    {
        InitializeGLFW();

        // Do not create an OpenGL/OpenGL ES context.
        // The same GLFW window can later be used to create a Vulkan surface.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        Handle = glfwCreateWindow(static_cast<int>(Width), static_cast<int>(Height), Title.c_str(), nullptr, nullptr);

        if (Handle == nullptr)
        {
            TerminateGLFW();
            throw std::runtime_error("Failed to create GLFW window.");
        }
    }

    Window::~Window()
    {
        if (Handle != nullptr)
        {
            glfwDestroyWindow(Handle);
            Handle = nullptr;
        }

        TerminateGLFW();
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(Handle) == GLFW_TRUE;
    }

    void Window::PollEvents() const
    {
        glfwPollEvents();
    }

    GLFWwindow* Window::GetNativeHandle() const noexcept
    {
        return Handle;
    }

    void Window::InitializeGLFW()
    {
        std::scoped_lock Lock(GLFWMutex);

        if (WindowCount == 0)
        {
            if (glfwInit() != GLFW_TRUE)
            {
                throw std::runtime_error("Failed to initialize GLFW.");
            }
        }

        ++WindowCount;
    }

    void Window::TerminateGLFW()
    {
        std::scoped_lock Lock(GLFWMutex);

        if (WindowCount == 0)
        {
            return;
        }

        --WindowCount;

        if (WindowCount == 0)
        {
            glfwTerminate();
        }
    }
} // namespace MDSS
