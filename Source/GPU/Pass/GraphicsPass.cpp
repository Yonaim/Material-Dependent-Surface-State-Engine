/**
 * @file GraphicsPass.cpp
 * @brief Vulkan render pass 실행 구간의 공통 기록 helper 구현.
 */

#include "GPU/Pass/GraphicsPass.h"

#include <limits>
#include <stdexcept>

namespace MDSS::GPU
{
#pragma region TGraphicsPass_Implementation

    TGraphicsPass::TGraphicsPass(const TRenderPass& RenderPass) noexcept : RenderPass(&RenderPass)
    {
    }

    void TGraphicsPass::Begin(VkCommandBuffer               CommandBuffer,
                              const TFramebuffer&           Framebuffers,
                              std::size_t                   FramebufferIndex,
                              VkExtent2D                    RenderExtent,
                              std::span<const VkClearValue> ClearValues,
                              VkSubpassContents             Contents) const
    {
        if (CommandBuffer == VK_NULL_HANDLE || RenderPass == nullptr || RenderExtent.width == 0 ||
            RenderExtent.height == 0 || ClearValues.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::invalid_argument("Graphics pass begin received invalid command or render-area parameters.");
        }

        VkRenderPassBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        BeginInfo.renderPass = RenderPass->GetHandle();
        BeginInfo.framebuffer = Framebuffers.Get(FramebufferIndex);
        if (BeginInfo.renderPass == VK_NULL_HANDLE || BeginInfo.framebuffer == VK_NULL_HANDLE)
        {
            throw std::logic_error("Graphics pass begin received an uninitialized Vulkan render target.");
        }
        BeginInfo.renderArea.offset = {0, 0};
        BeginInfo.renderArea.extent = RenderExtent;
        BeginInfo.clearValueCount = static_cast<std::uint32_t>(ClearValues.size());
        BeginInfo.pClearValues = ClearValues.empty() ? nullptr : ClearValues.data();
        vkCmdBeginRenderPass(CommandBuffer, &BeginInfo, Contents);
    }

    void TGraphicsPass::End(VkCommandBuffer CommandBuffer) const
    {
        if (CommandBuffer == VK_NULL_HANDLE || RenderPass == nullptr)
        {
            throw std::invalid_argument("Graphics pass end received an invalid command buffer.");
        }

        vkCmdEndRenderPass(CommandBuffer);
    }
#pragma endregion
} // namespace MDSS::GPU
