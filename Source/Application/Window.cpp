#include "Application/Window.h"

#include "Logger/Logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
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

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        Handle = glfwCreateWindow(static_cast<int>(Width), static_cast<int>(Height), Title.c_str(), nullptr, nullptr);

        if (Handle == nullptr)
        {
            TerminateGLFW();
            throw std::runtime_error("Failed to create GLFW window.");
        }

        glfwSetWindowUserPointer(Handle, this);
        glfwSetFramebufferSizeCallback(Handle, FramebufferSizeCallback);

        Logger::Info("Application",
                     "GLFW window created: " + std::to_string(Width) + "x" + std::to_string(Height) + ".");
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

    bool Window::WasFramebufferResized() const noexcept
    {
        return bFramebufferResized;
    }

    void Window::ResetFramebufferResized() noexcept
    {
        bFramebufferResized = false;
    }

    void Window::GetFramebufferSize(std::uint32_t& Width, std::uint32_t& Height) const noexcept
    {
        int FramebufferWidth = 0;
        int FramebufferHeight = 0;
        glfwGetFramebufferSize(Handle, &FramebufferWidth, &FramebufferHeight);

        Width = static_cast<std::uint32_t>(std::max(FramebufferWidth, 0));
        Height = static_cast<std::uint32_t>(std::max(FramebufferHeight, 0));
    }

    void Window::WaitForNonZeroFramebuffer() const
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        GetFramebufferSize(Width, Height);

        while ((Width == 0 || Height == 0) && !ShouldClose())
        {
            glfwWaitEvents();
            GetFramebufferSize(Width, Height);
        }
    }

    void Window::FramebufferSizeCallback(GLFWwindow* WindowHandle, int Width, int Height)
    {
        auto* WindowInstance = static_cast<Window*>(glfwGetWindowUserPointer(WindowHandle));
        if (WindowInstance == nullptr)
        {
            return;
        }

        WindowInstance->bFramebufferResized = true;
        Logger::Debug("Application",
                      "Framebuffer resize requested: " + std::to_string(std::max(Width, 0)) + "x" +
                          std::to_string(std::max(Height, 0)) + ".");
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
            Logger::Debug("Application", "GLFW initialized.");
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
            Logger::Verbose("Application", "GLFW terminated.");
        }
    }
} // namespace MDSS
