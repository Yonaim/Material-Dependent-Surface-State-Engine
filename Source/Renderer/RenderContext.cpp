/**
 * @file RenderContext.cpp
 * @brief frame-in-flight별 command buffer와 동기화 자원.
 */

#include "Renderer/RenderContext.h"

#include "Logger/Logger.h"
#include "VulkanContext/VulkanContext.h"

#include <limits>
#include <stdexcept>

namespace MDSS
{
    TRenderContext::TRenderContext(const TVulkanContext& Context)
        : Device(Context.GetDevice()), CommandPool(Context.GetCommands().GetPool()),
          CommandBuffers(Context.GetCommands().AllocatePrimary(static_cast<std::uint32_t>(MaxFramesInFlight)))
    {
        VkSemaphoreCreateInfo SemaphoreInfo{};
        SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo FenceInfo{};
        FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (std::size_t Index = 0; Index < MaxFramesInFlight; ++Index)
        {
            if (vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &ImageAvailableSemaphores[Index]) != VK_SUCCESS ||
                vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &RenderFinishedSemaphores[Index]) != VK_SUCCESS ||
                vkCreateFence(Device, &FenceInfo, nullptr, &InFlightFences[Index]) != VK_SUCCESS)
            {
                for (std::size_t Created = 0; Created <= Index; ++Created)
                {
                    if (ImageAvailableSemaphores[Created] != VK_NULL_HANDLE)
                    {
                        vkDestroySemaphore(Device, ImageAvailableSemaphores[Created], nullptr);
                    }
                    if (RenderFinishedSemaphores[Created] != VK_NULL_HANDLE)
                    {
                        vkDestroySemaphore(Device, RenderFinishedSemaphores[Created], nullptr);
                    }
                    if (InFlightFences[Created] != VK_NULL_HANDLE)
                    {
                        vkDestroyFence(Device, InFlightFences[Created], nullptr);
                    }
                }

                vkFreeCommandBuffers(
                    Device, CommandPool, static_cast<std::uint32_t>(CommandBuffers.size()), CommandBuffers.data());
                CommandBuffers.clear();
                throw std::runtime_error("Failed to create Vulkan frame synchronization objects.");
            }
        }

        TLogger::Info("TRenderer",
                     "Frame synchronization initialized with " + std::to_string(MaxFramesInFlight) +
                         " frames in flight.");
    }

    TRenderContext::~TRenderContext()
    {
        for (std::size_t Index = 0; Index < MaxFramesInFlight; ++Index)
        {
            if (ImageAvailableSemaphores[Index] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(Device, ImageAvailableSemaphores[Index], nullptr);
            }
            if (RenderFinishedSemaphores[Index] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(Device, RenderFinishedSemaphores[Index], nullptr);
            }
            if (InFlightFences[Index] != VK_NULL_HANDLE)
            {
                vkDestroyFence(Device, InFlightFences[Index], nullptr);
            }
        }

        if (!CommandBuffers.empty())
        {
            vkFreeCommandBuffers(
                Device, CommandPool, static_cast<std::uint32_t>(CommandBuffers.size()), CommandBuffers.data());
            CommandBuffers.clear();
        }
    }

    void TRenderContext::WaitForCurrentFrame() const
    {
        if (vkWaitForFences(
                Device, 1, &InFlightFences[CurrentFrame], VK_TRUE, std::numeric_limits<std::uint64_t>::max()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed while waiting for the current Vulkan frame fence.");
        }
    }

    void TRenderContext::ResetCurrentFence() const
    {
        if (vkResetFences(Device, 1, &InFlightFences[CurrentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset the current Vulkan frame fence.");
        }
    }

    void TRenderContext::AdvanceFrame() noexcept
    {
        CurrentFrame = (CurrentFrame + 1) % static_cast<std::uint32_t>(MaxFramesInFlight);
    }

    std::uint32_t TRenderContext::GetCurrentFrameIndex() const noexcept
    {
        return CurrentFrame;
    }

    VkCommandBuffer TRenderContext::GetCurrentCommandBuffer() const noexcept
    {
        return CommandBuffers[CurrentFrame];
    }

    VkSemaphore TRenderContext::GetImageAvailableSemaphore() const noexcept
    {
        return ImageAvailableSemaphores[CurrentFrame];
    }

    VkSemaphore TRenderContext::GetRenderFinishedSemaphore() const noexcept
    {
        return RenderFinishedSemaphores[CurrentFrame];
    }

    VkFence TRenderContext::GetInFlightFence() const noexcept
    {
        return InFlightFences[CurrentFrame];
    }
} // namespace MDSS
