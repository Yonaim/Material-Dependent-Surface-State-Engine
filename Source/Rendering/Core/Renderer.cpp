#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
// ============================================================
// 00. Renderer Lifecycle
// ============================================================
#pragma region 00_Renderer_Lifecycle

    TRenderer::TRenderer(const GPU::TVulkanContext&         Context,
                         TWindow&                           TWindow,
                         Asset::TAssetManager&              Assets,
                         SurfaceState::TSurfaceDataManager& SurfaceData,
                         const TScene&                      Scene,
                         SurfaceState::TSurfaceStateSystem& SurfaceStateSystem,
                         TFrameCoordinator&                 FrameCoordinator)
        : Context(Context), TargetWindow(TWindow), Assets(Assets), SurfaceData(SurfaceData),
          SurfaceStateSystem(SurfaceStateSystem), FrameCoordinator(FrameCoordinator), SwapchainData(Context, TWindow),
          DepthFormat(FindDepthFormat(Context.GetPhysicalDevice())),
          DepthImages(CreateDepthImages(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        SwapchainData.GetExtent(),
                                        DepthFormat,
                                        SwapchainData.GetImageViews().size())),
          DepthImageViews(CreateDepthImageViews(Context.GetDevice(), DepthImages, DepthFormat)),
          MainRenderPass(Context.GetDevice(), SwapchainData.GetImageFormat(), DepthFormat),
          MainGraphicsPass(MainRenderPass),
          MaterialDescriptorSetLayout(CreateMaterialDescriptorSetLayout(Context.GetDevice())),
          StaticMeshPipeline(Context.GetDevice(),
                             MainRenderPass.GetHandle(),
                             BuildStaticMeshPipelineConfig(MaterialDescriptorSetLayout)),
          WireframePipeline(Context.GetDevice(),
                            MainRenderPass.GetHandle(),
                            BuildWireframePipelineConfig(MaterialDescriptorSetLayout)),
          GizmoPipeline(Context.GetDevice(), MainRenderPass.GetHandle(), BuildGizmoPipelineConfig()),
          WorldReferencePipeline(Context.GetDevice(), MainRenderPass.GetHandle(), BuildWorldReferencePipelineConfig()),
          MainFramebuffers(Context.GetDevice(),
                           MainRenderPass.GetHandle(),
                           SwapchainData.GetExtent(),
                           SwapchainData.GetImageViews(),
                           GetDepthImageViewHandles(DepthImageViews)),
          FrameContext(Context)
    {
        TScopeExit ConstructionCleanup(
            [this]
            {
                // The TRenderer destructor is not called when construction fails. Release raw Vulkan
                // handles owned by this object; RAII members clean themselves during stack unwinding.
                if (TimestampQueryPool != VK_NULL_HANDLE)
                {
                    vkDestroyQueryPool(this->Context.GetDevice(), TimestampQueryPool, nullptr);
                    TimestampQueryPool = VK_NULL_HANDLE;
                }
                DestroyRenderFinishedSemaphores();
                if (MaterialDescriptorPool != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorPool(this->Context.GetDevice(), MaterialDescriptorPool, nullptr);
                    MaterialDescriptorPool = VK_NULL_HANDLE;
                }
                MaterialResources.clear();
                if (MaterialDescriptorSetLayout != VK_NULL_HANDLE)
                {
                    vkDestroyDescriptorSetLayout(this->Context.GetDevice(), MaterialDescriptorSetLayout, nullptr);
                    MaterialDescriptorSetLayout = VK_NULL_HANDLE;
                }
            });

        VkPhysicalDeviceFeatures DeviceFeatures{};
        vkGetPhysicalDeviceFeatures(Context.GetPhysicalDevice(), &DeviceFeatures);
        VkPhysicalDeviceDriverProperties DriverProperties{};
        DriverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
        VkPhysicalDeviceProperties2 DevicePropertiesWithDriver{};
        DevicePropertiesWithDriver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        DevicePropertiesWithDriver.pNext = &DriverProperties;
        vkGetPhysicalDeviceProperties2(Context.GetPhysicalDevice(), &DevicePropertiesWithDriver);
        const VkPhysicalDeviceProperties& DeviceProperties = DevicePropertiesWithDriver.properties;
        const VkDeviceSize                UniformAlignment =
            std::max<VkDeviceSize>(DeviceProperties.limits.minUniformBufferOffsetAlignment, 1U);

        MaterialUniformStride =
            (sizeof(TMaterialUniform) + UniformAlignment - 1U) / UniformAlignment * UniformAlignment;
        bRenderPassSubstageTimingsReliable =
            DriverProperties.driverID != VK_DRIVER_ID_MOLTENVK || DeviceProperties.vendorID != 0x106BU;
        bSupportsWireframeLineWidth = DeviceFeatures.wideLines == VK_TRUE;
        if (bSupportsWireframeLineWidth)
        {
            WireframeLineWidthMin = std::max(1.0F, DeviceProperties.limits.lineWidthRange[0]);
            WireframeLineWidthMax = std::max(WireframeLineWidthMin, DeviceProperties.limits.lineWidthRange[1]);
            WireframeLineWidth = std::clamp(2.0F, WireframeLineWidthMin, WireframeLineWidthMax);
        }

        std::vector<TGizmoVertex> GizmoVertices =
            BuildWorldReferenceVertices(WorldGridVertexCount, WorldAxisVertexCount);
        const std::vector<TGizmoVertex> TranslateGizmoVertices = BuildTranslateGizmoVertices();
        TranslateGizmoVertexCount = static_cast<std::uint32_t>(TranslateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), TranslateGizmoVertices.begin(), TranslateGizmoVertices.end());
        const std::vector<TGizmoVertex> RotateGizmoVertices = BuildRotateGizmoVertices();
        RotateGizmoVertexCount = static_cast<std::uint32_t>(RotateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), RotateGizmoVertices.begin(), RotateGizmoVertices.end());
        GizmoVertexBuffer = std::make_unique<GPU::TGPUBuffer>(
            Context.GetPhysicalDevice(),
            Context.GetDevice(),
            static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            GPU::TGPUBufferMemoryCategory::Renderer);
        GizmoVertexBuffer->Upload(GizmoVertices.data(),
                                  static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(TGizmoVertex)));

        CreateMaterialDescriptorResources();

        if (const SurfaceState::TSurfaceStateDescriptorResources* Descriptors =
                SurfaceStateSystem.GetGPUResources().GetAnyInstanceDescriptors())
        {
            SurfaceDebugPipeline = std::make_unique<GPU::TGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
            TexelInspector = std::make_unique<SurfaceState::TTexelInspector>(Context.GetPhysicalDevice(),
                                                                             Context.GetDevice(),
                                                                             Descriptors->GetLayout(),
                                                                             TRenderContext::MaxFramesInFlight);
            TexelGeometryPreview =
                std::make_unique<SurfaceState::TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                      Context.GetDevice(),
                                                                      Descriptors->GetLayout(),
                                                                      Scene.GetStaticMeshInstances().size());
            TexelGeometryPipeline = std::make_unique<GPU::TGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildTexelGeometryPipelineConfig(
                    MaterialDescriptorSetLayout, Descriptors->GetLayout(), TexelGeometryPreview->GetOutputLayout()));
            RenderStateTexture = std::make_unique<TRenderStateTexture>(Context.GetPhysicalDevice(),
                                                                       Context.GetDevice(),
                                                                       Descriptors->GetLayout(),
                                                                       SurfaceStateSystem.GetGPUResources());
            for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
            {
                SurfaceLitPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                  Descriptors->GetLayout(),
                                                  VK_NULL_HANDLE,
                                                  RenderStateTexture->GetLayout(),
                                                  Mode));
                BaseSurfaceLitPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                      Descriptors->GetLayout(),
                                                      TexelGeometryPreview->GetOutputLayout(),
                                                      RenderStateTexture->GetLayout(),
                                                      Mode));
            }
            MudLayerGeometry =
                std::make_unique<SurfaceState::TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                      Context.GetDevice(),
                                                                      Descriptors->GetLayout(),
                                                                      Scene.GetStaticMeshInstances().size(),
                                                                      true);
            MudLayerGeometry->SetLowAmountHeightFade(DemoEffects.LowAmountHeightFade);
            HeightFieldSmoothing = std::make_unique<THeightFieldSmoothing>(Context.GetPhysicalDevice(),
                                                                           Context.GetDevice(),
                                                                           Descriptors->GetLayout(),
                                                                           MudLayerGeometry->GetOutputLayout(),
                                                                           Scene.GetStaticMeshInstances().size());
            OverlaySides = std::make_unique<TAccumulationOverlaySides>(Context.GetPhysicalDevice(),
                                                                       Context.GetDevice(),
                                                                       Descriptors->GetLayout(),
                                                                       MudLayerGeometry->GetOutputLayout(),
                                                                       Scene.GetStaticMeshInstances().size());
            for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
            {
                MudOverlayTopPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout,
                                               Descriptors->GetLayout(),
                                               MudLayerGeometry->GetOutputLayout(),
                                               OverlaySides->GetLayout(),
                                               false,
                                               true,
                                               RenderStateTexture->GetLayout(),
                                               Mode));
                MudOverlaySidePipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildOverlayPipelineConfig(MaterialDescriptorSetLayout,
                                               Descriptors->GetLayout(),
                                               MudLayerGeometry->GetOutputLayout(),
                                               OverlaySides->GetLayout(),
                                               true,
                                               true,
                                               RenderStateTexture->GetLayout(),
                                               Mode));
            }
        }
        CreateRenderFinishedSemaphores();
        VkPhysicalDeviceProperties PhysicalDeviceProperties{};
        vkGetPhysicalDeviceProperties(Context.GetPhysicalDevice(), &PhysicalDeviceProperties);
        TimestampPeriodNanoseconds = PhysicalDeviceProperties.limits.timestampPeriod;
        std::uint32_t QueueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(Context.GetPhysicalDevice(), &QueueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> QueueFamilies(QueueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(Context.GetPhysicalDevice(), &QueueFamilyCount, QueueFamilies.data());
        const std::uint32_t GraphicsQueueFamily = Context.GetQueues().GetFamilyIndices().GraphicsFamily.value();
        if (GraphicsQueueFamily < QueueFamilies.size())
        {
            TimestampValidBits = QueueFamilies[GraphicsQueueFamily].timestampValidBits;
        }
        if (TimestampValidBits > 0)
        {
            CreateTimestampQueryPool(SurfaceStateSystem.GetSolverInstanceCount());
        }
        else
        {
            TLogger::Info("TRenderer", "Graphics queue does not support timestamp queries.");
        }
        SurfaceData.SetSimulationResolution(Scene.GetSimulationResolution());
        SurfaceTexelMeshResolution = Scene.GetSimulationResolution();
        OverlayTexelMeshResolution = Scene.GetSimulationResolution();
        TLogger::Info("TRenderer", "Static mesh pipeline ready with MTL base-color and tangent-space normal mapping.");
        TLogger::Debug("TRenderer",
                       "Depth format=" + std::to_string(static_cast<int>(DepthFormat)) +
                           ", material descriptor count=" + std::to_string(MaterialResources.size()) + ".");
        ConstructionCleanup.Release();
    }

    TRenderer::~TRenderer()
    {
        if (Context.GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Context.GetDevice());
        }
        if (BenchmarkOutput.is_open())
        {
            for (const std::string& Line : BenchmarkJsonLines)
                BenchmarkOutput << Line << '\n';
            BenchmarkOutput.flush();
        }
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            vkDestroyQueryPool(Context.GetDevice(), TimestampQueryPool, nullptr);
            TimestampQueryPool = VK_NULL_HANDLE;
        }
        DestroyRenderFinishedSemaphores();

        SurfaceDebugPipeline.reset();
        for (auto& Pipeline : SurfaceLitPipelines)
            Pipeline.reset();
        for (auto& Pipeline : BaseSurfaceLitPipelines)
            Pipeline.reset();
        RenderStateTexture.reset();
        for (auto& Pipeline : MudOverlayTopPipelines)
            Pipeline.reset();
        for (auto& Pipeline : MudOverlaySidePipelines)
            Pipeline.reset();
        TexelGeometryPipeline.reset();
        TexelGeometryPreview.reset();
        OverlaySides.reset();
        HeightFieldSmoothing.reset();
        MudLayerGeometry.reset();
        TexelInspector.reset();
        if (MaterialDescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(Context.GetDevice(), MaterialDescriptorPool, nullptr);
            MaterialDescriptorPool = VK_NULL_HANDLE;
        }
        MaterialResources.clear();
        if (MaterialDescriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(Context.GetDevice(), MaterialDescriptorSetLayout, nullptr);
            MaterialDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }

