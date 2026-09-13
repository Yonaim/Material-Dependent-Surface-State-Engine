#pragma once

#include "VulkanContext/Vulkan/VulkanDevice.h"
#include "VulkanContext/Vulkan/VulkanInstance.h"
#include "VulkanContext/Vulkan/VulkanQueue.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class Window;

    class VulkanContext
    {
    public:
        explicit VulkanContext(const Window& Window);
        ~VulkanContext();

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        [[nodiscard]] VkInstance         GetInstance() const noexcept;
        [[nodiscard]] VkSurfaceKHR       GetSurface() const noexcept;
        [[nodiscard]] VkPhysicalDevice   GetPhysicalDevice() const noexcept;
        [[nodiscard]] VkDevice           GetDevice() const noexcept;
        [[nodiscard]] const VulkanQueue& GetQueues() const noexcept;

    private:
        static std::vector<const char*> RequiredInstanceExtensions();
        static VkSurfaceKHR             CreateSurface(VkInstance Instance, const Window& Window);

        VulkanInstance Instance;
        VkSurfaceKHR   Surface = VK_NULL_HANDLE;
        VulkanDevice   Device;
        VulkanQueue    Queues;
    };
} // namespace MDSS
