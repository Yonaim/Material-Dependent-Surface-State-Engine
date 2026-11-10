/**
 * @file Renderer.cpp
 * @brief 프레임의 Solver·Surface 표시·장면·UI 명령을 기록하고 제출한다.
 */

#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
    // 교체 리소스를 준비 완료 전까지 독립 소유해 예외 시 기존 Renderer 자원을 유지한다.
    struct FRenderer::FSceneReplacementResources
    {
        std::unique_ptr<SurfaceState::FSurfaceStateSystem>     Replacement;
        std::unique_ptr<GPU::FGraphicsPipeline>                ReplacementDebugPipeline;
        std::unique_ptr<SurfaceState::FTexelInspector>         ReplacementInspector;
        std::unique_ptr<SurfaceState::FTexelGeometryPreview>   ReplacementGeometryPreview;
        std::unique_ptr<GPU::FGraphicsPipeline>                ReplacementGeometryPipeline;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> ReplacementLitPipelines;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> ReplacementBaseLitPipelines;
        std::unique_ptr<FRenderStateTexture>                   ReplacementRenderStateTexture;
        std::unique_ptr<SurfaceState::FTexelGeometryPreview>   ReplacementMudGeometry;
        std::unique_ptr<FHeightFieldSmoothing>                 ReplacementHeightFieldSmoothing;
        std::unique_ptr<FAccumulationOverlaySides>             ReplacementOverlaySides;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> ReplacementMudTopPipelines;
        std::array<std::unique_ptr<GPU::FGraphicsPipeline>, 3> ReplacementMudSidePipelines;
    };

