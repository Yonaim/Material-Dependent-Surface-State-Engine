/**
 * @file Swapchain.h
 * @brief 표면 지원 정보에 따른 swapchain 선택·생성·재생성.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class FWindow;
}

namespace MDSS::GPU
{
    class FVulkanContext;

    struct FSwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR>   PresentModes;
    };

    class FSwapchain
    {
    public:
        // Swapchain lifecycle and recreation
        FSwapchain(const FVulkanContext& Context, const FWindow& FWindow);
        ~FSwapchain();

        FSwapchain(const FSwapchain&) = delete;
        FSwapchain& operator=(const FSwapchain&) = delete;
        FSwapchain(FSwapchain&&) = delete;
        FSwapchain& operator=(FSwapchain&&) = delete;

        /** @brief 현재 surface와 window 크기에 맞춰 swapchain 및 image view를 다시 만든다. */
        void Recreate(const FVulkanContext& Context, const FWindow& FWindow);

        // Swapchain image access
        [[nodiscard]] VkSwapchainKHR GetHandle() const noexcept;
        [[nodiscard]] VkFormat       GetImageFormat() const noexcept;
        [[nodiscard]] bool           CanReadback() const noexcept
        {
            return bCanReadback;
        }
        [[nodiscard]] VkExtent2D                      GetExtent() const noexcept;
        [[nodiscard]] const std::vector<VkImage>&     GetImages() const noexcept;
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const noexcept;

        /** @brief physical device와 surface의 capabilities, formats, present modes를 조회한다. */
        [[nodiscard]] static FSwapchainSupportDetails QuerySupport(VkPhysicalDevice PhysicalDevice,
                                                                   VkSurfaceKHR     Surface);

    private:
        // Surface support and creation helpers
        static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats);
        static VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes);
        static VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& Capabilities, const FWindow& FWindow);
        static VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& Capabilities);

        void Create(const FVulkanContext& Context, const FWindow& FWindow);
        void Destroy();
        void CreateImageViews();

        VkDevice                 Device = VK_NULL_HANDLE;
        VkSwapchainKHR           SwapchainData = VK_NULL_HANDLE;
        VkFormat                 ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D               Extent{};
        bool                     bCanReadback = false;
        std::vector<VkImage>     Images;
        std::vector<VkImageView> ImageViews;
    };
} // namespace MDSS::GPU
