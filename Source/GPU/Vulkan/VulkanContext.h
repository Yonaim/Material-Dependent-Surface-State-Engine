/**
 * @file VulkanContext.h
 * @brief Vulkan instance·device·queue·command 자원 통합 수명 주기.
 */

#pragma once

#include "GPU/Vulkan/Command/VulkanCommand.h"
#include "GPU/Vulkan/VulkanDevice.h"
#include "GPU/Vulkan/VulkanInstance.h"
#include "GPU/Vulkan/VulkanQueue.h"

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class FWindow;
}

namespace MDSS::GPU
{

    class FVulkanContext
    {
    public:
        // Vulkan context lifecycle
        /** @brief FWindow surface와 Vulkan 실행에 필요한 핵심 자원을 순서대로 초기화한다. */
        explicit FVulkanContext(const FWindow& FWindow);
        ~FVulkanContext();

        FVulkanContext(const FVulkanContext&) = delete;
        FVulkanContext& operator=(const FVulkanContext&) = delete;
        FVulkanContext(FVulkanContext&&) = delete;
        FVulkanContext& operator=(FVulkanContext&&) = delete;

        // Instance, device, queue, and command access
        [[nodiscard]] VkInstance            GetInstance() const noexcept;
        [[nodiscard]] VkSurfaceKHR          GetSurface() const noexcept;
        [[nodiscard]] VkPhysicalDevice      GetPhysicalDevice() const noexcept;
        [[nodiscard]] VkDevice              GetDevice() const noexcept;
        [[nodiscard]] bool                  SupportsGeometryShader() const noexcept;
        [[nodiscard]] const FVulkanQueue&   GetQueues() const noexcept;
        [[nodiscard]] const FVulkanCommand& GetCommands() const noexcept;

    private:
        // Platform extension and presentation surface setup
        /** @brief GLFW가 현재 platform에서 요구하는 Vulkan instance extension을 반환한다. */
        static std::vector<const char*> RequiredInstanceExtensions();
        /** @brief GLFW native window에 대응하는 Vulkan presentation surface를 생성한다. */
        static VkSurfaceKHR CreateSurface(VkInstance Instance, const FWindow& FWindow);

        FVulkanInstance Instance;
        VkSurfaceKHR    Surface = VK_NULL_HANDLE;
        FVulkanDevice   Device;
        FVulkanQueue    Queues;
        FVulkanCommand  Commands;
    };
} // namespace MDSS::GPU
