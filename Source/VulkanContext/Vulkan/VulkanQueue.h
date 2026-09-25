/**
 * @file VulkanQueue.h
 * @brief graphics·present queue family 검색과 queue 접근.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace MDSS
{
    struct QueueFamilyIndices
    {
        std::optional<std::uint32_t> GraphicsFamily;
        std::optional<std::uint32_t> PresentFamily;

        [[nodiscard]] bool Complete() const noexcept
        {
            return GraphicsFamily.has_value() && PresentFamily.has_value();
        }
    };

    class VulkanQueue
    {
    public:
        VulkanQueue(VkPhysicalDevice PhysicalDevice, VkDevice Device, VkSurfaceKHR Surface);

        [[nodiscard]] VkQueue                   GetGraphics() const noexcept;
        [[nodiscard]] VkQueue                   GetPresent() const noexcept;
        [[nodiscard]] const QueueFamilyIndices& GetFamilyIndices() const noexcept;

        /** @brief graphics와 surface presentation을 지원하는 queue family index를 찾는다. */
        [[nodiscard]] static QueueFamilyIndices FindFamilies(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface);

    private:
        QueueFamilyIndices FamilyIndices;
        VkQueue            GraphicsQueue = VK_NULL_HANDLE;
        VkQueue            PresentQueue = VK_NULL_HANDLE;
    };
} // namespace MDSS