#pragma endregion
// ============================================================
// 01. Frame Rendering
// ============================================================
#pragma region 01_Frame_Rendering

    void TRenderer::RenderFrame(const TScene& SceneData,
                                TDebugUI&     DebugInterface,
                                float         DeltaTime,
                                bool          bSuspendSimulationClock)
    {
        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        using TClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](TClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };
        auto CpuStageStart = TClock::now();
        FrameContext.WaitForCurrentFrame();
        if (DebugInterface.GetViewportCount() > MaterialViewportCapacity)
        {
            if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
                throw std::runtime_error("Failed to wait for GPU before growing viewport uniforms.");
            MaterialViewportCapacity = std::max(DebugInterface.GetViewportCount(), MaterialViewportCapacity * 2U);
            if (MaterialUniformStride * (MaterialViewportCapacity - 1U) > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("Too many viewports for dynamic material uniform offsets.");
            CreateMaterialDescriptorResources();
        }
        ProfilingStats.FrameFenceWaitCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        if (TexelInspector)
            TexelInspector->CompleteFrame(FrameIndex);
        if (OverlaySides)
        {
            const auto Activity = OverlaySides->CompleteFrame(FrameIndex);
            ProfilingStats.OverlayActiveTopTriangles = Activity.Active;
            ProfilingStats.OverlayTotalTopTriangles = Activity.Total;
        }
        if (MudLayerGeometry)
        {
            const auto Activity = MudLayerGeometry->CompleteOccupancyFrame(FrameIndex);
            ProfilingStats.OverlayActiveTiles = Activity.Active;
            ProfilingStats.OverlayTotalTiles = Activity.Total;
        }
        if (TimestampQueryPool != VK_NULL_HANDLE && bTimestampQueriesSubmitted[FrameIndex])
        {
            const std::uint32_t QueryBase = FrameIndex * TimestampQueriesPerFrame;
            const std::uint32_t OverlayQueryOffset =
                FixedTimestampQueryCount + SolverTimestampStepsSubmitted[FrameIndex] * SolverTimestampGroupCount * 8U;
            const std::uint32_t QueryCount =
                OverlayQueryOffset + OverlayTimestampLayersSubmitted[FrameIndex] * OverlayTimestampQueriesPerLayer;
            std::vector<std::uint64_t> Timestamps(QueryCount, 0U);
            const VkResult             QueryResult = vkGetQueryPoolResults(Context.GetDevice(),
                                                                           TimestampQueryPool,
                                                                           QueryBase,
                                                                           QueryCount,
                                                                           Timestamps.size() * sizeof(std::uint64_t),
                                                                           Timestamps.data(),
                                                                           sizeof(std::uint64_t),
                                                                           VK_QUERY_RESULT_64_BIT);
            if (QueryResult == VK_SUCCESS && TimestampPeriodNanoseconds > 0.0F)
            {
                ProfilingStats.OverlayDisplayMode = OverlayDisplayModesSubmitted[FrameIndex];
                const auto ToMilliseconds = [this](std::uint64_t Start, std::uint64_t End)
                {
                    std::uint64_t ElapsedTicks = End - Start;
                    if (TimestampValidBits < 64U)
                    {
                        ElapsedTicks &= (std::uint64_t{1} << TimestampValidBits) - 1U;
                    }
                    return static_cast<float>(static_cast<double>(ElapsedTicks) * TimestampPeriodNanoseconds / 1.0e6);
                };
                ProfilingStats.RenderPreparationGpuMilliseconds = ToMilliseconds(Timestamps[0], Timestamps[1]);
                ProfilingStats.SceneDrawGpuMilliseconds =
                    ToMilliseconds(Timestamps[2], Timestamps[bRenderPassSubstageTimingsReliable ? 3U : 20U]);
                ProfilingStats.TexelInspectorGpuMilliseconds = ToMilliseconds(Timestamps[4], Timestamps[5]);
                ProfilingStats.BaseMeshDrawGpuMilliseconds = ToMilliseconds(Timestamps[6], Timestamps[7]);
                ProfilingStats.MudOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[8], Timestamps[9]);
                ProfilingStats.ReservedOverlayDrawGpuMilliseconds = ToMilliseconds(Timestamps[10], Timestamps[11]);
                ProfilingStats.OverlayPreparationGpuMilliseconds = ToMilliseconds(Timestamps[12], Timestamps[13]);
                ProfilingStats.SceneSetupGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[6]);
                ProfilingStats.RenderPassBeginGpuMilliseconds = ToMilliseconds(Timestamps[2], Timestamps[14]);
                ProfilingStats.RenderPassColorStageGpuMilliseconds = ToMilliseconds(Timestamps[15], Timestamps[16]);
                ProfilingStats.RenderPassDepthStageGpuMilliseconds = ToMilliseconds(Timestamps[17], Timestamps[18]);
                ProfilingStats.ViewportSetupGpuMilliseconds = ToMilliseconds(Timestamps[14], Timestamps[6]);
                ProfilingStats.UIDrawGpuMilliseconds = ToMilliseconds(Timestamps[3], Timestamps[19]);
                ProfilingStats.RenderPassEndGpuMilliseconds = ToMilliseconds(Timestamps[19], Timestamps[20]);
                ProfilingStats.SceneBetweenDrawsGpuMilliseconds =
                    ToMilliseconds(Timestamps[7], Timestamps[8]) + ToMilliseconds(Timestamps[9], Timestamps[10]);
                ProfilingStats.SceneTailGpuMilliseconds = ToMilliseconds(Timestamps[11], Timestamps[3]);
                ProfilingStats.OverlayGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayHeightGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayNormalVertexGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySmoothingGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesDispatchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayBoundarySearchGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayTopCommandGpuMilliseconds = 0.0F;
                ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds = 0.0F;
                ProfilingStats.OverlayCoverageSampleGpuMilliseconds = 0.0F;
                for (std::uint32_t Layer = 0; Layer < OverlayTimestampLayersSubmitted[FrameIndex]; ++Layer)
                {
                    const std::uint32_t LayerQuery = OverlayQueryOffset + Layer * OverlayTimestampQueriesPerLayer;
                    ProfilingStats.OverlayGeometryGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlayHeightGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery], Timestamps[LayerQuery + 12U]);
                    ProfilingStats.OverlayNormalVertexGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 12U], Timestamps[LayerQuery + 1U]);
                    ProfilingStats.OverlaySmoothingGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 2U], Timestamps[LayerQuery + 3U]);
                    ProfilingStats.OverlaySidesGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 11U]);
                    ProfilingStats.OverlaySidesPreBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 4U], Timestamps[LayerQuery + 5U]);
                    ProfilingStats.OverlayCoverageSampleGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]);
                    ProfilingStats.OverlayBoundarySearchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]);
                    ProfilingStats.OverlaySidesInterPassBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 6U], Timestamps[LayerQuery + 7U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 8U], Timestamps[LayerQuery + 9U]);
                    ProfilingStats.OverlayTopCommandGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesDispatchGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 5U], Timestamps[LayerQuery + 6U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 7U], Timestamps[LayerQuery + 8U]) +
                        ToMilliseconds(Timestamps[LayerQuery + 9U], Timestamps[LayerQuery + 10U]);
                    ProfilingStats.OverlaySidesPostBarrierGpuMilliseconds +=
                        ToMilliseconds(Timestamps[LayerQuery + 10U], Timestamps[LayerQuery + 11U]);
                }
                LastRenderGpuMilliseconds =
                    ProfilingStats.RenderPreparationGpuMilliseconds + ProfilingStats.SceneDrawGpuMilliseconds;
                if (bRenderPassSubstageTimingsReliable)
                    LastRenderGpuMilliseconds +=
                        ProfilingStats.UIDrawGpuMilliseconds + ProfilingStats.RenderPassEndGpuMilliseconds;
                LastSolverPass1GpuMilliseconds = 0.0F;
                LastSolverPass2GpuMilliseconds = 0.0F;
                ProfilingStats.AccumulationGeometryGpuMilliseconds = 0.0F;
                ProfilingStats.TransferWeightGpuMilliseconds = 0.0F;
                if (SolverTimestampStepsSubmitted[FrameIndex] > 0U)
                {
                    for (std::uint32_t Slot = 0;
                         Slot < SolverTimestampGroupCount * SolverTimestampStepsSubmitted[FrameIndex];
                         ++Slot)
                    {
                        const std::uint32_t SlotQuery = FixedTimestampQueryCount + Slot * 8U;
                        ProfilingStats.AccumulationGeometryGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery], Timestamps[SlotQuery + 1U]);
                        ProfilingStats.TransferWeightGpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 2U], Timestamps[SlotQuery + 3U]);
                        LastSolverPass1GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 4U], Timestamps[SlotQuery + 5U]);
                        LastSolverPass2GpuMilliseconds +=
                            ToMilliseconds(Timestamps[SlotQuery + 6U], Timestamps[SlotQuery + 7U]);
                    }
                }
                ProfilingStats.SolverPass1GpuMilliseconds = LastSolverPass1GpuMilliseconds;
                ProfilingStats.SolverPass2GpuMilliseconds = LastSolverPass2GpuMilliseconds;
                ProfilingStats.SolverGpuMilliseconds = LastSolverPass1GpuMilliseconds + LastSolverPass2GpuMilliseconds +
                                                       ProfilingStats.AccumulationGeometryGpuMilliseconds +
                                                       ProfilingStats.TransferWeightGpuMilliseconds;
                LastSolverGpuMilliseconds = ProfilingStats.SolverGpuMilliseconds;
                BenchmarkGpuElapsed = ToMilliseconds(Timestamps[21], Timestamps[23]);
                BenchmarkSimulationElapsed = ToMilliseconds(Timestamps[21], Timestamps[22]);
                WriteBenchmarkSample(FrameIndex);
            }
            bTimestampQueriesSubmitted[FrameIndex] = false;
            SolverTimestampStepsSubmitted[FrameIndex] = 0;
            OverlayTimestampLayersSubmitted[FrameIndex] = 0;
        }

        const bool bResetSolverState = FrameCoordinator.BeginFrame(DebugInterface, DeltaTime, bSuspendSimulationClock);
        if (bResetSolverState)
        {
            if (TexelInspector)
                TexelInspector->Invalidate();
        }
        ProfilingStats.DroppedSimulationSeconds = FrameCoordinator.GetDroppedSeconds();

        if (TargetWindow.WasFramebufferResized())
        {
            RecreateSwapchain(DebugInterface);
            return;
        }

        std::uint32_t ImageIndex = 0;
        CpuStageStart = TClock::now();
        const VkResult AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);
        ProfilingStats.AcquireCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);

        if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            TLogger::Debug("TRenderer", "TSwapchain became out of date while acquiring; recreating it.");
            RecreateSwapchain(DebugInterface);
            return;
        }
        if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire a Vulkan swapchain image.");
        }

        FrameContext.ResetCurrentFence();

        const VkCommandBuffer CommandBuffer = FrameContext.GetCurrentCommandBuffer();
        if (vkResetCommandBuffer(CommandBuffer, 0) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset Vulkan command buffer.");
        }

        FrameCoordinator.PrepareSteps(DebugInterface, bResetSolverState);
        const auto& SimulationSteps = FrameCoordinator.GetSteps();
        ProfilingStats.SimulationSteps = FrameCoordinator.GetStepCount();
        ProfilingStats.SimulatedThisFrameSeconds = 0.0F;
        for (const float StepSeconds : SimulationSteps)
            ProfilingStats.SimulatedThisFrameSeconds += StepSeconds;
        ProfilingStats.PendingSimulationSeconds = static_cast<float>(FrameCoordinator.GetPendingSeconds());
        ProfilingStats.SimulationInstances = static_cast<std::uint32_t>(SurfaceStateSystem.GetSolverInstanceCount());
        ProfilingStats.SimulationTexels = 0;
        ProfilingStats.StateChannels = 0;
        ProfilingStats.SimulationResolution = GetSimulationResolution();
        ProfilingStats.SurfaceTexelMeshResolution = SurfaceTexelMeshResolution;
        ProfilingStats.OverlayTexelMeshResolution = OverlayTexelMeshResolution;
        const auto& GPUResources = SurfaceStateSystem.GetGPUResources();
        for (std::size_t Instance = 0; Instance < GPUResources.GetSceneInstanceCount(); ++Instance)
        {
            if (GPUResources.GetInstanceDescriptors(Instance) == nullptr)
                continue;
            ProfilingStats.SimulationTexels += GPUResources.GetInstanceTexelCount(Instance);
            ProfilingStats.StateChannels = static_cast<std::uint32_t>(GPUResources.GetInstanceChannelCount(Instance));
        }
        if (SimulationSteps.empty())
            LastSolverGpuMilliseconds = LastSolverPass1GpuMilliseconds = LastSolverPass2GpuMilliseconds = 0.0F;
        CpuStageStart = TClock::now();
        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData, DebugInterface, SimulationSteps);
        OverlayDisplayModesSubmitted[FrameIndex] = OverlayDisplayMode;
        ProfilingStats.CommandRecordCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            bTimestampQueriesSubmitted[FrameIndex] = true;
            SolverTimestampStepsSubmitted[FrameIndex] = FrameCoordinator.GetStepCount();
            if (bBenchmarkCaptureEnabled)
            {
                BenchmarkSubmissions[FrameIndex] = {NextBenchmarkFrameIndex++,
                                                    FrameCoordinator.GetStepCount(),
                                                    ProfilingStats.SimulationResolution,
                                                    ProfilingStats.SurfaceTexelMeshResolution,
                                                    ProfilingStats.OverlayTexelMeshResolution,
                                                    ProfilingStats.SimulationInstances,
                                                    ProfilingStats.StateChannels,
                                                    ProfilingStats.SimulationTexels,
                                                    ProfilingStats.SimulatedThisFrameSeconds};
            }
        }

        const VkSemaphore WaitSemaphore = FrameContext.GetImageAvailableSemaphore();
        if (ImageIndex >= RenderFinishedSemaphores.size())
        {
            throw std::runtime_error("Acquired swapchain image has no render-finished semaphore.");
        }
        const VkSemaphore          SignalSemaphore = RenderFinishedSemaphores[ImageIndex];
        const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = &WaitSemaphore;
        SubmitInfo.pWaitDstStageMask = &WaitStage;
        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &CommandBuffer;
        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = &SignalSemaphore;

        CpuStageStart = TClock::now();
        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }
        ProfilingStats.QueueSubmitCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);

        const VkSwapchainKHR SwapchainHandle = SwapchainData.GetHandle();
        VkPresentInfoKHR     PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = &SignalSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &SwapchainHandle;
        PresentInfo.pImageIndices = &ImageIndex;

        CpuStageStart = TClock::now();
        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
        ProfilingStats.PresentCpuMilliseconds = ToMilliseconds(TClock::now() - CpuStageStart);
        const bool bSwapchainNeedsRecreation =
            AcquireResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
            PresentResult == VK_SUBOPTIMAL_KHR || TargetWindow.WasFramebufferResized();

        if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR &&
            PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present Vulkan swapchain image.");
        }

        if (bBenchmarkCaptureEnabled)
        {
            BenchmarkSubmissions[FrameIndex].CpuMilliseconds = {ProfilingStats.FrameFenceWaitCpuMilliseconds,
                                                                ProfilingStats.AcquireCpuMilliseconds,
                                                                ProfilingStats.CommandRecordCpuMilliseconds,
                                                                ProfilingStats.QueueSubmitCpuMilliseconds,
                                                                ProfilingStats.PresentCpuMilliseconds};
            if (!bBenchmarkMetadataWritten && NextBenchmarkFrameIndex > BenchmarkWarmupFrames)
                WriteBenchmarkMetadata(SceneData, DebugInterface);
        }
        FrameContext.AdvanceFrame();

        if (bSwapchainNeedsRecreation)
        {
            TLogger::Debug("TRenderer", "Presentation requires swapchain recreation.");
            RecreateSwapchain(DebugInterface);
        }
    }

