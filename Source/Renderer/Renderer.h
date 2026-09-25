/**
 * @file Renderer.h
 * @brief swapchain 기반 장면 렌더링과 재생성 흐름.
 */

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
    enum class RenderViewMode : std::uint32_t
    {
        Lit = 0,
        Unlit,
        VertexNormalWS,
        NormalTextureTS,
        MappedNormalWS
    };

    class AssetManager;
    class DebugUI;
    class Scene;
    class VulkanContext;
    class Window;

    class Renderer
    {
    public:
        Renderer(const VulkanContext& Context, Window& Window, const AssetManager& Assets);
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        /** @brief 한 프레임을 acquire, record, submit, present 순서로 렌더링한다. */
        void RenderFrame(const Scene& SceneData, DebugUI& DebugInterface);

        [[nodiscard]] const Swapchain& GetSwapchain() const noexcept;
        [[nodiscard]] VkRenderPass     GetRenderPassHandle() const noexcept;

        [[nodiscard]] RenderViewMode GetRenderViewMode() const noexcept;
        void                         SetRenderViewMode(RenderViewMode Mode);

        [[nodiscard]] bool GetFlipNormalY() const noexcept;
        void               SetFlipNormalY(bool bEnabled);

        [[nodiscard]] float GetNormalStrength() const noexcept;
        void                SetNormalStrength(float Strength);

        [[nodiscard]] float GetAmbientLight() const noexcept;
        void                SetAmbientLight(float Intensity);

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
        void UpdateMaterialUniforms();
        void RecreateSwapchain(DebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                 std::uint32_t   ImageIndex,
                                 const Scene&    SceneData,
                                 const DebugUI&  DebugInterface) const;

        const VulkanContext&                Context;
        Window&                             TargetWindow;
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
        RenderViewMode                      ViewMode = RenderViewMode::Lit;
        bool                                bFlipNormalY = true;
        float                               NormalStrength = 1.0F;
        float                               AmbientLight = 0.25F;
    };
} // namespace MDSS
