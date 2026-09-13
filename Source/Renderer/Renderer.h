#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Swapchain.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class VulkanContext;
    class Window;

    class Renderer
    {
    public:
        Renderer(const VulkanContext& Context, const Window& Window);

        void RenderFrame();

        [[nodiscard]] const Swapchain& GetSwapchain() const noexcept;

    private:
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex) const;

        const VulkanContext& Context;
        Swapchain            SwapchainData;
        RenderPass           MainRenderPass;
        GraphicsPipeline     TrianglePipeline;
        Framebuffer          MainFramebuffers;
        RenderContext        FrameContext;
    };
} // namespace MDSS
