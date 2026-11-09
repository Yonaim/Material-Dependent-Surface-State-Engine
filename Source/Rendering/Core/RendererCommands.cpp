#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
    struct TRenderer::FCommandRecordingState
    {
        std::uint32_t                    FrameIndex;
        float                            LitHeightDisplayScale;
        std::uint32_t                    QueryBase;
        std::uint32_t                    OverlayQueryBase;
        std::vector<TRenderViewMode>     ViewModes;
        bool                             bPrecomputeCoverage = false;
        bool                             bOverlayRendering = false;
        std::uint32_t                    FragmentMode = 0;
        std::vector<std::array<bool, 3>> OverlayActive;
        std::vector<VkDescriptorSet>     OverlayGeometrySets;
    };

    struct TRenderer::FViewportRecordingState
    {
        std::size_t     ViewportIndex;
        TRenderViewMode Mode;
        bool            bTexelGeometry;
        bool            bShowSurfaceDebug;
        bool            bCanShowSurfaceDebug;
        bool            bWireframe;
        bool            bSurfaceLit;
        std::uint32_t   MaterialUniformOffset;
        const TCamera*  ViewCamera;
        glm::mat4       ViewProjection;
        TViewFrustum    ViewFrustum;
    };

// ============================================================
// 00. Command Buffer Recording
// ============================================================
#pragma region 00_Command_Buffer_Recording

    void TRenderer::RecordCommandBuffer(VkCommandBuffer        CommandBuffer,
                                        std::uint32_t          ImageIndex,
                                        const TScene&          SceneData,
                                        const TDebugUI&        DebugInterface,
                                        std::span<const float> SimulationSteps)
    {
        FCommandRecordingState Frame = BeginCommandRecording(CommandBuffer, SceneData, DebugInterface, SimulationSteps);
        RecordSimulationAndInspector(CommandBuffer, SceneData, Frame);
        PrepareDisplayResources(CommandBuffer, SceneData, DebugInterface, Frame);
        BeginSceneGraphicsPass(CommandBuffer, ImageIndex, Frame);
        RecordSceneViewports(CommandBuffer, SceneData, DebugInterface, Frame);
        RecordUIAndEndCommandBuffer(CommandBuffer, ImageIndex, DebugInterface, Frame);
    }

#pragma endregion

