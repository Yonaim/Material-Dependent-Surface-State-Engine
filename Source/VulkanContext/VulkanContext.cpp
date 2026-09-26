/**
 * @file VulkanContext.cpp
 * @brief Vulkan instance·device·queue·command 자원 통합 수명 주기.
 */

#include "VulkanContext/VulkanContext.h"

#include "Application/Window.h"
#include "Logger/Logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>

namespace MDSS
{
    TVulkanContext::TVulkanContext(const TWindow& TWindow)
        : Instance("MDSS Engine", RequiredInstanceExtensions()), Surface(CreateSurface(Instance.GetHandle(), TWindow)),
          Device(Instance.GetHandle(), Surface), Queues(Device.GetPhysicalHandle(), Device.GetHandle(), Surface),
          Commands(Device.GetHandle(), Queues.GetFamilyIndices().GraphicsFamily.value())
    {
        const auto& Families = Queues.GetFamilyIndices();
        TLogger::Info(
            "Vulkan",
            "Graphics/compute/present queues acquired (graphics family=" +
                std::to_string(Families.GraphicsFamily.value()) +
                ", present family=" + std::to_string(Families.PresentFamily.value()) + ").");
        TLogger::Info("Vulkan", "Graphics command pool created.");
    }

    TVulkanContext::~TVulkanContext()
    {
        if (Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(Instance.GetHandle(), Surface, nullptr);
            Surface = VK_NULL_HANDLE;
        }
    }

    VkInstance TVulkanContext::GetInstance() const noexcept
    {
        return Instance.GetHandle();
    }

    VkSurfaceKHR TVulkanContext::GetSurface() const noexcept
    {
        return Surface;
    }

    VkPhysicalDevice TVulkanContext::GetPhysicalDevice() const noexcept
    {
        return Device.GetPhysicalHandle();
    }

    VkDevice TVulkanContext::GetDevice() const noexcept
    {
        return Device.GetHandle();
    }

    bool TVulkanContext::SupportsGeometryShader() const noexcept
    {
        return Device.SupportsGeometryShader();
    }

    const TVulkanQueue& TVulkanContext::GetQueues() const noexcept
    {
        return Queues;
    }

    const TVulkanCommand& TVulkanContext::GetCommands() const noexcept
    {
        return Commands;
    }

    std::vector<const char*> TVulkanContext::RequiredInstanceExtensions()
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

        TLogger::Debug("Vulkan", "GLFW requested " + std::to_string(ExtensionCount) + " Vulkan instance extensions.");
        return {Extensions, Extensions + ExtensionCount};
    }

    VkSurfaceKHR TVulkanContext::CreateSurface(VkInstance Instance, const TWindow& TWindow)
    {
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        if (glfwCreateWindowSurface(Instance, TWindow.GetNativeHandle(), nullptr, &Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan window surface.");
        }

        TLogger::Info("Vulkan", "TWindow surface created.");
        return Surface;
    }
} // namespace MDSS
