/**
 * @file Framebuffer.cpp
 * @brief swapchain image와 depth image를 연결하는 framebuffer 자원.
 */

#include "Renderer/Framebuffer.h"

#include "Logger/Logger.h"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace MDSS
{
    TFramebuffer::TFramebuffer(VkDevice                        Device,
                             VkRenderPass                    TRenderPass,
                             VkExtent2D                      Extent,
                             const std::vector<VkImageView>& ColorImageViews,
                             VkImageView                     DepthImageView)
        : Device(Device)
    {
        Create(TRenderPass, Extent, ColorImageViews, DepthImageView);
    }

    TFramebuffer::~TFramebuffer()
    {
        Reset();
    }

    void TFramebuffer::Recreate(VkRenderPass                    TRenderPass,
                               VkExtent2D                      Extent,
                               const std::vector<VkImageView>& ColorImageViews,
                               VkImageView                     DepthImageView)
    {
        Reset();
        Create(TRenderPass, Extent, ColorImageViews, DepthImageView);
    }

    void TFramebuffer::Reset()
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

    void TFramebuffer::Create(VkRenderPass                    TRenderPass,
                             VkExtent2D                      Extent,
                             const std::vector<VkImageView>& ColorImageViews,
                             VkImageView                     DepthImageView)
    {
        Handles.resize(ColorImageViews.size(), VK_NULL_HANDLE);

        for (std::size_t Index = 0; Index < ColorImageViews.size(); ++Index)
        {
            const std::array<VkImageView, 2> Attachments = {ColorImageViews[Index], DepthImageView};

            VkFramebufferCreateInfo CreateInfo{};
            CreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            CreateInfo.renderPass = TRenderPass;
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

        TLogger::Debug("TRenderer",
                      "Framebuffers created: " + std::to_string(Handles.size()) + " at " +
                          std::to_string(Extent.width) + "x" + std::to_string(Extent.height) + ".");
    }

    VkFramebuffer TFramebuffer::Get(std::size_t Index) const
    {
        if (Index >= Handles.size())
        {
            throw std::out_of_range("TFramebuffer index is out of range.");
        }

        return Handles[Index];
    }

    std::size_t TFramebuffer::GetCount() const noexcept
    {
        return Handles.size();
    }
} // namespace MDSS