// ============================================================
// 01. Frame Preparation
// ============================================================
#pragma region 01_Frame_Preparation

    TRenderer::FCommandRecordingState TRenderer::BeginCommandRecording(VkCommandBuffer        CommandBuffer,
                                                                       const TScene&          SceneData,
                                                                       const TDebugUI&        DebugInterface,
                                                                       std::span<const float> SimulationSteps)
    {
        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin Vulkan command buffer.");
        }

        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        const float         LitHeightDisplayScale = SceneData.GetLitHeightDisplayScale();
        UploadMaterialUniforms(FrameIndex, SceneData, DebugInterface, LitHeightDisplayScale);

        const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
        const std::uint32_t OverlayQueryBase =
            QueryBase + FixedTimestampQueryCount +
            static_cast<std::uint32_t>(SimulationSteps.size()) * SolverTimestampGroupCount * 8U;
        OverlayTimestampLayersSubmitted[FrameIndex] = 0;

        return {FrameIndex, LitHeightDisplayScale, QueryBase, OverlayQueryBase, {}, false, false, 0U, {}, {}};
    }

    void TRenderer::RecordSimulationAndInspector(VkCommandBuffer               CommandBuffer,
                                                 const TScene&                 SceneData,
                                                 const FCommandRecordingState& Frame)
    {
        const auto& FrameIndex = Frame.FrameIndex;
        const auto& LitHeightDisplayScale = Frame.LitHeightDisplayScale;
        const auto& QueryBase = Frame.QueryBase;

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdResetQueryPool(CommandBuffer, TimestampQueryPool, QueryBase, TimestampQueriesPerFrame);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 21U);
        }

        FrameCoordinator.RecordSimulation(
            CommandBuffer, TimestampQueryPool, QueryBase + FixedTimestampQueryCount, SolverTimestampGroupCount * 8U);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 22U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 4U);
        }
        if (TexelInspector && InspectedTexel &&
            DebugStateChannel < SurfaceData.GetSurfaceStateRegistry().GetStateCount())
        {
            const auto& Resources = SurfaceStateSystem.GetGPUResources();
            if (const auto* Descriptors = Resources.GetInstanceDescriptors(InspectedTexel->Instance))
                TexelInspector->Record(
                    CommandBuffer,
                    FrameIndex,
                    *Descriptors,
                    *InspectedTexel,
                    DebugStateChannel,
                    static_cast<std::uint32_t>(Resources.GetInstanceChannelCount(InspectedTexel->Instance)),
                    LitHeightDisplayScale,
                    SceneData.GetStaticMeshInstances()[InspectedTexel->Instance].GetTransform().GetMatrix(),
                    Resources.IsCurrentStateAB(InspectedTexel->Instance),
                    FrameCoordinator.GetStepSerial());
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 5U);
            vkCmdWriteTimestamp(CommandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, TimestampQueryPool, QueryBase);
        }
    }

    void TRenderer::PrepareDisplayResources(VkCommandBuffer         CommandBuffer,
                                            const TScene&           SceneData,
                                            const TDebugUI&         DebugInterface,
                                            FCommandRecordingState& Frame)
    {
        const auto& FrameIndex = Frame.FrameIndex;
        const auto& LitHeightDisplayScale = Frame.LitHeightDisplayScale;
        const auto& QueryBase = Frame.QueryBase;
        const auto& OverlayQueryBase = Frame.OverlayQueryBase;

        std::vector<TRenderViewMode> ViewModes(DebugInterface.GetViewportCount(), TRenderViewMode::Lit);
        bool                         bAnyTexelGeometry = false;
        bool                         bAnyLitView = false;
        for (std::size_t ViewportIndex = 0; ViewportIndex < ViewModes.size(); ++ViewportIndex)
        {
            ViewModes[ViewportIndex] = GetRenderViewMode(ViewportIndex);
            bAnyTexelGeometry |= UsesTexelGeometry(ViewModes[ViewportIndex]);
            bAnyLitView |= ViewModes[ViewportIndex] == TRenderViewMode::Lit;
        }

        bAnyTexelGeometry &= TexelGeometryPreview && TexelGeometryPipeline;
        const auto          DemoBindings = GetDemoSurfaceStateBindings();
        const bool          bPrecomputeCoverage = DemoEffects.bCoverageSmoothing && bPrecomputeCoverageSmoothingEnabled;
        const bool          bUseRenderStateTexture = bRenderStateTextureSamplingEnabled || bPrecomputeCoverage;
        const std::uint32_t FragmentMode = bPrecomputeCoverage ? 2U : (bUseRenderStateTexture ? 1U : 0U);

        if (bAnyLitView && DemoEffects.bEnabled && RenderStateTexture)
            RenderStateTexture->Record(
                CommandBuffer,
                SurfaceStateSystem.GetGPUResources(),
                {DemoBindings.Heat, DemoBindings.Mud, DemoBindings.WaterFilm, DemoBindings.Lava},
                static_cast<std::uint32_t>(SurfaceData.GetSurfaceStateRegistry().GetStateCount()),
                bUseRenderStateTexture,
                bPrecomputeCoverage,
                bSeparableCoverageSmoothingEnabled,
                bSharedStateTextureSmoothingEnabled);

        const bool                       bOverlayRendering = bAnyLitView && CanRenderLitOverlays();
        std::vector<std::array<bool, 3>> OverlayActive(SceneData.GetStaticMeshInstances().size(),
                                                       {false, false, false});
        std::vector<VkDescriptorSet>     OverlayGeometrySets(SceneData.GetStaticMeshInstances().size(), VK_NULL_HANDLE);

        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 12U);
        if (bAnyTexelGeometry || bOverlayRendering)
        {
            const auto& Resources = SurfaceStateSystem.GetGPUResources();
            for (std::size_t Instance = 0; Instance < SceneData.GetStaticMeshInstances().size(); ++Instance)
            {
                const auto* Shared = Resources.GetInstanceSharedGeometry(Instance);
                const auto* Descriptors = Resources.GetInstanceDescriptors(Instance);
                if (!Shared || !Descriptors || !Shared->GetTexelMeshIndexBuffer())
                    continue;
                const auto& MeshInstance = SceneData.GetStaticMeshInstances()[Instance];
                const auto  ChannelCount = static_cast<std::uint32_t>(Resources.GetInstanceChannelCount(Instance));
                const auto  Model = MeshInstance.GetTransform().GetMatrix();
                const bool  bStateAB = Resources.IsCurrentStateAB(Instance);

                if (bAnyTexelGeometry)
                {
                    for (std::size_t ViewportIndex = 0; ViewportIndex < ViewModes.size(); ++ViewportIndex)
                    {
                        const TRenderViewMode Mode = ViewModes[ViewportIndex];
                        if (!UsesTexelGeometry(Mode))
                            continue;
                        const bool bAccumulation = Mode == TRenderViewMode::SurfaceAccumulation ||
                                                   Mode == TRenderViewMode::SurfaceFinalGeometry ||
                                                   Mode == TRenderViewMode::TotalSimulationHeight;
                        const bool bTotalHeight = Mode == TRenderViewMode::TotalSimulationHeight;
                        TexelGeometryPreview->Record(CommandBuffer,
                                                     Instance,
                                                     *Descriptors,
                                                     static_cast<std::uint32_t>(Shared->GetTexelCount()),
                                                     DebugStateChannel,
                                                     ChannelCount,
                                                     LitHeightDisplayScale,
                                                     bAccumulation ? SurfaceDebugSettings.DisplacementScale : 1.0F,
                                                     Model,
                                                     bStateAB,
                                                     bAccumulation,
                                                     VK_NULL_HANDLE,
                                                     0U,
                                                     bTotalHeight,
                                                     0U,
                                                     {0xffffffffU, 0xffffffffU, 0xffffffffU},
                                                     0U,
                                                     ViewportIndex + 1U);
                    }
                }
                if (!bOverlayRendering)
                    continue;

                OverlayActive[Instance] = GetLitOverlayActivity(MeshInstance, Shared, DemoBindings);
                const auto [bMud, bWater, bLava] = OverlayActive[Instance];
                if (bMud || bWater || bLava)
                    TexelGeometryPreview->Record(CommandBuffer,
                                                 Instance,
                                                 *Descriptors,
                                                 static_cast<std::uint32_t>(Shared->GetTexelCount()),
                                                 0U,
                                                 ChannelCount,
                                                 0.0F,
                                                 1.0F,
                                                 Model,
                                                 bStateAB,
                                                 false);

                // Reserved keys identify derived geometry, never Registry State channels.
                constexpr std::uint32_t CombinedSurfaceKey = 0xffffU;
                constexpr std::uint32_t AllStateChannels = std::numeric_limits<std::uint32_t>::max();
                const auto              RecordSurface =
                    [&](SurfaceState::TTexelGeometryPreview& Preview, std::uint32_t Key, bool bSmoothCoverage)
                {
                    const std::uint32_t LayerQuery = OverlayQueryBase + OverlayTimestampLayersSubmitted[FrameIndex] *
                                                                            OverlayTimestampQueriesPerLayer;
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                        vkCmdWriteTimestamp(
                            CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, LayerQuery);
                    Preview.Record(
                        CommandBuffer,
                        Instance,
                        *Descriptors,
                        static_cast<std::uint32_t>(Shared->GetTexelCount()),
                        AllStateChannels,
                        ChannelCount,
                        LitHeightDisplayScale,
                        1.0F,
                        Model,
                        bStateAB,
                        true,
                        TimestampQueryPool,
                        LayerQuery + 12U,
                        true,
                        FrameIndex,
                        {DemoBindings.Mud, DemoBindings.WaterFilm, DemoBindings.Lava, DemoBindings.Ice},
                        (bMud ? 1U : 0U) | (bWater ? 2U : 0U) | (bLava ? 4U : 0U) |
                            (bMud && DemoEffects.bIceDisplacement && DemoBindings.Ice != SurfaceState::InvalidStateId
                                 ? 8U
                                 : 0U));
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                    {
                        vkCmdWriteTimestamp(
                            CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, LayerQuery + 1U);
                        vkCmdWriteTimestamp(
                            CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, LayerQuery + 2U);
                    }
                    VkDescriptorSet        GeometrySet = Preview.GetOutputSet(Instance);
                    const GPU::TGPUBuffer* GeometryBuffer = &Preview.GetOutputBuffer(Instance);
                    const auto             SmoothChannel = [&](SurfaceState::TStateId Channel)
                    {
                        HeightFieldSmoothing->Record(CommandBuffer,
                                                     Instance,
                                                     Channel,
                                                     ChannelCount,
                                                     static_cast<std::uint32_t>(Shared->GetTexelCount()),
                                                     *Descriptors,
                                                     GeometrySet,
                                                     *GeometryBuffer,
                                                     Preview.GetGeometryCacheBuffer(Instance),
                                                     bStateAB,
                                                     LitHeightDisplayScale,
                                                     static_cast<std::uint32_t>(OverlayOccupancyTileSize),
                                                     bSparseHeightSmoothingEnabled && bOverlayTileCullingEnabled,
                                                     bSharedHeightSmoothingEnabled);
                        GeometrySet = HeightFieldSmoothing->GetOutputSet(Instance, Channel);
                        GeometryBuffer = &HeightFieldSmoothing->GetOutputBuffer(Instance, Channel);
                    };
                    if (DemoEffects.bHeightFieldSmoothing)
                    {
                        if (bMud && DemoBindings.Mud != SurfaceState::InvalidStateId)
                            SmoothChannel(DemoBindings.Mud);
                        if (bMud && DemoEffects.bIceDisplacement && DemoBindings.Ice != SurfaceState::InvalidStateId)
                            SmoothChannel(DemoBindings.Ice);
                        if (bWater && DemoBindings.WaterFilm != SurfaceState::InvalidStateId)
                            SmoothChannel(DemoBindings.WaterFilm);
                        if (bLava && DemoBindings.Lava != SurfaceState::InvalidStateId)
                            SmoothChannel(DemoBindings.Lava);
                    }
                    OverlayGeometrySets[Instance] = GeometrySet;
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                    {
                        vkCmdWriteTimestamp(
                            CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, LayerQuery + 3U);
                        vkCmdWriteTimestamp(
                            CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, LayerQuery + 4U);
                    }
                    OverlaySides->Record(
                        CommandBuffer,
                        Instance,
                        Key,
                        ChannelCount,
                        OverlayTexelMeshResolution,
                        *Shared,
                        *Descriptors,
                        GeometrySet,
                        Preview.GetGeometryCacheBuffer(Instance),
                        bStateAB,
                        FrameIndex,
                        TimestampQueryPool,
                        LayerQuery + 4U,
                        bOverlayTileCullingEnabled ? static_cast<std::uint32_t>(OverlayOccupancyTileSize) : 0U,
                        bSmoothCoverage,
                        false,
                        LitHeightDisplayScale,
                        {DemoBindings.Mud, DemoBindings.WaterFilm, DemoBindings.Lava, DemoBindings.Ice},
                        (bMud ? 1U : 0U) | (bWater ? 2U : 0U) | (bLava ? 4U : 0U) |
                            (bMud && DemoEffects.bIceDisplacement && DemoBindings.Ice != SurfaceState::InvalidStateId
                                 ? 8U
                                 : 0U),
                        bSharedOverlayCoverageEnabled,
                        DemoEffects.LavaThreshold);
                    if (TimestampQueryPool != VK_NULL_HANDLE)
                        ++OverlayTimestampLayersSubmitted[FrameIndex];
                };
                if (bMud || bWater || bLava)
                {
                    // Accumulation height was produced immediately after the latest solver step.
                    RecordSurface(*MudLayerGeometry, CombinedSurfaceKey, DemoEffects.bCoverageSmoothing);
                }
            }
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 13U);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 1U);

        Frame.ViewModes = std::move(ViewModes);
        Frame.bPrecomputeCoverage = bPrecomputeCoverage;
        Frame.bOverlayRendering = bOverlayRendering;
        Frame.FragmentMode = FragmentMode;
        Frame.OverlayActive = std::move(OverlayActive);
        Frame.OverlayGeometrySets = std::move(OverlayGeometrySets);
    }

#pragma endregion

// ============================================================
// 02. Graphics Pass and Viewports
// ============================================================
#pragma region 02_Graphics_Pass_and_Viewports

    void TRenderer::BeginSceneGraphicsPass(VkCommandBuffer               CommandBuffer,
                                           std::uint32_t                 ImageIndex,
                                           const FCommandRecordingState& Frame)
    {
        const auto& QueryBase = Frame.QueryBase;

        std::array<VkClearValue, 2> ClearValues{};
        ClearValues[0].color = {{0.03F, 0.04F, 0.06F, 1.0F}};
        ClearValues[1].depthStencil = {1.0F, 0};

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 2U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, TimestampQueryPool, QueryBase + 15U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, TimestampQueryPool, QueryBase + 17U);
        }
        MainGraphicsPass.Begin(CommandBuffer, MainFramebuffers, ImageIndex, SwapchainData.GetExtent(), ClearValues);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, TimestampQueryPool, QueryBase + 16U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, TimestampQueryPool, QueryBase + 18U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 14U);
        }
    }

    void TRenderer::RecordSceneViewports(VkCommandBuffer               CommandBuffer,
                                         const TScene&                 SceneData,
                                         const TDebugUI&               DebugInterface,
                                         const FCommandRecordingState& Frame)
    {
        const VkExtent2D Extent = SwapchainData.GetExtent();
        for (std::size_t ViewportIndex = 0; ViewportIndex < DebugInterface.GetViewportCount(); ++ViewportIndex)
        {
            const glm::vec4 NormalizedViewport = DebugInterface.GetViewportRectNormalized(ViewportIndex);
            if (NormalizedViewport.z <= 0.0F || NormalizedViewport.w <= 0.0F)
                continue;
            const FViewportRecordingState View = ConfigureViewport(
                CommandBuffer, SceneData, DebugInterface, Frame, ViewportIndex, NormalizedViewport, Extent);
            const SurfaceState::TSurfaceGPUResourceManager& SurfaceGPU = SurfaceStateSystem.GetGPUResources();
            RecordBaseMeshes(CommandBuffer, SceneData, SurfaceGPU, Frame, View);
            RecordAccumulationOverlays(CommandBuffer, SceneData, SurfaceGPU, Frame, View);
            RecordWorldReferenceAndGizmo(CommandBuffer, SceneData, DebugInterface, View);
        }
    }

    void TRenderer::RecordUIAndEndCommandBuffer(VkCommandBuffer               CommandBuffer,
                                                std::uint32_t                 ImageIndex,
                                                const TDebugUI&               DebugInterface,
                                                const FCommandRecordingState& Frame)
    {
        const auto& QueryBase = Frame.QueryBase;

        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 3U);
        DebugInterface.Render(CommandBuffer);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 19U);

        MainGraphicsPass.End(CommandBuffer);
        if (bBenchmarkCaptureEnabled)
            RecordBenchmarkScreenshot(CommandBuffer, ImageIndex);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 20U);
        if (TimestampQueryPool != VK_NULL_HANDLE)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 23U);
        if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record Vulkan command buffer.");
        }
    }

#pragma endregion

// ============================================================
// 03. Viewport Configuration
// ============================================================
#pragma region 03_Viewport_Configuration

    TRenderer::FViewportRecordingState TRenderer::ConfigureViewport(VkCommandBuffer               CommandBuffer,
                                                                    const TScene&                 SceneData,
                                                                    const TDebugUI&               DebugInterface,
                                                                    const FCommandRecordingState& Frame,
                                                                    std::size_t                   ViewportIndex,
                                                                    const glm::vec4&              NormalizedViewport,
                                                                    VkExtent2D                    Extent)
    {
        const auto& ViewModes = Frame.ViewModes;
        const auto& FragmentMode = Frame.FragmentMode;

        const TRenderViewMode Mode = ViewModes[ViewportIndex];
        const bool            bTexelGeometry = UsesTexelGeometry(Mode) && TexelGeometryPreview && TexelGeometryPipeline;
        const bool            bShowSurfaceDebug = Mode >= TRenderViewMode::SurfaceStateHeatmap;
        const bool            bCanShowSurfaceDebug = bShowSurfaceDebug && SurfaceDebugPipeline != nullptr;
        const bool            bWireframe = Mode == TRenderViewMode::Wireframe;
        const bool            bSurfaceLit =
            Mode == TRenderViewMode::Lit && DemoEffects.bEnabled && SurfaceLitPipelines[FragmentMode];
        vkCmdBindPipeline(CommandBuffer,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          bTexelGeometry ? TexelGeometryPipeline->GetHandle()
                          : bCanShowSurfaceDebug
                              ? SurfaceDebugPipeline->GetHandle()
                              : (bWireframe ? WireframePipeline.GetHandle() : StaticMeshPipeline.GetHandle()));
        if (bWireframe)
        {
            // Dynamic line width must be set before drawing with the wireframe pipeline.
            // Without wideLines support, Vulkan only permits the default width of 1.0.
            vkCmdSetLineWidth(CommandBuffer, bSupportsWireframeLineWidth ? WireframeLineWidth : 1.0F);
        }
        const TCamera&      ViewCamera = DebugInterface.GetViewportCamera(SceneData, ViewportIndex);
        const std::uint32_t MaterialUniformOffset = static_cast<std::uint32_t>(MaterialUniformStride * ViewportIndex);
        const std::uint32_t ViewportX = std::min(
            static_cast<std::uint32_t>(std::max(NormalizedViewport.x, 0.0F) * static_cast<float>(Extent.width)),
            Extent.width - 1U);
        const std::uint32_t ViewportY = std::min(
            static_cast<std::uint32_t>(std::max(NormalizedViewport.y, 0.0F) * static_cast<float>(Extent.height)),
            Extent.height - 1U);
        const std::uint32_t ViewportWidth = std::clamp(
            static_cast<std::uint32_t>(std::max(NormalizedViewport.z, 0.0F) * static_cast<float>(Extent.width)),
            1U,
            Extent.width - ViewportX);
        const std::uint32_t ViewportHeight = std::clamp(
            static_cast<std::uint32_t>(std::max(NormalizedViewport.w, 0.0F) * static_cast<float>(Extent.height)),
            1U,
            Extent.height - ViewportY);

        VkViewport Viewport{};
        Viewport.x = static_cast<float>(ViewportX);
        Viewport.y = static_cast<float>(ViewportY);
        Viewport.width = static_cast<float>(ViewportWidth);
        Viewport.height = static_cast<float>(ViewportHeight);
        Viewport.minDepth = 0.0F;
        Viewport.maxDepth = 1.0F;
        vkCmdSetViewport(CommandBuffer, 0, 1, &Viewport);

        VkRect2D Scissor{};
        Scissor.offset = {static_cast<std::int32_t>(ViewportX), static_cast<std::int32_t>(ViewportY)};
        Scissor.extent = {ViewportWidth, ViewportHeight};
        vkCmdSetScissor(CommandBuffer, 0, 1, &Scissor);

        const float        AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
        const glm::mat4    ViewProjection = ViewCamera.GetViewProjectionMatrix(AspectRatio);
        const TViewFrustum ViewFrustum(ViewProjection);
        return {ViewportIndex,
                Mode,
                bTexelGeometry,
                bShowSurfaceDebug,
                bCanShowSurfaceDebug,
                bWireframe,
                bSurfaceLit,
                MaterialUniformOffset,
                &ViewCamera,
                ViewProjection,
                ViewFrustum};
    }