// ============================================================
// 00. Renderer Lifecycle
// ============================================================
#pragma region 00_Renderer_Lifecycle
// Scene·device·표시 자원을 연결해 Renderer의 시작과 종료를 관리한다.

    FRenderer::FRenderer(const GPU::FVulkanContext&         Context,
                         FWindow&                           FWindow,
                         Asset::FAssetManager&              Assets,
                         SurfaceState::FSurfaceDataManager& SurfaceData,
                         const FScene&                      Scene,
                         SurfaceState::FSurfaceStateSystem& SurfaceStateSystem,
                         FFrameCoordinator&                 FrameCoordinator)
        : Context(Context), TargetWindow(FWindow), Assets(Assets), SurfaceData(SurfaceData),
          SurfaceStateSystem(SurfaceStateSystem), FrameCoordinator(FrameCoordinator), SwapchainData(Context, FWindow),
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
                // The FRenderer destructor is not called when construction fails. Release raw Vulkan
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
            (sizeof(FMaterialUniform) + UniformAlignment - 1U) / UniformAlignment * UniformAlignment;
        bRenderPassSubstageTimingsReliable =
            DriverProperties.driverID != VK_DRIVER_ID_MOLTENVK || DeviceProperties.vendorID != 0x106BU;
        bSupportsWireframeLineWidth = DeviceFeatures.wideLines == VK_TRUE;

        if (bSupportsWireframeLineWidth)
        {
            WireframeLineWidthMin = std::max(1.0F, DeviceProperties.limits.lineWidthRange[0]);
            WireframeLineWidthMax = std::max(WireframeLineWidthMin, DeviceProperties.limits.lineWidthRange[1]);
            WireframeLineWidth = std::clamp(2.0F, WireframeLineWidthMin, WireframeLineWidthMax);
        }

        std::vector<FGizmoVertex> GizmoVertices =
            BuildWorldReferenceVertices(WorldGridVertexCount, WorldAxisVertexCount);
        const std::vector<FGizmoVertex> TranslateGizmoVertices = BuildTranslateGizmoVertices();
        TranslateGizmoVertexCount = static_cast<std::uint32_t>(TranslateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), TranslateGizmoVertices.begin(), TranslateGizmoVertices.end());
        const std::vector<FGizmoVertex> RotateGizmoVertices = BuildRotateGizmoVertices();
        RotateGizmoVertexCount = static_cast<std::uint32_t>(RotateGizmoVertices.size());
        GizmoVertices.insert(GizmoVertices.end(), RotateGizmoVertices.begin(), RotateGizmoVertices.end());
        GizmoVertexBuffer = std::make_unique<GPU::FGPUBuffer>(
            Context.GetPhysicalDevice(),
            Context.GetDevice(),
            static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(FGizmoVertex)),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            GPU::EGPUBufferMemoryCategory::Renderer);
        GizmoVertexBuffer->Upload(GizmoVertices.data(),
                                  static_cast<VkDeviceSize>(GizmoVertices.size() * sizeof(FGizmoVertex)));

        CreateMaterialDescriptorResources();

        if (const SurfaceState::FSurfaceStateDescriptorResources* Descriptors =
                SurfaceStateSystem.GetGPUResources().GetAnyInstanceDescriptors())
        {
            SurfaceDebugPipeline = std::make_unique<GPU::FGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
            TexelInspector = std::make_unique<SurfaceState::FTexelInspector>(Context.GetPhysicalDevice(),
                                                                             Context.GetDevice(),
                                                                             Descriptors->GetLayout(),
                                                                             FRenderContext::MaxFramesInFlight);
            TexelGeometryPreview =
                std::make_unique<SurfaceState::FTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                      Context.GetDevice(),
                                                                      Descriptors->GetLayout(),
                                                                      Scene.GetStaticMeshInstances().size());
            TexelGeometryPipeline = std::make_unique<GPU::FGraphicsPipeline>(
                Context.GetDevice(),
                MainRenderPass.GetHandle(),
                BuildTexelGeometryPipelineConfig(
                    MaterialDescriptorSetLayout, Descriptors->GetLayout(), TexelGeometryPreview->GetOutputLayout()));
            RenderStateTexture = std::make_unique<FRenderStateTexture>(Context.GetPhysicalDevice(),
                                                                       Context.GetDevice(),
                                                                       Descriptors->GetLayout(),
                                                                       SurfaceStateSystem.GetGPUResources());
            for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
            {
                SurfaceLitPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                  Descriptors->GetLayout(),
                                                  VK_NULL_HANDLE,
                                                  RenderStateTexture->GetLayout(),
                                                  Mode));
                BaseSurfaceLitPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                      Descriptors->GetLayout(),
                                                      TexelGeometryPreview->GetOutputLayout(),
                                                      RenderStateTexture->GetLayout(),
                                                      Mode));
            }

            MudLayerGeometry =
                std::make_unique<SurfaceState::FTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                      Context.GetDevice(),
                                                                      Descriptors->GetLayout(),
                                                                      Scene.GetStaticMeshInstances().size(),
                                                                      true);
            MudLayerGeometry->SetLowAmountHeightFade(DemoEffects.LowAmountHeightFade);
            HeightFieldSmoothing = std::make_unique<FHeightFieldSmoothing>(Context.GetPhysicalDevice(),
                                                                           Context.GetDevice(),
                                                                           Descriptors->GetLayout(),
                                                                           MudLayerGeometry->GetOutputLayout(),
                                                                           Scene.GetStaticMeshInstances().size());
            OverlaySides = std::make_unique<FAccumulationOverlaySides>(Context.GetPhysicalDevice(),
                                                                       Context.GetDevice(),
                                                                       Descriptors->GetLayout(),
                                                                       MudLayerGeometry->GetOutputLayout(),
                                                                       Scene.GetStaticMeshInstances().size());
            for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
            {
                MudOverlayTopPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
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
                MudOverlaySidePipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
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
            FLogger::Info("TRenderer", "Graphics queue does not support timestamp queries.");
        }

        SurfaceData.SetSimulationResolution(Scene.GetSimulationResolution());
        SurfaceTexelMeshResolution = Scene.GetSimulationResolution();
        OverlayTexelMeshResolution = Scene.GetSimulationResolution();
        FLogger::Info("TRenderer", "Static mesh pipeline ready with MTL base-color and tangent-space normal mapping.");
        FLogger::Debug("TRenderer",
                       "Depth format=" + std::to_string(static_cast<int>(DepthFormat)) +
                           ", material descriptor count=" + std::to_string(MaterialResources.size()) + ".");
        ConstructionCleanup.Release();
    }

    FRenderer::~FRenderer()
    {
        if (Context.GetDevice() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Context.GetDevice());
        }

        if (Benchmark.BenchmarkOutput.is_open())
        {
            for (const std::string& Line : Benchmark.BenchmarkJsonLines)
                Benchmark.BenchmarkOutput << Line << '\n';
            Benchmark.BenchmarkOutput.flush();
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
// 완료 자원을 회수한 뒤 이번 frame의 compute·draw 명령을 제출한다.

    void FRenderer::RenderFrame(const FScene& SceneData,
                                FDebugUI&     DebugInterface,
                                float         DeltaTime,
                                bool          bSuspendSimulationClock)
    {
        const std::uint32_t FrameIndex = FrameContext.GetCurrentFrameIndex();
        CompletePreviousFrame(FrameIndex, DebugInterface);

        const bool bResetSolverState = FrameCoordinator.BeginFrame(DebugInterface, DeltaTime, bSuspendSimulationClock);

        if (bResetSolverState && TexelInspector)
        {
            TexelInspector->Invalidate();
        }

        ProfilingStats.DroppedSimulationSeconds = FrameCoordinator.GetDroppedSeconds();

        if (TargetWindow.WasFramebufferResized())
        {
            RecreateSwapchain(DebugInterface);
            return;
        }

        std::uint32_t ImageIndex = 0;
        VkResult AcquireResult = VK_SUCCESS;

        if (!AcquireFrameImage(DebugInterface, ImageIndex, AcquireResult))
        {
            return;
        }

        const VkCommandBuffer CommandBuffer = RecordFrameCommands(
            FrameIndex, ImageIndex, SceneData, DebugInterface, bResetSolverState);
        const VkSemaphore SignalSemaphore = SubmitFrameCommands(CommandBuffer, ImageIndex);
        const bool bRecreateSwapchain = PresentFrameImage(ImageIndex, AcquireResult, SignalSemaphore);

        FinalizeFrameBenchmark(FrameIndex, SceneData, DebugInterface);
        FrameContext.AdvanceFrame();

        if (bRecreateSwapchain)
        {
            FLogger::Debug("TRenderer", "Presentation requires swapchain recreation.");
            RecreateSwapchain(DebugInterface);
        }
    }

#pragma endregion

// ============================================================
// 01.01. Frame Execution Details
// ============================================================
#pragma region 01_01_Frame_Execution_Details
// 프레임 자원 회수부터 Vulkan 명령 기록·제출·표시까지의 세부 단계를 구현한다.

    void FRenderer::CompletePreviousFrame(std::uint32_t FrameIndex, const FDebugUI& DebugInterface)
    {
        using FClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](FClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };
        auto CpuStageStart = FClock::now();
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

        ProfilingStats.FrameFenceWaitCpuMilliseconds = ToMilliseconds(FClock::now() - CpuStageStart);

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

        CollectCompletedFrameTimings(FrameIndex);

    }

    bool FRenderer::AcquireFrameImage(FDebugUI& DebugInterface, std::uint32_t& ImageIndex, VkResult& AcquireResult)
    {
        using FClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](FClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };

        auto CpuStageStart = FClock::now();
        AcquireResult = vkAcquireNextImageKHR(Context.GetDevice(),
                                                             SwapchainData.GetHandle(),
                                                             std::numeric_limits<std::uint64_t>::max(),
                                                             FrameContext.GetImageAvailableSemaphore(),
                                                             VK_NULL_HANDLE,
                                                             &ImageIndex);
        ProfilingStats.AcquireCpuMilliseconds = ToMilliseconds(FClock::now() - CpuStageStart);

        if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        {
            FLogger::Debug("TRenderer", "TSwapchain became out of date while acquiring; recreating it.");
            RecreateSwapchain(DebugInterface);
            return false;
        }

        if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire a Vulkan swapchain image.");
        }

        return true;
    }

    VkCommandBuffer FRenderer::RecordFrameCommands(std::uint32_t FrameIndex,
                                                   std::uint32_t ImageIndex,
                                                   const FScene& SceneData,
                                                   FDebugUI& DebugInterface,
                                                   bool bResetSolverState)
    {
        using FClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](FClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };

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
        auto CpuStageStart = FClock::now();
        RecordCommandBuffer(CommandBuffer, ImageIndex, SceneData, DebugInterface, SimulationSteps);
        OverlayDisplayModesSubmitted[FrameIndex] = OverlayDisplayMode;
        ProfilingStats.CommandRecordCpuMilliseconds = ToMilliseconds(FClock::now() - CpuStageStart);

        if (TimestampQueryPool != VK_NULL_HANDLE)
        {
            bTimestampQueriesSubmitted[FrameIndex] = true;
            SolverTimestampStepsSubmitted[FrameIndex] = FrameCoordinator.GetStepCount();

            if (Benchmark.bBenchmarkCaptureEnabled)
            {
                Benchmark.BenchmarkSubmissions[FrameIndex] = {Benchmark.NextBenchmarkFrameIndex++,
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

        return CommandBuffer;
    }

    VkSemaphore FRenderer::SubmitFrameCommands(VkCommandBuffer CommandBuffer, std::uint32_t ImageIndex)
    {
        using FClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](FClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };

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

        const auto CpuStageStart = FClock::now();

        if (vkQueueSubmit(Context.GetQueues().GetGraphics(), 1, &SubmitInfo, FrameContext.GetInFlightFence()) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit Vulkan draw command buffer.");
        }

        ProfilingStats.QueueSubmitCpuMilliseconds = ToMilliseconds(FClock::now() - CpuStageStart);

        return SignalSemaphore;
    }

    bool FRenderer::PresentFrameImage(std::uint32_t ImageIndex, VkResult AcquireResult, VkSemaphore SignalSemaphore)
    {
        using FClock = std::chrono::steady_clock;
        const auto ToMilliseconds = [](FClock::duration Duration)
        { return std::chrono::duration<float, std::milli>(Duration).count(); };

        const VkSwapchainKHR SwapchainHandle = SwapchainData.GetHandle();
        VkPresentInfoKHR     PresentInfo{};
        PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        PresentInfo.waitSemaphoreCount = 1;
        PresentInfo.pWaitSemaphores = &SignalSemaphore;
        PresentInfo.swapchainCount = 1;
        PresentInfo.pSwapchains = &SwapchainHandle;
        PresentInfo.pImageIndices = &ImageIndex;

        const auto CpuStageStart = FClock::now();
        const VkResult PresentResult = vkQueuePresentKHR(Context.GetQueues().GetPresent(), &PresentInfo);
        ProfilingStats.PresentCpuMilliseconds = ToMilliseconds(FClock::now() - CpuStageStart);
        const bool bSwapchainNeedsRecreation =
            AcquireResult == VK_SUBOPTIMAL_KHR || PresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
            PresentResult == VK_SUBOPTIMAL_KHR || TargetWindow.WasFramebufferResized();

        if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR &&
            PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
        {
            throw std::runtime_error("Failed to present Vulkan swapchain image.");
        }

        return bSwapchainNeedsRecreation;
    }

    void FRenderer::FinalizeFrameBenchmark(std::uint32_t FrameIndex,
                                           const FScene& SceneData,
                                           const FDebugUI& DebugInterface)
    {
        if (Benchmark.bBenchmarkCaptureEnabled)
        {
            Benchmark.BenchmarkSubmissions[FrameIndex].CpuMilliseconds = {ProfilingStats.FrameFenceWaitCpuMilliseconds,
                                                                          ProfilingStats.AcquireCpuMilliseconds,
                                                                          ProfilingStats.CommandRecordCpuMilliseconds,
                                                                          ProfilingStats.QueueSubmitCpuMilliseconds,
                                                                          ProfilingStats.PresentCpuMilliseconds};
            if (!Benchmark.bBenchmarkMetadataWritten &&
                Benchmark.NextBenchmarkFrameIndex > Benchmark.BenchmarkWarmupFrames)
                WriteBenchmarkMetadata(SceneData, DebugInterface);
            if (Benchmark.AuthoringScreenshot)
                WriteAuthoringCheckpoint(SceneData);
        }

    }

#pragma endregion

// ============================================================
// 02. Scene Resource Replacement
// ============================================================
#pragma region 02_Scene_Resource_Replacement
// 이전 GPU 사용이 끝난 뒤 새 Scene에 대응하는 표시 자원을 구성한다.

    void FRenderer::ReloadSceneResources(const FScene& Scene, bool bResetStateSettings)
    {
        // GPU 정지 → 새 리소스 준비 → Registry 복구 가능 구간 → 리소스 교체 순서로 수행한다.
        WaitForSceneReloadAndUpdateMaterials();

        auto&               ReusePool = SurfaceStateSystem.GetReusableBufferPool();
        const std::uint64_t ReuseCountBefore = ReusePool.GetReuseCount();
        const VkDeviceSize  ReusedBytesBefore = ReusePool.GetReusedBytes();
        auto PreviousRegistry = SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(Scene));
        FSceneReplacementResources Resources;
        try
        {
            BuildSceneReplacement(Scene, bResetStateSettings, Resources);
        }
        catch (...)
        {
            SurfaceData.ExchangeSurfaceStateRegistry(std::move(PreviousRegistry));
            throw;
        }

        ReleaseSceneRenderObjects();
        SurfaceStateSystem.ReplaceSceneResources(std::move(*Resources.Replacement));
        FLogger::Info("TRenderer",
                      "Surface GPU buffer reuse: " + std::to_string(ReusePool.GetReuseCount() - ReuseCountBefore) +
                          " buffer(s), " +
                          std::to_string((ReusePool.GetReusedBytes() - ReusedBytesBefore) / (1024U * 1024U)) +
                          " MiB; retained " + std::to_string(ReusePool.GetRetainedBytes() / (1024U * 1024U)) + " MiB.");
        FrameCoordinator.ResetClock();
        CommitSceneReplacement(Resources);
        SurfaceData.SetSimulationResolution(Scene.GetSimulationResolution());

        if (bResetStateSettings)
        {
            DebugProfileParameterOverrides.clear();
            DebugStateChannel = 0;
        }

        ResetSceneGpuProfiling();
    }

    void FRenderer::BuildSceneReplacement(const FScene& Scene,
                                          bool bResetStateSettings,
                                          FSceneReplacementResources& Resources)
    {
        // 기존 설정을 복제한 Solver와 종속 renderer 자원을 먼저 생성한다.
        auto& ReusePool = SurfaceStateSystem.GetReusableBufferPool();
        auto& Replacement = Resources.Replacement;
        Replacement =
                std::make_unique<SurfaceState::FSurfaceStateSystem>(Context, Assets, SurfaceData, Scene, &ReusePool);
        ConfigureReplacementSolver(*Replacement, bResetStateSettings);
        BuildSceneRenderPipelines(Scene, Resources);
    }

    void FRenderer::BuildSceneRenderPipelines(const FScene& Scene,
                                              FSceneReplacementResources& Resources)
    {
        // Surface State descriptors에 의존하는 debug·lighting·overlay pipelines를 생성한다.
        auto& Replacement = Resources.Replacement;
        auto& ReplacementDebugPipeline = Resources.ReplacementDebugPipeline;
        auto& ReplacementInspector = Resources.ReplacementInspector;
        auto& ReplacementGeometryPreview = Resources.ReplacementGeometryPreview;
        auto& ReplacementGeometryPipeline = Resources.ReplacementGeometryPipeline;
        auto& ReplacementLitPipelines = Resources.ReplacementLitPipelines;
        auto& ReplacementBaseLitPipelines = Resources.ReplacementBaseLitPipelines;
        auto& ReplacementRenderStateTexture = Resources.ReplacementRenderStateTexture;
        auto& ReplacementMudGeometry = Resources.ReplacementMudGeometry;
        auto& ReplacementHeightFieldSmoothing = Resources.ReplacementHeightFieldSmoothing;
        auto& ReplacementOverlaySides = Resources.ReplacementOverlaySides;
        auto& ReplacementMudTopPipelines = Resources.ReplacementMudTopPipelines;
        auto& ReplacementMudSidePipelines = Resources.ReplacementMudSidePipelines;
            if (const SurfaceState::FSurfaceStateDescriptorResources* Descriptors =
                    Replacement->GetGPUResources().GetAnyInstanceDescriptors())
            {
                ReplacementDebugPipeline = std::make_unique<GPU::FGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildSurfaceDebugPipelineConfig(MaterialDescriptorSetLayout, Descriptors->GetLayout()));
                ReplacementInspector =
                    std::make_unique<SurfaceState::FTexelInspector>(Context.GetPhysicalDevice(),
                                                                    Context.GetDevice(),
                                                                    Descriptors->GetLayout(),
                                                                    FRenderContext::MaxFramesInFlight);
                ReplacementGeometryPreview =
                    std::make_unique<SurfaceState::FTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                          Context.GetDevice(),
                                                                          Descriptors->GetLayout(),
                                                                          Scene.GetStaticMeshInstances().size());
                ReplacementGeometryPipeline = std::make_unique<GPU::FGraphicsPipeline>(
                    Context.GetDevice(),
                    MainRenderPass.GetHandle(),
                    BuildTexelGeometryPipelineConfig(MaterialDescriptorSetLayout,
                                                     Descriptors->GetLayout(),
                                                     ReplacementGeometryPreview->GetOutputLayout()));
                ReplacementRenderStateTexture = std::make_unique<FRenderStateTexture>(Context.GetPhysicalDevice(),
                                                                                      Context.GetDevice(),
                                                                                      Descriptors->GetLayout(),
                                                                                      Replacement->GetGPUResources());
                for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
                {
                    ReplacementLitPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                      Descriptors->GetLayout(),
                                                      VK_NULL_HANDLE,
                                                      ReplacementRenderStateTexture->GetLayout(),
                                                      Mode));
                    ReplacementBaseLitPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
                        Context.GetDevice(),
                        MainRenderPass.GetHandle(),
                        BuildBaseSurfaceLitPipelineConfig(MaterialDescriptorSetLayout,
                                                          Descriptors->GetLayout(),
                                                          ReplacementGeometryPreview->GetOutputLayout(),
                                                          ReplacementRenderStateTexture->GetLayout(),
                                                          Mode));
                }

                ReplacementMudGeometry =
                    std::make_unique<SurfaceState::FTexelGeometryPreview>(Context.GetPhysicalDevice(),
                                                                          Context.GetDevice(),
                                                                          Descriptors->GetLayout(),
                                                                          Scene.GetStaticMeshInstances().size(),
                                                                          true);
                ReplacementMudGeometry->SetOccupancyTileSize(static_cast<std::uint32_t>(OverlayOccupancyTileSize));
                ReplacementMudGeometry->SetLowAmountHeightFade(DemoEffects.LowAmountHeightFade);
                ReplacementHeightFieldSmoothing =
                    std::make_unique<FHeightFieldSmoothing>(Context.GetPhysicalDevice(),
                                                            Context.GetDevice(),
                                                            Descriptors->GetLayout(),
                                                            ReplacementMudGeometry->GetOutputLayout(),
                                                            Scene.GetStaticMeshInstances().size());
                ReplacementOverlaySides =
                    std::make_unique<FAccumulationOverlaySides>(Context.GetPhysicalDevice(),
                                                                Context.GetDevice(),
                                                                Descriptors->GetLayout(),
                                                                ReplacementMudGeometry->GetOutputLayout(),
                                                                Scene.GetStaticMeshInstances().size());
                for (std::uint32_t Mode = 0; Mode < 3; ++Mode)
                {
                    ReplacementMudTopPipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
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
                    ReplacementMudSidePipelines[Mode] = std::make_unique<GPU::FGraphicsPipeline>(
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

    void FRenderer::CommitSceneReplacement(FSceneReplacementResources& Resources)
    {
        // 준비가 끝난 GPU 객체 소유권을 Renderer로 이전한다.
        auto& ReplacementDebugPipeline = Resources.ReplacementDebugPipeline;
        auto& ReplacementInspector = Resources.ReplacementInspector;
        auto& ReplacementGeometryPreview = Resources.ReplacementGeometryPreview;
        auto& ReplacementGeometryPipeline = Resources.ReplacementGeometryPipeline;
        auto& ReplacementLitPipelines = Resources.ReplacementLitPipelines;
        auto& ReplacementBaseLitPipelines = Resources.ReplacementBaseLitPipelines;
        auto& ReplacementRenderStateTexture = Resources.ReplacementRenderStateTexture;
        auto& ReplacementMudGeometry = Resources.ReplacementMudGeometry;
        auto& ReplacementHeightFieldSmoothing = Resources.ReplacementHeightFieldSmoothing;
        auto& ReplacementOverlaySides = Resources.ReplacementOverlaySides;
        auto& ReplacementMudTopPipelines = Resources.ReplacementMudTopPipelines;
        auto& ReplacementMudSidePipelines = Resources.ReplacementMudSidePipelines;
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
    }

    void FRenderer::WaitForSceneReloadAndUpdateMaterials()
    {
        // 기존 GPU 명령이 끝난 뒤 새 MTL material을 포함하도록 descriptor 리소스를 갱신한다.
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

    }

    void FRenderer::ConfigureReplacementSolver(SurfaceState::FSurfaceStateSystem& Replacement,
                                               bool bResetStateSettings)
    {
        // 이전 Solver 옵션과 필요한 런타임 override만 새 Solver로 이관한다.
            Replacement.SetStateDrivenGeometryUpdateEnabled(DebugSolverSettings.bStateDrivenGeometryUpdateEnabled);
            Replacement.SetRawFluxCacheEnabled(SurfaceStateSystem.IsRawFluxCacheEnabled());
            Replacement.SetCoalescedRawFluxLayoutEnabled(SurfaceStateSystem.IsCoalescedRawFluxLayoutEnabled());
            Replacement.SetHalfRawFluxCacheEnabled(SurfaceStateSystem.IsHalfRawFluxCacheEnabled());
            Replacement.SetHalfDynamicWeightsEnabled(SurfaceStateSystem.IsHalfDynamicWeightsEnabled());
            Replacement.SetCompactScratchStorageEnabled(SurfaceStateSystem.IsCompactScratchStorageEnabled());
            Replacement.SetRuntimeSolverFlagsEnabled(SurfaceStateSystem.IsRuntimeSolverFlagsEnabled());
            Replacement.SetAdaptiveGeometryEnabled(SurfaceStateSystem.IsAdaptiveGeometryEnabled());
            Replacement.SetSparseSolverEnabled(SurfaceStateSystem.IsSparseSolverEnabled());
            Replacement.SetSparseAccumulationHeightEnabled(SurfaceStateSystem.IsSparseAccumulationHeightEnabled());
            Replacement.SetActiveChannelMaskEnabled(SurfaceStateSystem.IsActiveChannelMaskEnabled());
            Replacement.SetPerWorkgroupChannelMaskEnabled(SurfaceStateSystem.IsPerWorkgroupChannelMaskEnabled());
            Replacement.SetSparseSimulationGeometryEnabled(SurfaceStateSystem.IsSparseSimulationGeometryEnabled());

            for (std::size_t Index = 0; Index < DebugSolverSettings.Enabled.size(); ++Index)
            {
                Replacement.SetDebugSolverTermEnabled(static_cast<SurfaceState::ESurfaceSolverTerm>(Index),
                                                       DebugSolverSettings.Enabled[Index]);
            }

            if (!bResetStateSettings)
            {
                for (const auto& [Key, Parameters] : DebugProfileParameterOverrides)
                {
                    Replacement.SetDebugProfileParameters(Key.first, Key.second, Parameters);
                }
            }

    }

    void FRenderer::ReleaseSceneRenderObjects()
    {
        // 이전 파이프라인과 연결 자원을 참조 순서에 맞게 해제한다.
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
    }

    void FRenderer::ResetSceneGpuProfiling()
    {
        // Scene 교체 뒤 타임스탬프 쿼리와 누적 계측값을 새로 시작한다.
        CreateTimestampQueryPool(SurfaceStateSystem.GetSolverInstanceCount());
        LastRenderGpuMilliseconds = -1.0F;
        LastSolverGpuMilliseconds = -1.0F;
        LastSolverPass1GpuMilliseconds = -1.0F;
        LastSolverPass2GpuMilliseconds = -1.0F;
    }

    void FRenderer::RestartSimulationState()
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
// 해상도·창 크기 변경 시 관련 attachment와 렌더 자원을 재구성한다.

    std::uint32_t FRenderer::GetSimulationResolution() const noexcept
    {
        return SurfaceData.GetSimulationResolution();
    }

    void FRenderer::TrimRenderResolutionCaches()
    {
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait before trimming render mesh caches.");
        if (OverlaySides)
            OverlaySides->ReleaseOutputs();
        if (HeightFieldSmoothing)
            HeightFieldSmoothing->ReleaseOutputs();
        if (MudLayerGeometry)
            MudLayerGeometry->ReleaseOutputs();
        if (TexelGeometryPreview)
            TexelGeometryPreview->ReleaseOutputs();
        SurfaceStateSystem.TrimRenderMeshVariants(SurfaceTexelMeshResolution, OverlayTexelMeshResolution);
    }

    void FRenderer::SetSurfaceTexelMeshResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceRenderMeshResolution(Resolution))
            throw std::invalid_argument("Surface Texel Mesh resolution must be 64, 128, 256 or 512.");
        if (SurfaceTexelMeshResolution == Resolution)
            return;
        SurfaceTexelMeshResolution = Resolution;
        TrimRenderResolutionCaches();
        LastRenderGpuMilliseconds = -1.0F;
    }

    void FRenderer::SetOverlayTexelMeshResolution(std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceRenderMeshResolution(Resolution))
            throw std::invalid_argument("Overlay Texel Mesh resolution must be 64, 128, 256 or 512.");
        if (OverlayTexelMeshResolution == Resolution)
            return;
        OverlayTexelMeshResolution = Resolution;
        TrimRenderResolutionCaches();
        LastRenderGpuMilliseconds = -1.0F;
    }

    void FRenderer::SetSimulationResolution(FScene& Scene, std::uint32_t Resolution)
    {
        if (!SurfaceState::IsSurfaceSimulationResolution(Resolution))
            throw std::invalid_argument("Simulation resolution must be 128, 256 or 512.");
        if (Resolution == GetSimulationResolution())
            return;

        const std::uint32_t                                  PreviousResolution = Scene.GetSimulationResolution();
        auto&                                                Instances = Scene.GetStaticMeshInstances();
        std::vector<SurfaceState::FSurfaceRuntimeDataHandle> PreviousHandles;
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
        FLogger::Info("TRenderer",
                      "Simulation resolution changed to " + std::to_string(Resolution) + " x " +
                          std::to_string(Resolution) + "; State reset.");
    }

    void FRenderer::RecreateSwapchain(FDebugUI& DebugInterface)
    {
        TargetWindow.WaitForNonZeroFramebuffer();

        if (TargetWindow.ShouldClose())
        {
            return;
        }

        std::uint32_t FramebufferWidth = 0;
        std::uint32_t FramebufferHeight = 0;
        TargetWindow.GetFramebufferSize(FramebufferWidth, FramebufferHeight);

        FLogger::Info("TRenderer",
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
        FLogger::Info("TRenderer",
                      "TSwapchain recreation complete: " + std::to_string(NewExtent.width) + "x" +
                          std::to_string(NewExtent.height) + ".");
    }

#pragma endregion
// ============================================================
// 04. Renderer Accessors
// ============================================================
#pragma region 04_Renderer_Accessors
// 현재 Scene·표시·진단 소비자가 사용할 Renderer 상태를 제공한다.

    const GPU::FSwapchain& FRenderer::GetSwapchain() const noexcept
    {
        return SwapchainData;
    }

    VkRenderPass FRenderer::GetRenderPassHandle() const noexcept
    {
        return MainRenderPass.GetHandle();
    }

    const SurfaceState::FSurfaceGPUResourceManager& FRenderer::GetSurfaceGPUResources() const noexcept
    {
        return SurfaceStateSystem.GetGPUResources();
    }

    float FRenderer::GetLastSolverGpuMilliseconds() const noexcept
    {
        return LastSolverGpuMilliseconds;
    }

    float FRenderer::GetLastRenderGpuMilliseconds() const noexcept
    {
        return LastRenderGpuMilliseconds;
    }

    float FRenderer::GetLastSolverPass1GpuMilliseconds() const noexcept
    {
        return LastSolverPass1GpuMilliseconds;
    }

    float FRenderer::GetLastSolverPass2GpuMilliseconds() const noexcept
    {
        return LastSolverPass2GpuMilliseconds;
    }

#pragma endregion
} // namespace MDSS::Rendering
