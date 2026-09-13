#pragma once

#include "AssetManager/Asset.h"
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
#include <memory>
#include <vector>

namespace MDSS
{
    class AssetManager;
    class Scene;
    class VulkanContext;
    class Window;

    class Renderer
    {
    public:
        Renderer(const VulkanContext& Context, const Window& Window, const AssetManager& Assets);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void RenderFrame(const Scene& SceneData);

        [[nodiscard]] const Swapchain& GetSwapchain() const noexcept;

    private:
        struct MaterialRenderResource
        {
            std::unique_ptr<GPUBuffer> UniformBuffer;
            VkDescriptorSet            DescriptorSet = VK_NULL_HANDLE;
        };

        static VkFormat              FindDepthFormat(VkPhysicalDevice PhysicalDevice);
        static VkFormat              FindSupportedFormat(VkPhysicalDevice     PhysicalDevice,
                                                         const VkFormat*      Candidates,
                                                         std::uint32_t        CandidateCount,
                                                         VkImageTiling        Tiling,
                                                         VkFormatFeatureFlags Features);
        static VkDescriptorSetLayout CreateMaterialDescriptorSetLayout(VkDevice Device);

        void CreateMaterialDescriptorResources();
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex, const Scene& SceneData) const;

        const VulkanContext&                Context;
        const AssetManager&                 Assets;
        Swapchain                           SwapchainData;
        VkFormat                            DepthFormat = VK_FORMAT_UNDEFINED;
        GPUImage                            DepthImage;
        GPUImageView                        DepthImageView;
        RenderPass                          MainRenderPass;
        VkDescriptorSetLayout               MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        GraphicsPipeline                    StaticMeshPipeline;
        Framebuffer                         MainFramebuffers;
        RenderContext                       FrameContext;
        VkDescriptorPool                    MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<MaterialRenderResource> MaterialResources;
    };
} // namespace MDSS
