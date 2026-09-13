#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace MDSS
{
    class VulkanContext;

    class RenderContext
    {
    public:
        static constexpr std::size_t MaxFramesInFlight = 2;

        explicit RenderContext(const VulkanContext& Context);
        ~RenderContext();

        RenderContext(const RenderContext&) = delete;
        RenderContext& operator=(const RenderContext&) = delete;
        RenderContext(RenderContext&&) = delete;
        RenderContext& operator=(RenderContext&&) = delete;

        void WaitForCurrentFrame() const;
        void ResetCurrentFence() const;
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