#pragma endregion

// ============================================================
// 04. Base Mesh Drawing
// ============================================================
#pragma region 04_Base_Mesh_Drawing

    void TRenderer::RecordBaseMeshes(VkCommandBuffer                                 CommandBuffer,
                                     const TScene&                                   SceneData,
                                     const SurfaceState::TSurfaceGPUResourceManager& SurfaceGPU,
                                     const FCommandRecordingState&                   Frame,
                                     const FViewportRecordingState&                  View)
    {
        const auto& FrameIndex = Frame.FrameIndex;
        const auto& QueryBase = Frame.QueryBase;
        const auto& bOverlayRendering = Frame.bOverlayRendering;
        const auto& bPrecomputeCoverage = Frame.bPrecomputeCoverage;
        const auto& FragmentMode = Frame.FragmentMode;
        const auto& OverlayActive = Frame.OverlayActive;

        const auto& ViewportIndex = View.ViewportIndex;
        const auto& Mode = View.Mode;
        const auto& bTexelGeometry = View.bTexelGeometry;
        const auto& bShowSurfaceDebug = View.bShowSurfaceDebug;
        const auto& bCanShowSurfaceDebug = View.bCanShowSurfaceDebug;
        const auto& bWireframe = View.bWireframe;
        const auto& bSurfaceLit = View.bSurfaceLit;
        const auto& MaterialUniformOffset = View.MaterialUniformOffset;
        const auto& ViewProjection = View.ViewProjection;
        const auto& ViewFrustum = View.ViewFrustum;

        if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 6U);
        std::size_t SceneIndex = 0;
        for (const TStaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
        {
            const std::size_t CurrentSceneIndex = SceneIndex++;
            if (Instance.GetMesh() == Asset::InvalidAssetHandle)
            {
                continue;
            }

            const Asset::TMeshAsset& Mesh = Assets.GetMesh(Instance.GetMesh());
            // Debug texel geometry can extend beyond the source mesh; its bounds are not static.
            if (!bTexelGeometry &&
                !IsMeshSphereVisible(
                    ViewFrustum, Mesh.GetBoundsCenter(), Mesh.GetBoundsRadius(), Instance.GetTransform().GetMatrix()))
                continue;
            const SurfaceState::TSurfaceStateDescriptorResources* SurfaceDescriptors =
                SurfaceGPU.GetInstanceDescriptors(CurrentSceneIndex);
            if (bShowSurfaceDebug && (!bCanShowSurfaceDebug || SurfaceDescriptors == nullptr))
            {
                continue;
            }
            const auto* Shared = SurfaceGPU.GetInstanceSharedGeometry(CurrentSceneIndex);
            const bool  bInstanceOverlay = Mode == TRenderViewMode::Lit && bOverlayRendering &&
                                          (OverlayActive[CurrentSceneIndex][0] || OverlayActive[CurrentSceneIndex][1] ||
                                           OverlayActive[CurrentSceneIndex][2]);
            if (bOverlayOnlyDebug && Mode == TRenderViewMode::Lit && bOverlayRendering)
                continue;
            if (!bBaseMeshDrawEnabled)
                continue;
            if (bTexelGeometry || bInstanceOverlay)
            {
                if (!Shared || !Shared->GetTexelMeshIndexBuffer())
                    continue;
                const auto& SurfaceMesh = Shared->GetTexelMeshVariant(SurfaceTexelMeshResolution);
                if (!bTexelGeometry && (!SurfaceMesh.IndexBuffer || !SurfaceMesh.VertexBuffer))
                    continue;
                const auto& Pipeline = bTexelGeometry ? TexelGeometryPipeline : BaseSurfaceLitPipelines[FragmentMode];
                vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline->GetHandle());
                const auto                     Layout = Pipeline->GetLayout();
                const TStaticMeshPushConstants Push{Instance.GetTransform().GetMatrix(), ViewProjection};
                vkCmdPushConstants(CommandBuffer,
                                   Layout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0,
                                   sizeof(Push),
                                   &Push);
                const VkBuffer     TexelVertices = bTexelGeometry ? Shared->GetTexelMeshVertexBuffer()->GetHandle()
                                                                  : SurfaceMesh.VertexBuffer->GetHandle();
                const VkDeviceSize TexelOffset = 0;
                vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &TexelVertices, &TexelOffset);
                const VkBuffer TexelIndices = bTexelGeometry ? Shared->GetTexelMeshIndexBuffer()->GetHandle()
                                                             : SurfaceMesh.IndexBuffer->GetHandle();
                vkCmdBindIndexBuffer(CommandBuffer, TexelIndices, 0, VK_INDEX_TYPE_UINT32);
                const std::array<VkDescriptorSet, 2> Sets{
                    SurfaceGPU.IsCurrentStateAB(CurrentSceneIndex) ? SurfaceDescriptors->GetABSet()
                                                                   : SurfaceDescriptors->GetBASet(),
                    TexelGeometryPreview->GetOutputSet(CurrentSceneIndex, bTexelGeometry ? ViewportIndex + 1U : 0U)};
                vkCmdBindDescriptorSets(
                    CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 1, Sets.size(), Sets.data(), 0, nullptr);
                if (!bTexelGeometry)
                {
                    const VkDescriptorSet RenderSet =
                        RenderStateTexture->GetSet(CurrentSceneIndex, bPrecomputeCoverage);
                    vkCmdBindDescriptorSets(
                        CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 3, 1, &RenderSet, 0, nullptr);
                }
                for (const auto& Section : Mesh.GetSections())
                {
                    if (Section.Material >= MaterialResources.size() ||
                        Section.Surface >=
                            (bTexelGeometry ? Shared->GetTexelMeshRanges().size() : SurfaceMesh.Ranges.size()))
                        continue;
                    const auto& Range = bTexelGeometry ? Shared->GetTexelMeshRanges()[Section.Surface]
                                                       : SurfaceMesh.Ranges[Section.Surface];
                    if (Range.IndexCount == 0)
                        continue;
                    const auto MaterialSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                    vkCmdBindDescriptorSets(CommandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            Layout,
                                            0,
                                            1,
                                            &MaterialSet,
                                            1,
                                            &MaterialUniformOffset);
                    vkCmdDrawIndexed(CommandBuffer, Range.IndexCount, 1, Range.FirstIndex, 0, 0);
                }
                continue;
            }
            const bool  bInstanceSurfaceLit = bSurfaceLit && SurfaceDescriptors;
            const auto& Pipeline = bInstanceSurfaceLit
                                       ? *SurfaceLitPipelines[FragmentMode]
                                       : (bCanShowSurfaceDebug ? *SurfaceDebugPipeline
                                                               : (bWireframe ? WireframePipeline : StaticMeshPipeline));
            const auto  Layout = Pipeline.GetLayout();
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.GetHandle());
            const VkBuffer     VertexBuffer = Mesh.GetVertexBuffer().GetHandle();
            const VkDeviceSize VertexOffset = 0;
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &VertexOffset);
            vkCmdBindIndexBuffer(CommandBuffer, Mesh.GetIndexBuffer().GetHandle(), 0, VK_INDEX_TYPE_UINT32);

            const TStaticMeshPushConstants PushConstants{Instance.GetTransform().GetMatrix(), ViewProjection};
            vkCmdPushConstants(CommandBuffer,
                               Layout,
                               (bInstanceSurfaceLit || bCanShowSurfaceDebug)
                                   ? (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                                   : VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(PushConstants),
                               &PushConstants);

            for (const Asset::TMeshSection& Section : Mesh.GetSections())
            {
                if (Section.Material >= MaterialResources.size())
                {
                    continue;
                }

                const VkDescriptorSet DescriptorSet = MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                vkCmdBindDescriptorSets(CommandBuffer,
                                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        Layout,
                                        0,
                                        1,
                                        &DescriptorSet,
                                        1,
                                        &MaterialUniformOffset);

                if (bCanShowSurfaceDebug || bInstanceSurfaceLit)
                {
                    const VkDescriptorSet StateSet = SurfaceGPU.IsCurrentStateAB(CurrentSceneIndex)
                                                         ? SurfaceDescriptors->GetABSet()
                                                         : SurfaceDescriptors->GetBASet();
                    vkCmdBindDescriptorSets(
                        CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 1, 1, &StateSet, 0, nullptr);
                }
                if (bInstanceSurfaceLit)
                {
                    const VkDescriptorSet RenderSet =
                        RenderStateTexture->GetSet(CurrentSceneIndex, bPrecomputeCoverage);
                    vkCmdBindDescriptorSets(
                        CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Layout, 2, 1, &RenderSet, 0, nullptr);
                }

                vkCmdDrawIndexed(CommandBuffer,
                                 Section.IndexCount,
                                 1,
                                 Section.FirstIndex,
                                 0,
                                 (bCanShowSurfaceDebug || bInstanceSurfaceLit) ? Section.Surface : 0U);
            }
        }

        if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 7U);
    }

