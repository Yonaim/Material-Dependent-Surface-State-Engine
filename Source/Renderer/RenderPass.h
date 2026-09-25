/**
 * @file RenderPass.h
 * @brief color 및 depth attachment를 사용하는 render pass.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace MDSS
{
    class RenderPass
    {
    public:
        /** @brief 지정된 color·depth format에 맞는 render pass를 생성한다. */
        RenderPass(VkDevice Device, VkFormat ColorFormat, VkFormat DepthFormat);
        ~RenderPass();

        RenderPass(const RenderPass&) = delete;
        RenderPass& operator=(const RenderPass&) = delete;
        RenderPass(RenderPass&&) = delete;
        RenderPass& operator=(RenderPass&&) = delete;

        [[nodiscard]] VkRenderPass GetHandle() const noexcept;

    private:
        VkDevice     Device = VK_NULL_HANDLE;
        VkRenderPass Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS
