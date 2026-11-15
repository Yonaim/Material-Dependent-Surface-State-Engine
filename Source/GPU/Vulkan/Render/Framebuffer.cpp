/**
 * @file Framebuffer.cpp
 * @brief swapchain image와 depth image를 연결하는 framebuffer 자원.
 */

#include "GPU/Vulkan/Render/Framebuffer.h"

#include "Logger/Logger.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace MDSS::GPU
{
// ============================================================
// 00. Framebuffer Lifecycle
// ============================================================
#pragma region 00_Framebuffer_Lifecycle
// attachment 수명에 맞춰 framebuffer의 생성·해제를 관리한다.

    FFramebuffer::FFramebuffer(VkDevice                        Device,
                               VkRenderPass                    FRenderPass,
                               VkExtent2D                      Extent,
                               const std::vector<VkImageView>& ColorImageViews,
                               const std::vector<VkImageView>& DepthImageViews)
        : Device(Device)
    {
        Create(FRenderPass, Extent, ColorImageViews, DepthImageViews);
    }

    FFramebuffer::~FFramebuffer()
    {
        Reset();
    }

#pragma endregion

// ============================================================
// 01. Framebuffer Creation
// ============================================================
#pragma region 01_Framebuffer_Creation
// 현재 image view와 extent를 render pass의 attachment로 연결한다.

    void FFramebuffer::Recreate(VkRenderPass                    FRenderPass,
                                VkExtent2D                      Extent,
                                const std::vector<VkImageView>& ColorImageViews,
                                const std::vector<VkImageView>& DepthImageViews)
    {
        Reset();
        Create(FRenderPass, Extent, ColorImageViews, DepthImageViews);
    }

    void FFramebuffer::Reset()
    {
        for (VkFramebuffer Handle : Handles)
        {
            if (Handle != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(Device, Handle, nullptr);
            }
        }

        Handles.clear();
    }

    void FFramebuffer::Create(VkRenderPass                    FRenderPass,
                              VkExtent2D                      Extent,
                              const std::vector<VkImageView>& ColorImageViews,
                              const std::vector<VkImageView>& DepthImageViews)
    {
        if (ColorImageViews.size() != DepthImageViews.size() || ColorImageViews.empty())
            throw std::invalid_argument("Framebuffer color and depth image view counts must match.");
        Handles.resize(ColorImageViews.size(), VK_NULL_HANDLE);

        for (std::size_t Index = 0; Index < ColorImageViews.size(); ++Index)
        {
            const std::array<VkImageView, 2> Attachments = {ColorImageViews[Index], DepthImageViews[Index]};

            VkFramebufferCreateInfo CreateInfo{};
            CreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            CreateInfo.renderPass = FRenderPass;
            CreateInfo.attachmentCount = static_cast<std::uint32_t>(Attachments.size());
            CreateInfo.pAttachments = Attachments.data();
            CreateInfo.width = Extent.width;
            CreateInfo.height = Extent.height;
            CreateInfo.layers = 1;

            if (vkCreateFramebuffer(Device, &CreateInfo, nullptr, &Handles[Index]) != VK_SUCCESS)
            {
                for (std::size_t Created = 0; Created < Index; ++Created)
                {
                    vkDestroyFramebuffer(Device, Handles[Created], nullptr);
                }

                Handles.clear();
                throw std::runtime_error("Failed to create Vulkan framebuffer.");
            }
        }

        FLogger::Debug("TRenderer",
                       "Framebuffers created: " + std::to_string(Handles.size()) + " at " +
                           std::to_string(Extent.width) + "x" + std::to_string(Extent.height) + ".");
    }

#pragma endregion

// ============================================================
// 02. Framebuffer Accessors
// ============================================================
#pragma region 02_Framebuffer_Accessors
// frame index에 대응하는 framebuffer 식별자를 제공한다.

    VkFramebuffer FFramebuffer::Get(std::size_t Index) const
    {
        if (Index >= Handles.size())
        {
            throw std::out_of_range("TFramebuffer index is out of range.");
        }

        return Handles[Index];
    }

    std::size_t FFramebuffer::GetCount() const noexcept
    {
        return Handles.size();
    }
#pragma endregion
} // namespace MDSS::GPU