#pragma endregion

// ============================================================
// 05. Accumulation Overlay Drawing
// ============================================================
#pragma region 05_Accumulation_Overlay_Drawing

    void TRenderer::RecordAccumulationOverlays(VkCommandBuffer                                 CommandBuffer,
                                               const TScene&                                   SceneData,
                                               const SurfaceState::TSurfaceGPUResourceManager& SurfaceGPU,
                                               const FCommandRecordingState&                   Frame,
                                               const FViewportRecordingState&                  View)
    {
        const auto& FrameIndex = Frame.FrameIndex;
        const auto& QueryBase = Frame.QueryBase;
        const auto& bOverlayRendering = Frame.bOverlayRendering;
        const auto& bPrecomputeCoverage = Frame.bPrecomputeCoverage;
        const auto& FragmentMode = Frame.FragmentMode;
        const auto& OverlayActive = Frame.OverlayActive;
        const auto& OverlayGeometrySets = Frame.OverlayGeometrySets;

        const auto& ViewportIndex = View.ViewportIndex;
        const auto& Mode = View.Mode;
        const auto& MaterialUniformOffset = View.MaterialUniformOffset;
        const auto& ViewProjection = View.ViewProjection;

        if (bOverlayRendering && Mode == TRenderViewMode::Lit)
        {
            const auto DrawLayer = [&]()
            {
                const auto&              TopPipeline = MudOverlayTopPipelines[FragmentMode];
                const auto&              SidePipeline = MudOverlaySidePipelines[FragmentMode];
                std::vector<std::size_t> Order;
                for (std::size_t I = 0; I < OverlayActive.size(); ++I)
                    if (OverlayActive[I][0] || OverlayActive[I][1] || OverlayActive[I][2])
                        Order.push_back(I);
                for (const std::size_t I : Order)
                {
                    constexpr std::uint32_t Channel = 0xffffU;
                    const auto&             Instance = SceneData.GetStaticMeshInstances()[I];
                    const auto&             Mesh = Assets.GetMesh(Instance.GetMesh());
                    const auto*             Shared = SurfaceGPU.GetInstanceSharedGeometry(I);
                    const auto*             Descriptors = SurfaceGPU.GetInstanceDescriptors(I);
                    if (!Shared || !Descriptors)
                        continue;
                    const auto& OverlayMesh = Shared->GetTexelMeshVariant(OverlayTexelMeshResolution);
                    if (!OverlayMesh.IndexBuffer || !OverlayMesh.VertexBuffer)
                        continue;
                    const VkDescriptorSet StateSet =
                        SurfaceGPU.IsCurrentStateAB(I) ? Descriptors->GetABSet() : Descriptors->GetBASet();
                    const VkDescriptorSet          ComputedSet = OverlayGeometrySets[I];
                    const TStaticMeshPushConstants Push{Instance.GetTransform().GetMatrix(), ViewProjection};

                    const auto TopLayout = TopPipeline->GetLayout();
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TopPipeline->GetHandle());
                    vkCmdPushConstants(CommandBuffer,
                                       TopLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0,
                                       sizeof(Push),
                                       &Push);
                    const VkBuffer     Vertices = OverlayMesh.VertexBuffer->GetHandle();
                    const VkDeviceSize Offset = 0;
                    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Vertices, &Offset);
                    vkCmdBindIndexBuffer(CommandBuffer,
                                         OverlaySides->GetTopIndexBuffer(I, Channel, OverlayTexelMeshResolution),
                                         OverlaySides->GetTopIndexOffset(I, Channel, OverlayTexelMeshResolution),
                                         VK_INDEX_TYPE_UINT32);
                    const std::array<VkDescriptorSet, 3> TopSets{
                        StateSet, ComputedSet, OverlaySides->GetSet(I, Channel, OverlayTexelMeshResolution)};
                    vkCmdBindDescriptorSets(CommandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            TopLayout,
                                            1,
                                            static_cast<std::uint32_t>(TopSets.size()),
                                            TopSets.data(),
                                            0,
                                            nullptr);
                    const VkDescriptorSet RenderSet = RenderStateTexture->GetSet(I, bPrecomputeCoverage);
                    vkCmdBindDescriptorSets(
                        CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, TopLayout, 4, 1, &RenderSet, 0, nullptr);
                    const VkBuffer TopDrawBuffer =
                        OverlaySides->GetTopDrawBuffer(I, Channel, OverlayTexelMeshResolution);
                    if (bOverlayTopDrawEnabled && OverlayDisplayMode != TOverlayDisplayMode::SidesOnly)
                    {
                        for (const auto& Section : Mesh.GetSections())
                        {
                            if (Section.Material >= MaterialResources.size() ||
                                Section.Surface >= OverlayMesh.Ranges.size())
                                continue;
                            const auto& Range = OverlayMesh.Ranges[Section.Surface];
                            if (Range.IndexCount == 0)
                                continue;
                            const VkDescriptorSet MaterialSet =
                                MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                            vkCmdBindDescriptorSets(CommandBuffer,
                                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    TopLayout,
                                                    0,
                                                    1,
                                                    &MaterialSet,
                                                    1,
                                                    &MaterialUniformOffset);
                            vkCmdDrawIndexedIndirect(CommandBuffer,
                                                     TopDrawBuffer,
                                                     static_cast<VkDeviceSize>(Section.Surface) *
                                                         sizeof(VkDrawIndexedIndirectCommand),
                                                     1,
                                                     0);
                        }
                    }

                    const auto SideLayout = SidePipeline->GetLayout();
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SidePipeline->GetHandle());
                    vkCmdPushConstants(CommandBuffer,
                                       SideLayout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                       0,
                                       sizeof(Push),
                                       &Push);
                    const std::array<VkDescriptorSet, 3> SideSets{
                        StateSet, ComputedSet, OverlaySides->GetSet(I, Channel, OverlayTexelMeshResolution)};
                    vkCmdBindDescriptorSets(CommandBuffer,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            SideLayout,
                                            1,
                                            static_cast<std::uint32_t>(SideSets.size()),
                                            SideSets.data(),
                                            0,
                                            nullptr);
                    vkCmdBindDescriptorSets(
                        CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SideLayout, 4, 1, &RenderSet, 0, nullptr);
                    const VkBuffer SideDrawBuffer = OverlaySides->GetDrawBuffer(I, Channel, OverlayTexelMeshResolution);
                    if (bOverlaySidesDrawEnabled && OverlayDisplayMode != TOverlayDisplayMode::TopOnly)
                    {
                        for (const auto& Section : Mesh.GetSections())
                        {
                            if (Section.Material >= MaterialResources.size() ||
                                Section.Surface >= OverlayMesh.Ranges.size())
                                continue;
                            const auto& Range = OverlayMesh.Ranges[Section.Surface];
                            if (Range.IndexCount == 0)
                                continue;
                            const VkDescriptorSet MaterialSet =
                                MaterialResources[Section.Material].DescriptorSets[FrameIndex];
                            vkCmdBindDescriptorSets(CommandBuffer,
                                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    SideLayout,
                                                    0,
                                                    1,
                                                    &MaterialSet,
                                                    1,
                                                    &MaterialUniformOffset);
                            vkCmdDrawIndirect(CommandBuffer,
                                              SideDrawBuffer,
                                              static_cast<VkDeviceSize>(Section.Surface) *
                                                  sizeof(VkDrawIndirectCommand),
                                              1,
                                              0);
                        }
                    }
                    if (bOverlaySidesDrawEnabled && OverlayDisplayMode != TOverlayDisplayMode::TopOnly &&
                        OverlaySides->GetBoundaryCount(I, Channel, OverlayTexelMeshResolution) > 0 &&
                        !Mesh.GetSections().empty())
                    {
                        const auto Material = Mesh.GetSections().front().Material;
                        if (Material < MaterialResources.size())
                        {
                            const VkDescriptorSet MaterialSet = MaterialResources[Material].DescriptorSets[FrameIndex];
                            vkCmdBindDescriptorSets(CommandBuffer,
                                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                    SideLayout,
                                                    0,
                                                    1,
                                                    &MaterialSet,
                                                    1,
                                                    &MaterialUniformOffset);
                            vkCmdDrawIndirect(CommandBuffer,
                                              SideDrawBuffer,
                                              static_cast<VkDeviceSize>(OverlayMesh.Ranges.size()) *
                                                  sizeof(VkDrawIndirectCommand),
                                              1,
                                              0);
                        }
                    }
                }
            };
            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 8U);
            DrawLayer();
            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 9U);
            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 10U);
            if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
                vkCmdWriteTimestamp(
                    CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 11U);
        }
        else if (TimestampQueryPool != VK_NULL_HANDLE && ViewportIndex == 0)
        {
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 8U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 9U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 10U);
            vkCmdWriteTimestamp(
                CommandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, TimestampQueryPool, QueryBase + 11U);
        }
    }

