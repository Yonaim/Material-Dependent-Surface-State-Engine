/**
 * @file Window.cpp
 * @brief GLFW 창의 수명 주기와 framebuffer 변경 이벤트.
 */

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

// ============================================================
// 00. Window Lifecycle
// ============================================================
#pragma region 00_Window_Lifecycle

    FWindow::FWindow(std::uint32_t Width, std::uint32_t Height, std::string Title)
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

        FLogger::Info("TApplication",
                        "GLFW window created: " + std::to_string(Width) + "x" + std::to_string(Height) + ".");
    }

FWindow::~FWindow()
{
    if (Handle != nullptr)
    {
        glfwDestroyWindow(Handle);
        Handle = nullptr;
    }

    TerminateGLFW();
}

#pragma endregion

// ============================================================
// 01. Window State and Events
// ============================================================
#pragma region 01_Window_State_and_Events

    bool FWindow::ShouldClose() const
    {
        return glfwWindowShouldClose(Handle) == GLFW_TRUE;
    }

    void FWindow::PollEvents() const
    {
        glfwPollEvents();
    }

    GLFWwindow* FWindow::GetNativeHandle() const noexcept
    {
        return Handle;
    }

    bool FWindow::WasFramebufferResized() const noexcept
    {
        return bFramebufferResized;
    }

    void FWindow::ResetFramebufferResized() noexcept
    {
        bFramebufferResized = false;
    }

    void FWindow::GetFramebufferSize(std::uint32_t& Width, std::uint32_t& Height) const noexcept
    {
        int FramebufferWidth = 0;
        int FramebufferHeight = 0;
        glfwGetFramebufferSize(Handle, &FramebufferWidth, &FramebufferHeight);

        Width = static_cast<std::uint32_t>(std::max(FramebufferWidth, 0));
        Height = static_cast<std::uint32_t>(std::max(FramebufferHeight, 0));
    }

    void FWindow::WaitForNonZeroFramebuffer() const
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

    void FWindow::FramebufferSizeCallback(GLFWwindow* WindowHandle, int Width, int Height)
    {
        auto* WindowInstance = static_cast<FWindow*>(glfwGetWindowUserPointer(WindowHandle));

        if (WindowInstance == nullptr)
        {
            return;
        }

        WindowInstance->bFramebufferResized = true;
        FLogger::Debug("TApplication",
                       "TFramebuffer resize requested: " + std::to_string(std::max(Width, 0)) + "x" +
                           std::to_string(std::max(Height, 0)) + ".");
    }

#pragma endregion

// ============================================================
// 02. GLFW Lifecycle
// ============================================================
#pragma region 02_GLFW_Lifecycle

    void FWindow::InitializeGLFW()
    {
        std::scoped_lock Lock(GLFWMutex);

        if (WindowCount == 0)
        {
            if (glfwInit() != GLFW_TRUE)
            {
                throw std::runtime_error("Failed to initialize GLFW.");
            }

            FLogger::Debug("TApplication", "GLFW initialized.");
        }

        ++WindowCount;
    }

    void FWindow::TerminateGLFW()
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
            FLogger::Verbose("TApplication", "GLFW terminated.");
        }
    }
#pragma endregion
} // namespace MDSS
