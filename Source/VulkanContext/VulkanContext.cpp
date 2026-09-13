#include "VulkanContext/VulkanContext.h"

#include "Application/Window.h"
#include "Logger/Logger.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <stdexcept>

namespace MDSS
{
    VulkanContext::VulkanContext(const Window& Window)
        : Instance("MDSS Engine", RequiredInstanceExtensions()), Surface(CreateSurface(Instance.GetHandle(), Window)),
          Device(Instance.GetHandle(), Surface), Queues(Device.GetPhysicalHandle(), Device.GetHandle(), Surface),
          Commands(Device.GetHandle(), Queues.GetFamilyIndices().GraphicsFamily.value())
    {
        const auto& Families = Queues.GetFamilyIndices();
        Logger::Info("Vulkan", "Graphics/present queues acquired (graphics family=" +
                                   std::to_string(Families.GraphicsFamily.value()) + ", present family=" +
                                   std::to_string(Families.PresentFamily.value()) + ").");
        Logger::Info("Vulkan", "Graphics command pool created.");
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

    bool VulkanContext::SupportsGeometryShader() const noexcept
    {
        return Device.SupportsGeometryShader();
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

        Logger::Debug("Vulkan", "GLFW requested " + std::to_string(ExtensionCount) + " Vulkan instance extensions.");
        return {Extensions, Extensions + ExtensionCount};
    }

    VkSurfaceKHR VulkanContext::CreateSurface(VkInstance Instance, const Window& Window)
    {
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        if (glfwCreateWindowSurface(Instance, Window.GetNativeHandle(), nullptr, &Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan window surface.");
        }

        Logger::Info("Vulkan", "Window surface created.");
        return Surface;
    }
} // namespace MDSS
