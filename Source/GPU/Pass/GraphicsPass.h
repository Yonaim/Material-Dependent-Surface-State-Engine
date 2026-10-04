/**
 * @file GraphicsPass.h
 * @brief Vulkan render pass 실행 구간의 공통 기록 helper.
 */

#pragma once

#include "GPU/Vulkan/Render/Framebuffer.h"
#include "GPU/Vulkan/Render/RenderPass.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <span>

namespace MDSS::GPU
{
    class TGraphicsPass final
    {
    public:
        explicit TGraphicsPass(const TRenderPass& RenderPass) noexcept;

        /** @brief 지정한 framebuffer로 render pass 기록을 시작한다. */
        void Begin(VkCommandBuffer             CommandBuffer,
                   const TFramebuffer&         Framebuffers,
                   std::size_t                 FramebufferIndex,
                   VkExtent2D                  RenderExtent,
                   std::span<const VkClearValue> ClearValues,
                   VkSubpassContents           Contents = VK_SUBPASS_CONTENTS_INLINE) const;
        /** @brief 현재 render pass 기록을 종료한다. */
        void End(VkCommandBuffer CommandBuffer) const;

    private:
        const TRenderPass* RenderPass = nullptr;
    };
} // namespace MDSS::GPU
