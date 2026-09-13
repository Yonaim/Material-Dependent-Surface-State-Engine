#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace MDSS
{
    class VulkanContext;
    class Window;

    struct SwapchainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        Capabilities{};
        std::vector<VkSurfaceFormatKHR> Formats;
        std::vector<VkPresentModeKHR>   PresentModes;
    };

    class Swapchain
    {
    public:
        Swapchain(const VulkanContext& Context, const Window& Window);
        ~Swapchain();

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;
        Swapchain(Swapchain&&) = delete;
        Swapchain& operator=(Swapchain&&) = delete;

        [[nodiscard]] VkSwapchainKHR                  GetHandle() const noexcept;
        [[nodiscard]] VkFormat                        GetImageFormat() const noexcept;
        [[nodiscard]] VkExtent2D                      GetExtent() const noexcept;
        [[nodiscard]] const std::vector<VkImage>&     GetImages() const noexcept;
        [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const noexcept;

        [[nodiscard]] static SwapchainSupportDetails QuerySupport(VkPhysicalDevice PhysicalDevice,
                                                                  VkSurfaceKHR     Surface);

    private:
        static VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats);
        static VkPresentModeKHR   ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes);
        static VkExtent2D         ChooseExtent(const VkSurfaceCapabilitiesKHR& Capabilities, const Window& Window);
        static VkCompositeAlphaFlagBitsKHR ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& Capabilities);

        void CreateImageViews();

        VkDevice                 Device = VK_NULL_HANDLE;
        VkSwapchainKHR           SwapchainData = VK_NULL_HANDLE;
        VkFormat                 ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D               Extent{};
        std::vector<VkImage>     Images;
        std::vector<VkImageView> ImageViews;
    };
} // namespace MDSS
