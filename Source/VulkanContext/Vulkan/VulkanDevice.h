#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class VulkanDevice
    {
    public:
        VulkanDevice(VkInstance Instance, VkSurfaceKHR Surface);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;
        VulkanDevice(VulkanDevice&&) = delete;
        VulkanDevice& operator=(VulkanDevice&&) = delete;

        [[nodiscard]] VkPhysicalDevice GetPhysicalHandle() const noexcept;
        [[nodiscard]] VkDevice         GetHandle() const noexcept;
        [[nodiscard]] bool             SupportsGeometryShader() const noexcept;

    private:
        static bool IsDeviceSuitable(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);
        static bool SupportsRequiredExtensions(VkPhysicalDevice PhysicalDevice);
        static bool HasAdequateSwapchainSupport(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);
        static bool HasDeviceExtension(VkPhysicalDevice PhysicalDevice, const char* ExtensionName);

        static std::vector<const char*> BuildDeviceExtensions(VkPhysicalDevice PhysicalDevice);

        VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
        VkDevice         Device = VK_NULL_HANDLE;
        bool             bGeometryShaderSupported = false;
    };
} // namespace MDSS
