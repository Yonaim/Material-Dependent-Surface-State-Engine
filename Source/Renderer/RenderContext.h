/**
 * @file RenderContext.h
 * @brief frame-in-flight별 command buffer와 동기화 자원.
 */

#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace MDSS
{
    class TVulkanContext;

    class TRenderContext
    {
    public:
        static constexpr std::size_t MaxFramesInFlight = 2;

        explicit TRenderContext(const TVulkanContext& Context);
        ~TRenderContext();

        TRenderContext(const TRenderContext&) = delete;
        TRenderContext& operator=(const TRenderContext&) = delete;
        TRenderContext(TRenderContext&&) = delete;
        TRenderContext& operator=(TRenderContext&&) = delete;

        /** @brief 현재 frame slot의 fence가 신호될 때까지 CPU를 대기시킨다. */
        void WaitForCurrentFrame() const;
        /** @brief 현재 frame slot fence를 다음 queue submit을 위해 reset한다. */
        void ResetCurrentFence() const;
        /** @brief 다음 frame-in-flight slot으로 인덱스를 순환 이동한다. */
        void AdvanceFrame() noexcept;

        [[nodiscard]] std::uint32_t   GetCurrentFrameIndex() const noexcept;
        [[nodiscard]] VkCommandBuffer GetCurrentCommandBuffer() const noexcept;
        [[nodiscard]] VkSemaphore     GetImageAvailableSemaphore() const noexcept;
        [[nodiscard]] VkSemaphore     GetRenderFinishedSemaphore() const noexcept;
        [[nodiscard]] VkFence         GetInFlightFence() const noexcept;

    private:
        VkDevice      Device = VK_NULL_HANDLE;
        VkCommandPool CommandPool = VK_NULL_HANDLE;

        std::vector<VkCommandBuffer>               CommandBuffers;
        std::array<VkSemaphore, MaxFramesInFlight> ImageAvailableSemaphores{};
        std::array<VkSemaphore, MaxFramesInFlight> RenderFinishedSemaphores{};
        std::array<VkFence, MaxFramesInFlight>     InFlightFences{};
        std::uint32_t                              CurrentFrame = 0;
    };
} // namespace MDSS
