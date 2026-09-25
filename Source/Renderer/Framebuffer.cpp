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
    Framebuffer::Framebuffer(VkDevice                        Device,
                             VkRenderPass                    RenderPass,
                             VkExtent2D                      Extent,
                             const std::vector<VkImageView>& ColorImageViews,
                             VkImageView                     DepthImageView)
        : Device(Device)
    {
        Create(RenderPass, Extent, ColorImageViews, DepthImageView);
    }

    Framebuffer::~Framebuffer()
    {
        Reset();
    }

    void Framebuffer::Recreate(VkRenderPass                    RenderPass,
                               VkExtent2D                      Extent,
                               const std::vector<VkImageView>& ColorImageViews,
                               VkImageView                     DepthImageView)
    {
        Reset();
        Create(RenderPass, Extent, ColorImageViews, DepthImageView);
    }

    void Framebuffer::Reset()
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

    void Framebuffer::Create(VkRenderPass                    RenderPass,
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
            CreateInfo.renderPass = RenderPass;
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

        Logger::Debug("Renderer",
                      "Framebuffers created: " + std::to_string(Handles.size()) + " at " +
                          std::to_string(Extent.width) + "x" + std::to_string(Extent.height) + ".");
    }

    VkFramebuffer Framebuffer::Get(std::size_t Index) const
    {
        if (Index >= Handles.size())
        {
            throw std::out_of_range("Framebuffer index is out of range.");
        }

        return Handles[Index];
    }

    std::size_t Framebuffer::GetCount() const noexcept
    {
        return Handles.size();
    }
} // namespace MDSS
