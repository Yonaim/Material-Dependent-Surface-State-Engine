/**
 * @file RenderPass.h
 * @brief color 및 depth attachment를 사용하는 render pass.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class TRenderPass
    {
    public:
        /** @brief 지정된 color·depth format에 맞는 render pass를 생성한다. */
        TRenderPass(VkDevice Device, VkFormat ColorFormat, VkFormat DepthFormat);
        ~TRenderPass();

        TRenderPass(const TRenderPass&) = delete;
        TRenderPass& operator=(const TRenderPass&) = delete;
        TRenderPass(TRenderPass&&) = delete;
        TRenderPass& operator=(TRenderPass&&) = delete;

        [[nodiscard]] VkRenderPass GetHandle() const noexcept;

    private:
        VkDevice     Device = VK_NULL_HANDLE;
        VkRenderPass Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
