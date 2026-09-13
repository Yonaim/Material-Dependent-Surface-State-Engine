#pragma once

#include "Renderer/Framebuffer.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Swapchain.h"
#include "VulkanContext/GPU/GPUBuffer.h"
#include "VulkanContext/GPU/GPUImage.h"
#include "VulkanContext/GPU/GPUImageView.h"

#include <vulkan/vulkan.h>

#include <cstdint>

namespace MDSS
{
    class Scene;
    class VulkanContext;
    class Window;

    class Renderer
    {
    public:
        Renderer(const VulkanContext& Context, const Window& Window);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void RenderFrame(const Scene& SceneData);

        [[nodiscard]] const Swapchain& GetSwapchain() const noexcept;

    private:
        static VkFormat FindDepthFormat(VkPhysicalDevice PhysicalDevice);
        static VkFormat FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
                                            const VkFormat*      Candidates,
                                            std::uint32_t        CandidateCount,
                                            VkImageTiling        Tiling,
                                            VkFormatFeatureFlags Features);

        void RecordCommandBuffer(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex, const Scene& SceneData) const;

        const VulkanContext& Context;
        Swapchain            SwapchainData;
        VkFormat             DepthFormat = VK_FORMAT_UNDEFINED;
        GPUImage             DepthImage;
        GPUImageView         DepthImageView;
        RenderPass           MainRenderPass;
        GraphicsPipeline     StaticMeshPipeline;
        Framebuffer          MainFramebuffers;
        RenderContext        FrameContext;
        GPUBuffer            CubeVertexBuffer;
        GPUBuffer            CubeIndexBuffer;
        std::uint32_t        CubeIndexCount = 0;
    };
} // namespace MDSS
