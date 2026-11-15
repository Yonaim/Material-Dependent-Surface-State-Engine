/**
 * @file RenderPass.h
 * @brief color 및 depth attachment를 사용하는 render pass.
 */

#pragma once

#include <vulkan/vulkan.h>

namespace MDSS::GPU
{
    class FRenderPass
    {
    public:
        // Render pass lifecycle and access
        /** @brief 지정된 color·depth format에 맞는 render pass를 생성한다. */
        FRenderPass(VkDevice Device, VkFormat ColorFormat, VkFormat DepthFormat);
        ~FRenderPass();

        FRenderPass(const FRenderPass&) = delete;
        FRenderPass& operator=(const FRenderPass&) = delete;
        FRenderPass(FRenderPass&&) = delete;
        FRenderPass& operator=(FRenderPass&&) = delete;

        [[nodiscard]] VkRenderPass GetHandle() const noexcept;

    private:
        VkDevice     Device = VK_NULL_HANDLE;
        VkRenderPass Handle = VK_NULL_HANDLE;
    };
} // namespace MDSS::GPU
