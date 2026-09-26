/**
 * @file Renderer.h
 * @brief swapchain 기반 장면 렌더링과 재생성 흐름.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Swapchain.h"
#include "SurfaceStateSystem/SurfaceStateSystem.h"
#include "VulkanContext/GPU/GPUBuffer.h"
#include "VulkanContext/GPU/GPUImage.h"
#include "VulkanContext/GPU/GPUImageView.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace MDSS
{
    enum class TRenderViewMode : std::uint32_t
    {
        Lit = 0,
        Unlit,
        VertexNormalWS,
        NormalTextureTS,
        MappedNormalWS,
        SurfaceStateHeatmap,
        SurfaceValidity,
        SurfaceID,
        NeighborCount,
        SurfaceSeam
    };

    class TAssetManager;
    class TDebugUI;
    class TScene;
    class TVulkanContext;
    class TWindow;

    class TRenderer
    {
    public:
        TRenderer(const TVulkanContext& Context, TWindow& TWindow, const TAssetManager& Assets, const TScene& Scene);
        ~TRenderer();

        TRenderer(const TRenderer&) = delete;
        TRenderer& operator=(const TRenderer&) = delete;
        TRenderer(TRenderer&&) = delete;
        TRenderer& operator=(TRenderer&&) = delete;

        /** @brief 한 프레임을 acquire, record, submit, present 순서로 렌더링한다. */
        void RenderFrame(const TScene& SceneData, TDebugUI& DebugInterface, float DeltaTime);
        void SubmitContact(TSurfaceContactInput Contact);

        [[nodiscard]] const TSwapchain& GetSwapchain() const noexcept;
        [[nodiscard]] VkRenderPass     GetRenderPassHandle() const noexcept;
        [[nodiscard]] const TSurfaceGPUResourceManager& GetSurfaceGPUResources() const noexcept;

        [[nodiscard]] TRenderViewMode GetRenderViewMode() const noexcept;
        void                         SetRenderViewMode(TRenderViewMode Mode);
        [[nodiscard]] std::uint32_t GetDebugStateChannel() const noexcept;
        void SetDebugStateChannel(std::uint32_t Channel);

        [[nodiscard]] bool GetFlipNormalY() const noexcept;
        void               SetFlipNormalY(bool bEnabled);

        [[nodiscard]] float GetNormalStrength() const noexcept;
        void                SetNormalStrength(float Strength);

        [[nodiscard]] float GetAmbientLight() const noexcept;
        void                SetAmbientLight(float Intensity);

    private:
        struct TMaterialRenderResource
        {
            std::unique_ptr<TGPUBuffer> UniformBuffer;
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
        void CreateRenderFinishedSemaphores();
        void DestroyRenderFinishedSemaphores() noexcept;
        void UpdateMaterialUniforms();
        void RecreateSwapchain(TDebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                 std::uint32_t   ImageIndex,
                                 const TScene&    SceneData,
                                 const TDebugUI&  DebugInterface,
                                 float            DeltaTime);

        const TVulkanContext&                Context;
        TWindow&                             TargetWindow;
        const TAssetManager&                 Assets;
        TSwapchain                           SwapchainData;
        VkFormat                            DepthFormat = VK_FORMAT_UNDEFINED;
        TGPUImage                            DepthImage;
        TGPUImageView                        DepthImageView;
        TRenderPass                          MainRenderPass;
        VkDescriptorSetLayout               MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        TGraphicsPipeline                    StaticMeshPipeline;
        std::unique_ptr<TGraphicsPipeline>    SurfaceDebugPipeline;
        TFramebuffer                         MainFramebuffers;
        TRenderContext                       FrameContext;
        VkDescriptorPool                    MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<TMaterialRenderResource> MaterialResources;
        TRenderViewMode                      ViewMode = TRenderViewMode::Lit;
        std::uint32_t                         DebugStateChannel = 0;
        bool                                bFlipNormalY = true;
        float                               NormalStrength = 1.0F;
        float                               AmbientLight = 0.25F;
        std::unique_ptr<TSurfaceStateSystem> SurfaceStates;
        std::vector<VkSemaphore> RenderFinishedSemaphores;
    };
} // namespace MDSS