#pragma endregion

// ============================================================
// 06. World Reference and Gizmo Drawing
// ============================================================
#pragma region 06_World_Reference_and_Gizmo_Drawing

    void TRenderer::RecordWorldReferenceAndGizmo(VkCommandBuffer                CommandBuffer,
                                                 const TScene&                  SceneData,
                                                 const TDebugUI&                DebugInterface,
                                                 const FViewportRecordingState& View)
    {
        const auto& ViewProjection = View.ViewProjection;

        const TCamera& ViewCamera = *View.ViewCamera;

        if (GizmoVertexBuffer != nullptr &&
            ((bWorldGridVisible && WorldGridVertexCount > 0) || (bWorldAxisVisible && WorldAxisVertexCount > 0)))
        {
            const TGizmoPushConstants Constants{ViewProjection};
            vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, WorldReferencePipeline.GetHandle());
            const VkBuffer     VertexBuffer = GizmoVertexBuffer->GetHandle();
            const VkDeviceSize Offset = 0;
            vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &Offset);
            vkCmdPushConstants(CommandBuffer,
                               WorldReferencePipeline.GetLayout(),
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(Constants),
                               &Constants);
            if (bWorldGridVisible)
            {
                vkCmdDraw(CommandBuffer, WorldGridVertexCount, 1, 0, 0);
            }
            if (bWorldAxisVisible)
            {
                vkCmdDraw(CommandBuffer, WorldAxisVertexCount, 1, WorldGridVertexCount, 0);
            }
        }

        if (!DebugInterface.IsInjectModeEnabled())
        {
            if (const std::optional<std::size_t> Selected = DebugInterface.GetSelectedObject();
                Selected && *Selected < SceneData.GetStaticMeshInstances().size() && GizmoVertexBuffer != nullptr)
            {
                const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*Selected].GetTransform().Position;
                const float     GizmoScale = glm::length(ViewCamera.GetPosition() - Position) * 0.18F;
                if (GizmoScale > 0.01F)
                {
                    const glm::mat4 Model =
                        glm::scale(glm::translate(glm::mat4(1.0F), Position), glm::vec3(GizmoScale));
                    const TGizmoPushConstants Constants{ViewProjection * Model, DebugInterface.GetHoveredGizmoAxis()};
                    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GizmoPipeline.GetHandle());
                    const VkBuffer     VertexBuffer = GizmoVertexBuffer->GetHandle();
                    const VkDeviceSize Offset = 0;
                    vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &VertexBuffer, &Offset);
                    vkCmdPushConstants(CommandBuffer,
                                       GizmoPipeline.GetLayout(),
                                       VK_SHADER_STAGE_VERTEX_BIT,
                                       0,
                                       sizeof(Constants),
                                       &Constants);
                    const bool          bRotationGizmo = DebugInterface.IsRotationGizmoMode();
                    const std::uint32_t FirstVertex =
                        WorldGridVertexCount + WorldAxisVertexCount + (bRotationGizmo ? TranslateGizmoVertexCount : 0U);
                    const std::uint32_t VertexCount =
                        bRotationGizmo ? RotateGizmoVertexCount : TranslateGizmoVertexCount;
                    vkCmdDraw(CommandBuffer, VertexCount, 1, FirstVertex, 0);
                }
            }
        }
    }

#pragma endregion
} // namespace MDSS::Rendering
