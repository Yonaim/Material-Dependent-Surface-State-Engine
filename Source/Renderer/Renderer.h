/**
 * @file Renderer.h
 * @brief 스왑체인 기반 장면 렌더링과 재생성 흐름.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/DemoSurfaceEffects.h"
#include "Renderer/GraphicsPipeline.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPass.h"
#include "Renderer/Swapchain.h"
#include "SurfaceStateSystem/Debug/TexelInspector.h"
#include "SurfaceStateSystem/Debug/TexelGeometryPreview.h"
#include "SurfaceStateSystem/State/SimulationClock.h"
#include "SurfaceStateSystem/SurfaceStateSystem.h"
#include "VulkanContext/GPU/GPUBuffer.h"
#include "VulkanContext/GPU/GPUImage.h"
#include "VulkanContext/GPU/GPUImageView.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
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
        /** @brief texel 연결면을 복원한 Meso 높이로 표시한다. */
        MesoOffset = 14,
        /** @brief 원본 거시 형상을 노멀 맵 음영 없이 표시한다. */
        MacroGeometry = 15,
        /** @brief 확대 시 개별 텍셀과 고정 크기 묶음의 UV 격자를 표시한다. */
        SurfaceTexelGrid = 16,
        /** @brief 원본 삼각형의 월드 면적 / UV 면적 / 텍셀 수를 표시한다. */
        SurfaceTexelArea = 17,
        SurfaceAccumulation = 18,
        SurfaceFinalGeometry = 19
    };

    enum class TSolverTransferWeightView : std::uint32_t
    {
        Combined = 0,
        Distance,
        Normal,
        ProfileBoundary
    };

    struct TSurfaceDebugDisplaySettings
    {
        bool          bRawState = false;
        std::uint32_t AccumulationComponent = 0;
        float         RawStateMax = 4.0F;
        float         HeightMax = 0.01F;
        float DisplacementScale = 1.0F;
        // 0: shaded surface, 1: grid overlay, 2: grid on a dark surface.
        std::uint32_t HeightGridMode = 0;
        std::uint32_t HeightGridBlockSize = 8;
    };

    class TAssetManager;
    class TDebugUI;
    class TScene;
    class TVulkanContext;
    class TWindow;

    class TRenderer
    {
    public:
        TRenderer(const TVulkanContext& Context,
                  TWindow& TWindow,
                  TAssetManager& Assets,
                  const TScene& Scene,
                  TSurfaceStateSystem& SurfaceStates);
        ~TRenderer();

        TRenderer(const TRenderer&) = delete;
        TRenderer& operator=(const TRenderer&) = delete;
        TRenderer(TRenderer&&) = delete;
        TRenderer& operator=(TRenderer&&) = delete;

        /** @brief 이미지 획득, 명령 기록·제출, 화면 표시 순서로 한 프레임을 렌더링한다. */
        void RenderFrame(const TScene& SceneData, TDebugUI& DebugInterface, float DeltaTime);
        [[nodiscard]] double GetPendingSimulationSeconds() const noexcept { return SimulationClock.GetPendingSeconds(); }
        [[nodiscard]] double GetSimulatedSeconds() const noexcept { return SimulationClock.GetSimulatedSeconds(); }
        [[nodiscard]] std::uint32_t GetLastSimulationStepCount() const noexcept { return LastSimulationStepCount; }
        [[nodiscard]] float GetMaximumSimulationStep() const noexcept { return MaximumSimulationStep; }
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
        [[nodiscard]] const TSurfaceDebugDisplaySettings& GetSurfaceDebugDisplaySettings() const noexcept
        {
            return SurfaceDebugSettings;
        }
        void SetSurfaceDebugDisplaySettings(const TSurfaceDebugDisplaySettings& Settings);
        [[nodiscard]] const TDemoSurfaceEffectSettings& GetDemoSurfaceEffectSettings() const noexcept { return DemoEffects; }
        void SetDemoSurfaceEffectSettings(const TDemoSurfaceEffectSettings& Settings);
        void SetSceneLitHeightDisplayScale(TScene& Scene, float Scale);
        [[nodiscard]] TDemoSurfaceStateBindings GetDemoSurfaceStateBindings() const;

        [[nodiscard]] bool
             InspectTexel(const TScene& Scene, std::size_t Instance, std::uint32_t Triangle, glm::vec2 UV);
        void ClearInspectedTexel() noexcept;
        [[nodiscard]] const std::optional<TSurfaceTexelSelection>& GetInspectedTexel() const noexcept
        {
            return InspectedTexel;
        }
        [[nodiscard]] const std::optional<TSurfaceTexelSnapshot>& GetTexelSnapshot() const noexcept;
        [[nodiscard]] TSolverTransferWeightView GetSolverTransferWeightView() const noexcept;
        void SetSolverTransferWeightView(TSolverTransferWeightView View);
        [[nodiscard]] std::uint32_t             GetTexelGridBlockSize() const noexcept;
        void                                    SetTexelGridBlockSize(std::uint32_t Size);
        [[nodiscard]] float                     GetTexelAreaReference() const noexcept;
        /** @brief 해상도·Scene 전환과 독립적인 기준 면적(world units²/texel)을 설정한다. */
        void               SetTexelAreaReference(float Area);
        [[nodiscard]] bool IsDebugGeometryDriveEnabled() const noexcept;
        void SetDebugGeometryDriveEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugNormalWeightEnabled() const noexcept;
        void SetDebugNormalWeightEnabled(bool bEnabled);
        [[nodiscard]] bool IsDebugSolverTermEnabled(TSurfaceSolverTerm Term) const noexcept;
        void SetDebugSolverTermEnabled(TSurfaceSolverTerm Term, bool bEnabled);
        [[nodiscard]] bool IsRawFluxCacheEnabled() const noexcept;
        /** @brief Preserve State and allocations, but discard timings from the previous mode. */
        void SetRawFluxCacheEnabled(bool bEnabled);
        [[nodiscard]] bool IsAccumulationFeedbackEnabled() const noexcept;
        void SetAccumulationFeedbackEnabled(bool bEnabled);

        [[nodiscard]] bool GetFlipNormalY() const noexcept;
        void               SetFlipNormalY(bool bEnabled);

        [[nodiscard]] float GetNormalStrength() const noexcept;
        void                SetNormalStrength(float Strength);

        [[nodiscard]] float GetAmbientLight() const noexcept;
        void                SetAmbientLight(float Intensity);

    private:
        struct TMaterialRenderResource
        {
            std::array<std::unique_ptr<TGPUBuffer>, TRenderContext::MaxFramesInFlight> UniformBuffers;
            std::array<VkDescriptorSet, TRenderContext::MaxFramesInFlight> DescriptorSets{};
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
        void UploadMaterialUniforms(std::uint32_t Frame, const glm::vec3& CameraPosition, float LitHeightDisplayScale);
        [[nodiscard]] float GetDebugViewParameter() const noexcept;
        void RecreateSwapchain(TDebugUI& DebugInterface);
        void RecordCommandBuffer(VkCommandBuffer CommandBuffer,
                                 std::uint32_t   ImageIndex,
                                 const TScene&    SceneData,
                                 const TDebugUI&  DebugInterface,
                                 std::span<const float> SimulationSteps);

        const TVulkanContext&                Context;
        TWindow&                             TargetWindow;
        TAssetManager&                       Assets;
        /** @brief Application이 소유하며 Renderer는 compute 기록과 렌더링 동안만 참조한다. */
        TSurfaceStateSystem&                 SurfaceStates;
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
        std::uint32_t                         WorldGridVertexCount = 0;
        std::uint32_t                         WorldAxisVertexCount = 0;
        std::uint32_t                         TranslateGizmoVertexCount = 0;
        std::uint32_t                         RotateGizmoVertexCount = 0;
        bool                                  bWorldGridVisible = true;
        bool                                  bWorldAxisVisible = true;
        std::unique_ptr<TGraphicsPipeline>    SurfaceDebugPipeline;
        std::unique_ptr<TTexelGeometryPreview> TexelGeometryPreview;
        std::unique_ptr<TGraphicsPipeline>     TexelGeometryPipeline;
        std::unique_ptr<TGraphicsPipeline>     SurfaceLitPipeline;
        std::unique_ptr<TGraphicsPipeline>     TexelSurfaceLitPipeline;
        TFramebuffer                         MainFramebuffers;
        TRenderContext                       FrameContext;
        VkDescriptorPool                    MaterialDescriptorPool = VK_NULL_HANDLE;
        std::vector<TMaterialRenderResource> MaterialResources;
        TRenderViewMode                      ViewMode = TRenderViewMode::Lit;
        std::uint32_t                         DebugStateChannel = 0;
        bool                                 bStateHeatmapReliefShadingEnabled = true;
        TSurfaceDebugDisplaySettings          SurfaceDebugSettings;
        TDemoSurfaceEffectSettings             DemoEffects;
        std::optional<TSurfaceTexelSelection> InspectedTexel;
        std::unique_ptr<TTexelInspector>      TexelInspector;
        std::uint64_t                         SimulationStepSerial = 0;
        TSolverTransferWeightView             SolverTransferWeightView = TSolverTransferWeightView::Combined;
        std::uint32_t                         TexelGridBlockSize = 8;
        float                                 TexelAreaReference = 1.0e-4F;
        TSurfaceSolverDebugSettings DebugSolverSettings;
        bool                                bFlipNormalY = true;
        float                               NormalStrength = 1.0F;
        float                               AmbientLight = 0.25F;
        std::map<std::pair<TSRProfileAssetHandle, TStateId>, TSurfaceStateParameters> DebugProfileParameterOverrides;
        std::vector<VkSemaphore> RenderFinishedSemaphores;
        VkQueryPool TimestampQueryPool = VK_NULL_HANDLE;
        std::array<bool, TRenderContext::MaxFramesInFlight> bTimestampQueriesSubmitted{};
        std::array<std::uint32_t, TRenderContext::MaxFramesInFlight> SolverTimestampStepsSubmitted{};
        TSimulationClock SimulationClock;
        std::uint32_t LastSimulationStepCount = 0;
        float MaximumSimulationStep = FixedSimulationStepSeconds;
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
