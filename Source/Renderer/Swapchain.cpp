/**
 * @file Swapchain.cpp
 * @brief 표면 지원 정보에 따른 swapchain 선택·생성·재생성.
 */

#include "Renderer/Swapchain.h"

#include "Application/Window.h"
#include "Logger/Logger.h"
#include "VulkanContext/VulkanContext.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace MDSS
{
    Swapchain::Swapchain(const VulkanContext& Context, const Window& Window) : Device(Context.GetDevice())
    {
        Create(Context, Window);
    }

    Swapchain::~Swapchain()
    {
        Destroy();
    }

    void Swapchain::Recreate(const VulkanContext& Context, const Window& Window)
    {
        Destroy();
        Device = Context.GetDevice();
        Create(Context, Window);
    }

    void Swapchain::Create(const VulkanContext& Context, const Window& Window)
    {
        const SwapchainSupportDetails Support = QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());

        if (Support.Formats.empty() || Support.PresentModes.empty())
        {
            throw std::runtime_error("Swapchain support is incomplete for the selected GPU.");
        }

        const VkSurfaceFormatKHR SurfaceFormat = ChooseSurfaceFormat(Support.Formats);
        const VkPresentModeKHR   PresentMode = ChoosePresentMode(Support.PresentModes);
        const VkExtent2D         SelectedExtent = ChooseExtent(Support.Capabilities, Window);

        std::uint32_t ImageCount = Support.Capabilities.minImageCount + 1;
        if (Support.Capabilities.maxImageCount > 0 && ImageCount > Support.Capabilities.maxImageCount)
        {
            ImageCount = Support.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR CreateInfo{};
        CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        CreateInfo.surface = Context.GetSurface();
        CreateInfo.minImageCount = ImageCount;
        CreateInfo.imageFormat = SurfaceFormat.format;
        CreateInfo.imageColorSpace = SurfaceFormat.colorSpace;
        CreateInfo.imageExtent = SelectedExtent;
        CreateInfo.imageArrayLayers = 1;
        CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const auto&                        QueueFamilies = Context.GetQueues().GetFamilyIndices();
        const std::array<std::uint32_t, 2> QueueFamilyIndices = {QueueFamilies.GraphicsFamily.value(),
                                                                 QueueFamilies.PresentFamily.value()};

        if (QueueFamilyIndices[0] != QueueFamilyIndices[1])
        {
            CreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            CreateInfo.queueFamilyIndexCount = static_cast<std::uint32_t>(QueueFamilyIndices.size());
            CreateInfo.pQueueFamilyIndices = QueueFamilyIndices.data();
        }
        else
        {
            CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        CreateInfo.preTransform = Support.Capabilities.currentTransform;
        CreateInfo.compositeAlpha = ChooseCompositeAlpha(Support.Capabilities);
        CreateInfo.presentMode = PresentMode;
        CreateInfo.clipped = VK_TRUE;
        CreateInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(Device, &CreateInfo, nullptr, &SwapchainData) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan swapchain.");
        }

        vkGetSwapchainImagesKHR(Device, SwapchainData, &ImageCount, nullptr);
        Images.resize(ImageCount);
        vkGetSwapchainImagesKHR(Device, SwapchainData, &ImageCount, Images.data());

        ImageFormat = SurfaceFormat.format;
        Extent = SelectedExtent;

        try
        {
            CreateImageViews();
        }
        catch (...)
        {
            vkDestroySwapchainKHR(Device, SwapchainData, nullptr);
            SwapchainData = VK_NULL_HANDLE;
            throw;
        }

        Logger::Info("Renderer",
                     "Swapchain created: " + std::to_string(Extent.width) + "x" + std::to_string(Extent.height) + ", " +
                         std::to_string(Images.size()) + " images.");
        Logger::Debug("Renderer",
                      "Swapchain format=" + std::to_string(static_cast<int>(ImageFormat)) +
                          ", present mode=" + std::to_string(static_cast<int>(PresentMode)) + ".");
    }

    void Swapchain::Destroy()
    {
        for (VkImageView ImageView : ImageViews)
        {
            if (ImageView != VK_NULL_HANDLE)
            {
                vkDestroyImageView(Device, ImageView, nullptr);
            }
        }
        ImageViews.clear();
        Images.clear();

        if (SwapchainData != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(Device, SwapchainData, nullptr);
            SwapchainData = VK_NULL_HANDLE;
        }

        ImageFormat = VK_FORMAT_UNDEFINED;
        Extent = {};
    }

    VkSwapchainKHR Swapchain::GetHandle() const noexcept
    {
        return SwapchainData;
    }

    VkFormat Swapchain::GetImageFormat() const noexcept
    {
        return ImageFormat;
    }

    VkExtent2D Swapchain::GetExtent() const noexcept
    {
        return Extent;
    }

    const std::vector<VkImage>& Swapchain::GetImages() const noexcept
    {
        return Images;
    }

    const std::vector<VkImageView>& Swapchain::GetImageViews() const noexcept
    {
        return ImageViews;
    }

    SwapchainSupportDetails Swapchain::QuerySupport(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface)
    {
        SwapchainSupportDetails Details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Details.Capabilities);

        std::uint32_t FormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, nullptr);
        if (FormatCount > 0)
        {
            Details.Formats.resize(FormatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &FormatCount, Details.Formats.data());
        }

        std::uint32_t PresentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &PresentModeCount, nullptr);
        if (PresentModeCount > 0)
        {
            Details.PresentModes.resize(PresentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                PhysicalDevice, Surface, &PresentModeCount, Details.PresentModes.data());
        }

        return Details;
    }

    VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats)
    {
        const auto Preferred = std::find_if(Formats.begin(),
                                            Formats.end(),
                                            [](const VkSurfaceFormatKHR& Format)
                                            {
                                                return Format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                                                       Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                                            });

        return Preferred != Formats.end() ? *Preferred : Formats.front();
    }

    VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes)
    {
        const auto Mailbox = std::find(PresentModes.begin(), PresentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR);

        return Mailbox != PresentModes.end() ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& Capabilities, const Window& Window)
    {
        if (Capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
        {
            return Capabilities.currentExtent;
        }

        std::int32_t Width = 0;
        std::int32_t Height = 0;
        glfwGetFramebufferSize(Window.GetNativeHandle(), &Width, &Height);

        VkExtent2D Extent = {static_cast<std::uint32_t>(std::max(Width, 1)),
                             static_cast<std::uint32_t>(std::max(Height, 1))};

        Extent.width = std::clamp(Extent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        Extent.height =
            std::clamp(Extent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);

        return Extent;
    }

    VkCompositeAlphaFlagBitsKHR Swapchain::ChooseCompositeAlpha(const VkSurfaceCapabilitiesKHR& Capabilities)
    {
        constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> Candidates = {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                                           VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                                           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                                           VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};

        for (const VkCompositeAlphaFlagBitsKHR Candidate : Candidates)
        {
            if ((Capabilities.supportedCompositeAlpha & Candidate) != 0)
            {
                return Candidate;
            }
        }

        throw std::runtime_error("No supported Vulkan swapchain composite alpha mode was found.");
    }

    void Swapchain::CreateImageViews()
    {
        ImageViews.resize(Images.size());

        for (std::size_t Index = 0; Index < Images.size(); ++Index)
        {
            VkImageViewCreateInfo CreateInfo{};
            CreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            CreateInfo.image = Images[Index];
            CreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            CreateInfo.format = ImageFormat;
            CreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            CreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            CreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            CreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            CreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            CreateInfo.subresourceRange.baseMipLevel = 0;
            CreateInfo.subresourceRange.levelCount = 1;
            CreateInfo.subresourceRange.baseArrayLayer = 0;
            CreateInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(Device, &CreateInfo, nullptr, &ImageViews[Index]) != VK_SUCCESS)
            {
                for (std::size_t CreatedIndex = 0; CreatedIndex < Index; ++CreatedIndex)
                {
                    vkDestroyImageView(Device, ImageViews[CreatedIndex], nullptr);
                }
                ImageViews.clear();
                throw std::runtime_error("Failed to create swapchain image view.");
            }
        }
    }
} // namespace MDSS
