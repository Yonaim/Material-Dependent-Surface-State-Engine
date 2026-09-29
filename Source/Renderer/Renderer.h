/**
 * @file Renderer.h
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
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

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace MDSS
{
    enum class TRenderViewMode : std::uint32_t
    {
        Lit = 0,
        Unlit = 1,
        Wireframe = 2,
        VertexNormalWS = 3,
        NormalTextureTS = 4,
        MappedNormalWS = 5,
        SurfaceStateHeatmap = 6,
        SurfaceValidity = 7,
        SurfaceID = 8,
        NeighborCount = 9,
        SurfaceSeam = 10,
        OutgoingFluxScale = 11,
        SolverTransferWeight = 12,
        /** @brief 부호가 있는 중간 규모 높이를 색상으로 표시한다. */
        MesoHeight = 13,
        /** @brief 렌더 정점을 대응 텍셀의 Meso 높이만큼 옮긴다. */
        MesoOffset = 14,
        /** @brief 원본 거시 형상을 노멀 맵 음영 없이 표시한다. */
        MacroGeometry = 15
    };

    enum class TSolverTransferWeightView : std::uint32_t
    {
        Combined = 0,
        Distance,
        Normal,
        ProfileBoundary
    };

    class TAssetManager;
    class TDebugUI;
    class TScene;
    class TVulkanContext;
    class TWindow;

    class TRenderer
    {
    public:
        TRenderer(const TVulkanContext& Context, TWindow& TWindow, TAssetManager& Assets, const TScene& Scene);
        ~TRenderer();

        TRenderer(const TRenderer&) = delete;
        TRenderer& operator=(const TRenderer&) = delete;
        TRenderer(TRenderer&&) = delete;
        TRenderer& operator=(TRenderer&&) = delete;

        /** @brief 이미지 획득, 명령 기록·제출, 화면 표시 순서로 한 프레임을 렌더링한다. */
        void RenderFrame(const TScene& SceneData, TDebugUI& DebugInterface, float DeltaTime);
        void SubmitContact(TSurfaceContactInput Contact);
        /** @brief Rebuild the Scene Registry/resources; Scene changes also discard State-ID-based settings. */
        void ReloadSceneResources(const TScene& Scene, bool bResetStateSettings = true);
        [[nodiscard]] std::uint32_t GetSimulationResolution() const noexcept;
        /** @brief Rebuild Surface mapping and GPU resources, resetting State on success. */
        void SetSimulationResolution(TScene& Scene, std::uint32_t Resolution);
        void SetDebugProfileParameters(TSRProfileAssetHandle Profile,
                                       TStateId State,
                                       const TSurfaceStateParameters& Parameters,
                                       bool bKeepRuntimeOverride = true);

        [[nodiscard]] const TSwapchain& GetSwapchain() const noexcept;
        [[nodiscard]] VkRenderPass     GetRenderPassHandle() const noexcept;
        [[nodiscard]] const TSurfaceGPUResourceManager& GetSurfaceGPUResources() const noexcept;
        /** @brief 마지막 완료 프레임에서 측정한 GPU Solver 시간(ms), 미지원 시 -1. */
        [[nodiscard]] float GetLastSolverGpuMilliseconds() const noexcept;
        [[nodiscard]] float GetLastRenderGpuMilliseconds() const noexcept;
        [[nodiscard]] float GetLastSolverPass1GpuMilliseconds() const noexcept;
        [[nodiscard]] float GetLastSolverPass2GpuMilliseconds() const noexcept;

        [[nodiscard]] TRenderViewMode GetRenderViewMode() const noexcept;
        void                         SetRenderViewMode(TRenderViewMode Mode);
        [[nodiscard]] bool IsWorldGridVisible() const noexcept;
        void SetWorldGridVisible(bool bVisible) noexcept;
        [[nodiscard]] bool IsWorldAxisVisible() const noexcept;
        void SetWorldAxisVisible(bool bVisible) noexcept;
        [[nodiscard]] std::uint32_t GetDebugStateChannel() const noexcept;
        void SetDebugStateChannel(std::uint32_t Channel);
        [[nodiscard]] bool                      IsStateHeatmapReliefShadingEnabled() const noexcept;
        void                                    SetStateHeatmapReliefShadingEnabled(bool bEnabled);
        [[nodiscard]] TSolverTransferWeightView GetSolverTransferWeightView() const noexcept;
        void SetSolverTransferWeightView(TSolverTransferWeightView View);
        [[nodiscard]] bool IsDebugGeometryDriveEnabled() const noexcept;
        void SetDebugGeometryDriveEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugNormalWeightEnabled() const noexcept;
        void SetDebugNormalWeightEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugSolverTermEnabled(TSurfaceSolverTerm Term) const noexcept;
        void SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled);
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept;
        /** @brief Preserve State and allocations, but discard timings from the previous mode. */
        void SetRawFluxCacheEnabled(bool bEnabled);

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
        void CreateTimestampQueryPool(std::size_t SolverInstanceCount);
        void UpdateMaterialUniforms();
        void RecreateSwapchain(TDebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                 std::uint32_t   ImageIndex,
                                 const TScene&    SceneData,
                                 const TDebugUI&  DebugInterface,
                                 float            DeltaTime,
                                 bool             bRunSolverStep);

        const TVulkanContext&                Context;
        TWindow&                             TargetWindow;
        TAssetManager&                       Assets;
        TSwapchain                           SwapchainData;
        VkFormat                            DepthFormat = VK_FORMAT_UNDEFINED;
        TGPUImage                            DepthImage;
        TGPUImageView                        DepthImageView;
        TRenderPass                          MainRenderPass;
        VkDescriptorSetLayout               MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        TGraphicsPipeline                    StaticMeshPipeline;
        TGraphicsPipeline                    WireframePipeline;
        TGraphicsPipeline                    GizmoPipeline;
        TGraphicsPipeline                    WorldReferencePipeline;
        std::unique_ptr<TGPUBuffer>           GizmoVertexBuffer;
        std::uint32_t                         GizmoVertexCount = 0;
        std::uint32_t                         WorldGridVertexCount = 0;
        std::uint32_t                         WorldAxisVertexCount = 0;
        bool                                  bWorldGridVisible = true;
        bool                                  bWorldAxisVisible = true;
        std::unique_ptr<TGraphicsPipeline>    SurfaceDebugPipeline;
        TFramebuffer                         MainFramebuffers;
        TRenderContext                       FrameContext;
        VkDescriptorPool                    MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<TMaterialRenderResource> MaterialResources;
        TRenderViewMode                      ViewMode = TRenderViewMode::Lit;
        std::uint32_t                         DebugStateChannel = 0;
        bool                                 bStateHeatmapReliefShadingEnabled = true;
        TSolverTransferWeightView             SolverTransferWeightView = TSolverTransferWeightView::Combined;
        TSurfaceSolverDebugSettings DebugSolverSettings;
        bool                                bFlipNormalY = true;
        float                               NormalStrength = 1.0F;
        float                               AmbientLight = 0.25F;
        std::unique_ptr<TSurfaceStateSystem> SurfaceStates;
        std::map<std::pair<TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> DebugProfileParameterOverrides;
        std::vector<VkSemaphore> RenderFinishedSemaphores;
        VkQueryPool TimestampQueryPool = VK_NULL_HANDLE;
        std::array<bool, TRenderContext::MaxFramesInFlight> bTimestampQueriesSubmitted{};
        std::array<bool, TRenderContext::MaxFramesInFlight> bSolverTimestampQueriesSubmitted{};
        std::uint32_t TimestampQueriesPerFrame = 2;
        std::uint32_t SolverTimestampSlotCount = 0;
        float TimestampPeriodNanoseconds = 0.0F;
        std::uint32_t TimestampValidBits = 0;
        float LastRenderGpuMilliseconds = -1.0F;
        float LastSolverGpuMilliseconds = -1.0F;
        float LastSolverPass1GpuMilliseconds = -1.0F;
        float LastSolverPass2GpuMilliseconds = -1.0F;
    };
} // MDSS 네임스페이스
