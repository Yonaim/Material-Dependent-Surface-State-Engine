#include "VulkanContext/VulkanContext.h"

#include "Application/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace MDSS
{
    VulkanContext::VulkanContext(const Window& Window)
        : Instance("MDSSP Engine", RequiredInstanceExtensions()), Surface(CreateSurface(Instance.GetHandle(), Window)),
          Device(Instance.GetHandle(), Surface), Queues(Device.GetPhysicalHandle(), Device.GetHandle(), Surface),
          Commands(Device.GetHandle(), Queues.GetFamilyIndices().GraphicsFamily.value())
    {
        std::cout << "[Vulkan] Graphics and present queues acquired.\n";
        std::cout << "[Vulkan] Command pool created.\n";
    }

    VulkanContext::~VulkanContext()
    {
        if (Surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(Instance.GetHandle(), Surface, nullptr);
            Surface = VK_NULL_HANDLE;
        }
    }

    VkInstance VulkanContext::GetInstance() const noexcept
    {
        return Instance.GetHandle();
    }

    VkSurfaceKHR VulkanContext::GetSurface() const noexcept
    {
        return Surface;
    }

    VkPhysicalDevice VulkanContext::GetPhysicalDevice() const noexcept
    {
        return Device.GetPhysicalHandle();
    }

    VkDevice VulkanContext::GetDevice() const noexcept
    {
        return Device.GetHandle();
    }

    const VulkanQueue& VulkanContext::GetQueues() const noexcept
    {
        return Queues;
    }

    const VulkanCommand& VulkanContext::GetCommands() const noexcept
    {
        return Commands;
    }

    std::vector<const char*> VulkanContext::RequiredInstanceExtensions()
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

        return {Extensions, Extensions + ExtensionCount};
    }

    VkSurfaceKHR VulkanContext::CreateSurface(VkInstance Instance, const Window& Window)
    {
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        if (glfwCreateWindowSurface(Instance, Window.GetNativeHandle(), nullptr, &Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan window surface.");
        }

        std::cout << "[Vulkan] Window surface created.\n";
        return Surface;
    }
} // namespace MDSS