#pragma endregion
// ============================================================
// 02. Scene Resource Replacement
// ============================================================
#pragma region 02_Scene_Resource_Replacement

    void TRenderer::ReloadSceneResources(const TScene& Scene, bool bResetStateSettings)
    {
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for GPU before reloading Scene resources.");
        }
        // Scene loading can append new MTL materials to AssetManager. Rebuild their
        // descriptors before command recording; otherwise new sections are skipped.
        if (MaterialResources.size() != Assets.GetMaterialCount())
        {
            CreateMaterialDescriptorResources();
        }
        auto&               ReusePool = SurfaceStateSystem.GetReusableBufferPool();
        const std::uint64_t ReuseCountBefore = ReusePool.GetReuseCount();
        const VkDeviceSize  ReusedBytesBefore = ReusePool.GetReusedBytes();
        auto PreviousRegistry = SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(Scene));
        std::unique_ptr<SurfaceState::TSurfaceStateSystem>     Replacement;
        std::unique_ptr<GPU::TGraphicsPipeline>                ReplacementDebugPipeline;
        std::unique_ptr<SurfaceState::TTexelInspector>         ReplacementInspector;
        std::unique_ptr<SurfaceState::TTexelGeometryPreview>   ReplacementGeometryPreview;
        std::unique_ptr<GPU::TGraphicsPipeline>                ReplacementGeometryPipeline;
        std::array<std::unique_ptr<GPU::TGraphicsPipeline>, 3> ReplacementLitPipelines;
        std::array<std::unique_ptr<GPU::TGraphicsPipeline>, 3> ReplacementBaseLitPipelines;
        std::unique_ptr<TRenderStateTexture>                   ReplacementRenderStateTexture;
        std::unique_ptr<SurfaceState::TTexelGeometryPreview>   ReplacementMudGeometry;
        std::unique_ptr<THeightFieldSmoothing>                 ReplacementHeightFieldSmoothing;
        std::unique_ptr<TAccumulationOverlaySides>             ReplacementOverlaySides;
        std::array<std::unique_ptr<GPU::TGraphicsPipeline>, 3> ReplacementMudTopPipelines;
        std::array<std::unique_ptr<GPU::TGraphicsPipeline>, 3> ReplacementMudSidePipelines;
        try
        {
            Replacement =
                std::make_unique<SurfaceState::TSurfaceStateSystem>(Context, Assets, SurfaceData, Scene, &ReusePool);
            Replacement->SetStateDrivenGeometryUpdateEnabled(DebugSolverSettings.bStateDrivenGeometryUpdateEnabled);
            Replacement->SetRawFluxCacheEnabled(SurfaceStateSystem.IsRawFluxCacheEnabled());
            Replacement->SetCoalescedRawFluxLayoutEnabled(SurfaceStateSystem.IsCoalescedRawFluxLayoutEnabled());
            Replacement->SetHalfRawFluxCacheEnabled(SurfaceStateSystem.IsHalfRawFluxCacheEnabled());
            Replacement->SetHalfDynamicWeightsEnabled(SurfaceStateSystem.IsHalfDynamicWeightsEnabled());
            Replacement->SetCompactScratchStorageEnabled(SurfaceStateSystem.IsCompactScratchStorageEnabled());
            Replacement->SetRuntimeSolverFlagsEnabled(SurfaceStateSystem.IsRuntimeSolverFlagsEnabled());
            Replacement->SetAdaptiveGeometryEnabled(SurfaceStateSystem.IsAdaptiveGeometryEnabled());
            Replacement->SetSparseSolverEnabled(SurfaceStateSystem.IsSparseSolverEnabled());
            Replacement->SetSparseAccumulationHeightEnabled(SurfaceStateSystem.IsSparseAccumulationHeightEnabled());
            Replacement->SetActiveChannelMaskEnabled(SurfaceStateSystem.IsActiveChannelMaskEnabled());
            Replacement->SetPerWorkgroupChannelMaskEnabled(SurfaceStateSystem.IsPerWorkgroupChannelMaskEnabled());
            Replacement->SetSparseSimulationGeometryEnabled(SurfaceStateSystem.IsSparseSimulationGeometryEnabled());
            for (std::size_t Index = 0; Index < DebugSolverSettings.Enabled.size(); ++Index)
            {
                Replacement->SetDebugSolverTermEnabled(static_cast<SurfaceState::TSurfaceSolverTerm>(Index),
                                                       DebugSolverSettings.Enabled[Index]);
            }
            if (!bResetStateSettings)
            {
                for (const auto& [Key, Parameters] : DebugProfileParameterOverrides)
                {
                    Replacement->SetDebugProfileParameters(Key.first, Key.second, Parameters);
                }
            }
            if (const SurfaceState::TSurfaceStateDescriptorResources* Descriptors =
                    Replacement->GetGPUResources().GetAnyInstanceDescriptors())
            {
                ReplacementDebugPipeline = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
                ReplacementInspector =
                    std::make_unique<SurfaceState::TTexelInspector>(Context.GetPhysicalDevice(),
                                                                    Context.GetDevice(),
                                                                    Descriptors->GetLayout(),
                                                                    TRenderContext::MaxFramesInFlight);
                ReplacementGeometryPreview =
                    std::make_unique<SurfaceState::TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                          Context.GetDevice(),
                                                                          Descriptors->GetLayout(),
                                                                          Scene.GetStaticMeshInstances().size());
                ReplacementGeometryPipeline = std::make_unique<GPU::TGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildTexelGeometryPipelineConfig(MaterialDescriptorSetLayout,
                                                     Descriptors->GetLayout(),
                                                     ReplacementGeometryPreview->GetOutputLayout()));
                ReplacementRenderStateTexture = std::make_unique<TRenderStateTexture>(Context.GetPhysicalDevice(),
                                                                                      Context.GetDevice(),
                                                                                      Descriptors->GetLayout(),
                                                                                      Replacement->GetGPUResources());
                for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
                {
                    ReplacementLitPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                      Descriptors->GetLayout(),
                                                      VK_NULL_HANDLE,
                                                      ReplacementRenderStateTexture->GetLayout(),
                                                      Mode));
                    ReplacementBaseLitPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                          Descriptors->GetLayout(),
                                                          ReplacementGeometryPreview->GetOutputLayout(),
                                                          ReplacementRenderStateTexture->GetLayout(),
                                                          Mode));
                }
                ReplacementMudGeometry =
                    std::make_unique<SurfaceState::TTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                          Context.GetDevice(),
                                                                          Descriptors->GetLayout(),
                                                                          Scene.GetStaticMeshInstances().size(),
                                                                          true);
                ReplacementMudGeometry->SetOccupancyTileSize(static_cast<std::uint32_t>(OverlayOccupancyTileSize));
                ReplacementMudGeometry->SetLowAmountHeightFade(DemoEffects.LowAmountHeightFade);
                ReplacementHeightFieldSmoothing =
                    std::make_unique<THeightFieldSmoothing>(Context.GetPhysicalDevice(),
                                                            Context.GetDevice(),
                                                            Descriptors->GetLayout(),
                                                            ReplacementMudGeometry->GetOutputLayout(),
                                                            Scene.GetStaticMeshInstances().size());
                ReplacementOverlaySides =
                    std::make_unique<TAccumulationOverlaySides>(Context.GetPhysicalDevice(),
                                                                Context.GetDevice(),
                                                                Descriptors->GetLayout(),
                                                                ReplacementMudGeometry->GetOutputLayout(),
                                                                Scene.GetStaticMeshInstances().size());
                for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
                {
                    ReplacementMudTopPipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildOverlayPipelineConfig(MaterialDescriptorSetLayout,
                                                   Descriptors->GetLayout(),
                                                   ReplacementMudGeometry->GetOutputLayout(),
                                                   ReplacementOverlaySides->GetLayout(),
                                                   false,
                                                   true,
                                                   ReplacementRenderStateTexture->GetLayout(),
                                                   Mode));
                    ReplacementMudSidePipelines[Mode] = std::make_unique<GPU::TGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildOverlayPipelineConfig(MaterialDescriptorSetLayout,
                                                   Descriptors->GetLayout(),
                                                   ReplacementMudGeometry->GetOutputLayout(),
                                                   ReplacementOverlaySides->GetLayout(),
                                                   true,
                                                   true,
                                                   ReplacementRenderStateTexture->GetLayout(),
                                                   Mode));
                }
            }
        }
        catch (...)
        {
            SurfaceData.ExchangeSurfaceStateRegistry(std::move(PreviousRegistry));
            throw;
        }
        SurfaceDebugPipeline.reset();
        for (auto& Pipeline : SurfaceLitPipelines)
            Pipeline.reset();
        for (auto& Pipeline : BaseSurfaceLitPipelines)
            Pipeline.reset();
        RenderStateTexture.reset();
        for (auto& Pipeline : MudOverlayTopPipelines)
            Pipeline.reset();
        for (auto& Pipeline : MudOverlaySidePipelines)
            Pipeline.reset();
        TexelGeometryPipeline.reset();
        TexelGeometryPreview.reset();
        OverlaySides.reset();
        HeightFieldSmoothing.reset();
        MudLayerGeometry.reset();
        TexelInspector.reset();
        InspectedTexel.reset();
        SurfaceStateSystem.ReplaceSceneResources(std::move(*Replacement));
        TLogger::Info("TRenderer",
                      "Surface GPU buffer reuse: " + std::to_string(ReusePool.GetReuseCount() - ReuseCountBefore) +
                          " buffer(s), " +
                          std::to_string((ReusePool.GetReusedBytes() - ReusedBytesBefore) / (1024U * 1024U)) +
                          " MiB; retained " + std::to_string(ReusePool.GetRetainedBytes() / (1024U * 1024U)) + " MiB.");
        FrameCoordinator.ResetClock();
        SurfaceDebugPipeline = std::move(ReplacementDebugPipeline);
        TexelInspector = std::move(ReplacementInspector);
        TexelGeometryPreview = std::move(ReplacementGeometryPreview);
        TexelGeometryPipeline = std::move(ReplacementGeometryPipeline);
        SurfaceLitPipelines = std::move(ReplacementLitPipelines);
        BaseSurfaceLitPipelines = std::move(ReplacementBaseLitPipelines);
        RenderStateTexture = std::move(ReplacementRenderStateTexture);
        MudLayerGeometry = std::move(ReplacementMudGeometry);
        HeightFieldSmoothing = std::move(ReplacementHeightFieldSmoothing);
        OverlaySides = std::move(ReplacementOverlaySides);
        MudOverlayTopPipelines = std::move(ReplacementMudTopPipelines);
        MudOverlaySidePipelines = std::move(ReplacementMudSidePipelines);
        SurfaceData.SetSimulationResolution(Scene.GetSimulationResolution());
        if (bResetStateSettings)
        {
            DebugProfileParameterOverrides.clear();
            DebugStateChannel = 0;
        }
        CreateTimestampQueryPool(SurfaceStateSystem.GetSolverInstanceCount());
        LastRenderGpuMilliseconds = -1.0F;
        LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = -1.0F;
        LastSolverPass2GpuMilliseconds = -1.0F;
    }

    void TRenderer::RestartSimulationState()
    {
        FrameCoordinator.RestartState();
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

#pragma endregion
// ============================================================
// 03. Resolution and Swapchain
// ============================================================
#pragma region 03_Resolution_and_Swapchain

    std::uint32_t TRenderer::GetSimulationResolution() const noexcept
    {
        return SurfaceData.GetSimulationResolution();
    }

    void TRenderer::TrimRenderResolutionCaches()
    {
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait before trimming render mesh caches.");
        if (OverlaySides) OverlaySides->ReleaseOutputs();
        if (HeightFieldSmoothing) HeightFieldSmoothing->ReleaseOutputs();
        if (MudLayerGeometry) MudLayerGeometry->ReleaseOutputs();
        if (TexelGeometryPreview) TexelGeometryPreview->ReleaseOutputs();
        SurfaceStateSystem.TrimRenderMeshVariants(SurfaceTexelMeshResolution, OverlayTexelMeshResolution);
    }

    void TRenderer::SetSurfaceTexelMeshResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceRenderMeshResolution(Resolution))
            throw std::invalid_argument("Surface Texel Mesh resolution must be 64, 128, 256 or 512.");
        if (SurfaceTexelMeshResolution == Resolution)
            return;
        SurfaceTexelMeshResolution = Resolution;
        TrimRenderResolutionCaches();
        LastRenderGpuMilliseconds = -1.0F;
    }

    void TRenderer::SetOverlayTexelMeshResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceRenderMeshResolution(Resolution))
            throw std::invalid_argument("Overlay Texel Mesh resolution must be 64, 128, 256 or 512.");
        if (OverlayTexelMeshResolution == Resolution)
            return;
        OverlayTexelMeshResolution = Resolution;
        TrimRenderResolutionCaches();
        LastRenderGpuMilliseconds = -1.0F;
    }

    void TRenderer::SetSimulationResolution(TScene& Scene, std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        if (Resolution == GetSimulationResolution())
            return;

        const std::uint32_t                                  PreviousResolution = Scene.GetSimulationResolution();
        auto&                                                Instances = Scene.GetStaticMeshInstances();
        std::vector<SurfaceState::TSurfaceRuntimeDataHandle> PreviousHandles;
        PreviousHandles.reserve(Instances.size());
        for (const auto& Instance : Instances)
            PreviousHandles.push_back(Instance.GetSurfaceData());
        auto ReplacementHandles = PreviousHandles;
        try
        {
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
            {
                if (SurfaceData.HasSurfaceData(PreviousHandles[Index]))
                    ReplacementHandles[Index] =
                        SurfaceData.LoadSurfaceDataAtResolution(PreviousHandles[Index], Resolution);
            }
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
                Instances[Index].SetSurfaceData(ReplacementHandles[Index]);
            Scene.SetSimulationResolution(Resolution);
            // The solver must keep referencing the persistent Scene, rather than a temporary copy.
            ReloadSceneResources(Scene, false);
        }
        catch (...)
        {
            Scene.SetSimulationResolution(PreviousResolution);
            for (std::size_t Index = 0; Index < Instances.size(); ++Index)
                Instances[Index].SetSurfaceData(PreviousHandles[Index]);
            SurfaceData.ReleaseUnusedSurfaceData(PreviousHandles);
            throw;
        }
        SurfaceData.ReleaseUnusedSurfaceData(ReplacementHandles);
        TLogger::Info("TRenderer",
                      "Simulation resolution changed to " + std::to_string(Resolution) + " x " +
                          std::to_string(Resolution) + "; State reset.");
    }

    void TRenderer::RecreateSwapchain(TDebugUI& DebugInterface)
    {
        TargetWindow.WaitForNonZeroFramebuffer();
        if (TargetWindow.ShouldClose())
        {
            return;
        }

        std::uint32_t FramebufferWidth = 0;
        std::uint32_t FramebufferHeight = 0;
        TargetWindow.GetFramebufferSize(FramebufferWidth, FramebufferHeight);

        TLogger::Info("TRenderer",
                      "Recreating swapchain for framebuffer " + std::to_string(FramebufferWidth) + "x" +
                          std::to_string(FramebufferHeight) + ".");

        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for Vulkan device before swapchain recreation.");
        }

        DestroyRenderFinishedSemaphores();

        // 프레임버퍼가 스왑체인 이미지 뷰와 깊이 이미지 뷰를 참조하므로,
        // 이미지 뷰를 다시 만들기 전에 프레임버퍼를 먼저 해제한다.
        MainFramebuffers.Reset();
        DepthImageViews.clear();
        DepthImages.clear();

        const VkFormat PreviousColorFormat = SwapchainData.GetImageFormat();
        SwapchainData.Recreate(Context, TargetWindow);
        CreateRenderFinishedSemaphores();

        if (PreviousColorFormat != SwapchainData.GetImageFormat())
        {
            throw std::runtime_error(
                "TSwapchain color format changed during resize. TRenderPass/Pipeline recreation is required.");
        }

        DepthImages = CreateDepthImages(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        SwapchainData.GetExtent(),
                                        DepthFormat,
                                        SwapchainData.GetImageViews().size());
        DepthImageViews = CreateDepthImageViews(Context.GetDevice(), DepthImages, DepthFormat);
        MainFramebuffers.Recreate(MainRenderPass.GetHandle(),
                                  SwapchainData.GetExtent(),
                                  SwapchainData.GetImageViews(),
                                  GetDepthImageViewHandles(DepthImageViews));

        TargetWindow.ResetFramebufferResized();
        DebugInterface.OnSwapchainRecreated(Context, *this);

        const VkExtent2D NewExtent = SwapchainData.GetExtent();
        TLogger::Info("TRenderer",
                      "TSwapchain recreation complete: " + std::to_string(NewExtent.width) + "x" +
                          std::to_string(NewExtent.height) + ".");
    }

#pragma endregion
// ============================================================
// 04. Renderer Accessors
// ============================================================
#pragma region 04_Renderer_Accessors

    const GPU::TSwapchain& TRenderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
    }

    VkRenderPass TRenderer::GetRenderPassHandle() const noexcept
    {
        return MainRenderPass.GetHandle();
    }

    const SurfaceState::TSurfaceGPUResourceManager& TRenderer::GetSurfaceGPUResources() const noexcept
    {
        return SurfaceStateSystem.GetGPUResources();
    }

    float TRenderer::GetLastSolverGpuMilliseconds() const noexcept
    {
        return LastSolverGpuMilliseconds;
    }

    float TRenderer::GetLastRenderGpuMilliseconds() const noexcept
    {
        return LastRenderGpuMilliseconds;
    }

    float TRenderer::GetLastSolverPass1GpuMilliseconds() const noexcept
    {
        return LastSolverPass1GpuMilliseconds;
    }

    float TRenderer::GetLastSolverPass2GpuMilliseconds() const noexcept
    {
        return LastSolverPass2GpuMilliseconds;
    }

#pragma endregion
} // namespace MDSS::Rendering
