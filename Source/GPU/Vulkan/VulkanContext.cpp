/**
 * @file VulkanContext.cpp
 * @brief Vulkan instance·device·queue·command 자원 통합 수명 주기.
 */

#include "GPU/Vulkan/VulkanContext.h"

#include "Application/Window.h"
#include "Logger/Logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>

namespace MDSS::GPU
{
// ============================================================
// 00. Vulkan Context Lifecycle
// ============================================================
#pragma region 00_Vulkan_Context_Lifecycle

    FVulkanContext::FVulkanContext(const FWindow& FWindow)
        : Instance("MDSS Engine", RequiredInstanceExtensions()), Surface(CreateSurface(Instance.GetHandle(), FWindow)),
          Device(Instance.GetHandle(), Surface), Queues(Device.GetPhysicalHandle(), Device.GetHandle(), Surface),
          Commands(Device.GetHandle(), Queues.GetFamilyIndices().GraphicsFamily.value())
    {
        const auto& Families = Queues.GetFamilyIndices();
        FLogger::Info("Vulkan",
                      "Graphics/compute/present queues acquired (graphics family=" +
                          std::to_string(Families.GraphicsFamily.value()) +
                          ", present family=" + std::to_string(Families.PresentFamily.value()) + ").");
        FLogger::Info("Vulkan", "Graphics command pool created.");
    }

    FVulkanContext::~FVulkanContext()
    {
        if (Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(Instance.GetHandle(), Surface, nullptr);
            Surface = VK_NULL_HANDLE;
        }
    }

#pragma endregion

// ============================================================
// 01. Vulkan Context Accessors
// ============================================================
#pragma region 01_Vulkan_Context_Accessors

    VkInstance FVulkanContext::GetInstance() const noexcept
    {
        return Instance.GetHandle();
    }

    VkSurfaceKHR FVulkanContext::GetSurface() const noexcept
    {
        return Surface;
    }

    VkPhysicalDevice FVulkanContext::GetPhysicalDevice() const noexcept
    {
        return Device.GetPhysicalHandle();
    }

    VkDevice FVulkanContext::GetDevice() const noexcept
    {
        return Device.GetHandle();
    }

    bool FVulkanContext::SupportsGeometryShader() const noexcept
    {
        return Device.SupportsGeometryShader();
    }

    const FVulkanQueue& FVulkanContext::GetQueues() const noexcept
    {
        return Queues;
    }

    const FVulkanCommand& FVulkanContext::GetCommands() const noexcept
    {
        return Commands;
    }

#pragma endregion

// ============================================================
// 02. Instance Extensions and Surface
// ============================================================
#pragma region 02_Instance_Extensions_and_Surface

    std::vector<const char*> FVulkanContext::RequiredInstanceExtensions()
    {
        if (glfwVulkanSupported() != GLFW_TRUE)
        {
            throw std::runtime_error(
                "GLFW could not find a Vulkan loader. Install the Vulkan SDK/runtime and try again.");
        }

        std::uint32_t ExtensionCount = 0;
        const char**  Extensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);

        if (Extensions == nullptr || ExtensionCount == 0)
        {
            throw std::runtime_error("GLFW did not provide the required Vulkan instance extensions.");
        }

        FLogger::Debug("Vulkan", "GLFW requested " + std::to_string(ExtensionCount) + " Vulkan instance extensions.");
        return {Extensions, Extensions + ExtensionCount};
    }

    VkSurfaceKHR FVulkanContext::CreateSurface(VkInstance Instance, const FWindow& FWindow)
    {
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        if (glfwCreateWindowSurface(Instance, FWindow.GetNativeHandle(), nullptr, &Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan window surface.");
        }

        FLogger::Info("Vulkan", "TWindow surface created.");
        return Surface;
    }
#pragma endregion
} // namespace MDSS::GPU
