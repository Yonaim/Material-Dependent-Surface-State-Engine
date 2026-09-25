/**
 * @file VulkanDevice.h
 * @brief 물리 장치 선택과 논리 장치 생성.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class VulkanDevice
    {
    public:
        /**
         * @brief surface 지원과 필수 기능을 만족하는 physical device를 고르고 logical device를 만든다.
         * @throws std::runtime_error 사용할 수 있는 장치가 없거나 device 생성이 실패한 경우.
         */
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
