#include "Application/PerformanceSettings.h"
#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
// ============================================================
// 00. Runtime Settings and Benchmark Capture
// ============================================================
#pragma region 00_Runtime_Settings_and_Benchmark_Capture

    void TRenderer::SetDebugProfileParameters(Asset::TSRProfileAssetHandle                 Profile,
                                              SurfaceState::TStateId                       State,
                                              const SurfaceState::TSurfaceStateParameters& Parameters,
                                              bool                                         bKeepRuntimeOverride)
    {
        SurfaceStateSystem.SetDebugProfileParameters(Profile, State, Parameters, bKeepRuntimeOverride);
        if (TexelInspector)
            TexelInspector->Invalidate();
        const auto Key = std::make_pair(Profile, State);
        if (bKeepRuntimeOverride)
        {
            DebugProfileParameterOverrides[Key] = Parameters;
        }
        else
        {
            DebugProfileParameterOverrides.erase(Key);
        }
    }

    void TRenderer::ConfigureBenchmarkCapture(const std::filesystem::path& OutputPath,
                                              std::uint32_t                WarmupFrames,
                                              std::uint32_t                MeasurementFrames)
    {
        if (TimestampQueryPool == VK_NULL_HANDLE)
            throw std::runtime_error("Benchmark capture requires Vulkan timestamp query support.");
        if (OutputPath.empty() || MeasurementFrames == 0)
            throw std::invalid_argument("Benchmark capture requires an output path and measurement frames.");
        const std::filesystem::path AbsoluteOutput = std::filesystem::absolute(OutputPath).lexically_normal();
        if (AbsoluteOutput.has_parent_path())
            std::filesystem::create_directories(AbsoluteOutput.parent_path());
        BenchmarkOutput.open(AbsoluteOutput, std::ios::out | std::ios::trunc);
        if (!BenchmarkOutput)
            throw std::runtime_error("Unable to open benchmark output: " + AbsoluteOutput.string());
        BenchmarkWarmupFrames = WarmupFrames;
        BenchmarkOutputPath = AbsoluteOutput;
        bBenchmarkMetadataWritten = false;
        BenchmarkMeasurementFrames = MeasurementFrames;
        NextBenchmarkFrameIndex = 0;
        BenchmarkJsonLines.clear();
        bBenchmarkCaptureEnabled = true;
    }

    void TRenderer::WriteBenchmarkSample(std::uint32_t FrameSlot)
    {
        if (!bBenchmarkCaptureEnabled || FrameSlot >= BenchmarkSubmissions.size())
            return;
        const TBenchmarkFrameSubmission& Submitted = BenchmarkSubmissions[FrameSlot];
        if (Submitted.FrameIndex == std::numeric_limits<std::uint64_t>::max() ||
            Submitted.FrameIndex < BenchmarkWarmupFrames ||
            Submitted.FrameIndex >= BenchmarkWarmupFrames + BenchmarkMeasurementFrames)
            return;

        const auto OptionalTiming = [](float Value) -> nlohmann::json
        { return Value >= 0.0F && std::isfinite(Value) ? nlohmann::json(Value) : nlohmann::json(nullptr); };
        nlohmann::json Sample = {
            {"schema_version", 2},
            {"frame_index", Submitted.FrameIndex},
            {"resolution", Submitted.Resolution},
            {"surface_mesh_resolution", Submitted.SurfaceMeshResolution},
            {"overlay_mesh_resolution", Submitted.OverlayMeshResolution},
            {"simulation_steps", Submitted.SimulationSteps},
            {"simulated_seconds", Submitted.SimulatedSeconds},
            {"instances", Submitted.Instances},
            {"texels", Submitted.Texels},
            {"state_channels", Submitted.StateChannels},
            {"solver_gpu_ms", OptionalTiming(ProfilingStats.SolverGpuMilliseconds)},
            {"pass1_gpu_ms", OptionalTiming(ProfilingStats.SolverPass1GpuMilliseconds)},
            {"pass2_gpu_ms", OptionalTiming(ProfilingStats.SolverPass2GpuMilliseconds)},
            {"geometry_gpu_ms", OptionalTiming(ProfilingStats.AccumulationGeometryGpuMilliseconds)},
            {"transfer_weight_gpu_ms", OptionalTiming(ProfilingStats.TransferWeightGpuMilliseconds)},
            {"render_gpu_ms", OptionalTiming(LastRenderGpuMilliseconds)},
            {"frame_gpu_elapsed_ms", OptionalTiming(BenchmarkGpuElapsed)},
            {"simulation_gpu_elapsed_ms", OptionalTiming(BenchmarkSimulationElapsed)},
        };
        const std::array<std::pair<const char*, float>, 18> DetailedTimings{
            {{"render_preparation_gpu_ms", ProfilingStats.RenderPreparationGpuMilliseconds},
             {"scene_draw_gpu_ms", ProfilingStats.SceneDrawGpuMilliseconds},
             {"base_draw_gpu_ms", ProfilingStats.BaseMeshDrawGpuMilliseconds},
             {"overlay_draw_gpu_ms", ProfilingStats.MudOverlayDrawGpuMilliseconds},
             {"reserved_overlay_draw_gpu_ms", ProfilingStats.ReservedOverlayDrawGpuMilliseconds},
             {"overlay_preparation_gpu_ms", ProfilingStats.OverlayPreparationGpuMilliseconds},
             {"overlay_geometry_gpu_ms", ProfilingStats.OverlayGeometryGpuMilliseconds},
             {"overlay_height_gpu_ms", ProfilingStats.OverlayHeightGpuMilliseconds},
             {"overlay_normal_gpu_ms", ProfilingStats.OverlayNormalVertexGpuMilliseconds},
             {"overlay_smoothing_gpu_ms", ProfilingStats.OverlaySmoothingGpuMilliseconds},
             {"overlay_sides_gpu_ms", ProfilingStats.OverlaySidesGpuMilliseconds},
             {"overlay_coverage_gpu_ms", ProfilingStats.OverlayCoverageSampleGpuMilliseconds},
             {"ui_gpu_ms", ProfilingStats.UIDrawGpuMilliseconds},
             {"fence_wait_cpu_ms", Submitted.CpuMilliseconds[0]},
             {"acquire_cpu_ms", Submitted.CpuMilliseconds[1]},
             {"command_record_cpu_ms", Submitted.CpuMilliseconds[2]},
             {"queue_submit_cpu_ms", Submitted.CpuMilliseconds[3]},
             {"present_cpu_ms", Submitted.CpuMilliseconds[4]}}};
        for (const auto& [Name, Value] : DetailedTimings)
            Sample[Name] = OptionalTiming(Value);
        Sample["overlay_active_tiles"] = ProfilingStats.OverlayActiveTiles;
        Sample["overlay_total_tiles"] = ProfilingStats.OverlayTotalTiles;
        Sample["overlay_active_top_triangles"] = ProfilingStats.OverlayActiveTopTriangles;
        Sample["overlay_total_top_triangles"] = ProfilingStats.OverlayTotalTopTriangles;
        BenchmarkJsonLines.push_back(Sample.dump());
    }

    void TRenderer::WriteBenchmarkMetadata(const TScene& SceneData, const TDebugUI& DebugInterface)
    {
        const VkExtent2D Extent = SwapchainData.GetExtent();
        nlohmann::json   Viewports = nlohmann::json::array();
        for (std::size_t Index = 0; Index < DebugInterface.GetViewportCount(); ++Index)
        {
            const auto  Rect = DebugInterface.GetViewportRectNormalized(Index);
            const auto& Camera = DebugInterface.GetViewportCamera(SceneData, Index);
            const auto  Position = Camera.GetPosition();
            const auto  Target = Camera.GetTarget();
            Viewports.push_back({{"normalized_rect", {Rect.x, Rect.y, Rect.z, Rect.w}},
                                 {"camera_position", {Position.x, Position.y, Position.z}},
                                 {"camera_target", {Target.x, Target.y, Target.z}},
                                 {"fov_degrees", Camera.GetVerticalFieldOfViewDegrees()},
                                 {"view_mode", static_cast<std::uint32_t>(ViewModes[Index])}});
        }
        const nlohmann::json Metadata = {{"schema_version", 1},
                                         {"frame_index", NextBenchmarkFrameIndex - 1},
                                         {"swapchain_extent", {Extent.width, Extent.height}},
                                         {"viewports", Viewports},
                                         {"geometry_feedback", IsStateDrivenGeometryUpdateEnabled()},
                                         {"raw_flux_cache", IsRawFluxCacheEnabled()},
                                         {"coalesced_raw_flux", IsCoalescedRawFluxLayoutEnabled()},
                                         {"fp16_raw_flux", IsHalfRawFluxCacheEnabled()},
                                         {"fp16_weights", IsHalfDynamicWeightsEnabled()},
                                         {"sparse_solver", IsSparseSolverEnabled()},
                                         {"sparse_height", IsSparseAccumulationHeightEnabled()},
                                         {"active_channel_mask", IsActiveChannelMaskEnabled()},
                                         {"per_workgroup_channel_mask", IsPerWorkgroupChannelMaskEnabled()},
                                         {"sparse_geometry", IsSparseSimulationGeometryEnabled()},
                                         {"sparse_height_smoothing", IsSparseHeightSmoothingEnabled()},
                                         {"shared_height_smoothing", IsSharedHeightSmoothingEnabled()},
                                         {"shared_coverage_smoothing", IsSharedOverlayCoverageEnabled()},
                                         {"shared_state_texture_smoothing", IsSharedStateTextureSmoothingEnabled()},
                                         {"precompute_coverage", IsPrecomputeCoverageSmoothingEnabled()},
                                         {"render_state_texture", IsRenderStateTextureSamplingEnabled()},
                                         {"separable_coverage", IsSeparableCoverageSmoothingEnabled()},
                                         {"overlay_tile_culling", IsOverlayTileCullingEnabled()},
                                         {"overlay_tile_size", static_cast<std::uint32_t>(OverlayOccupancyTileSize)},
                                         {"base_draw", IsBaseMeshDrawEnabled()},
                                         {"overlay_top_draw", IsOverlayTopDrawEnabled()},
                                         {"overlay_sides_draw", IsOverlaySidesDrawEnabled()},
                                         {"surface_mesh_resolution", SurfaceTexelMeshResolution},
                                         {"overlay_mesh_resolution", OverlayTexelMeshResolution}};
        std::ofstream        Output(BenchmarkOutputPath.string() + ".settings.json");
        if (!(Output << Metadata.dump(2) << '\n'))
            throw std::runtime_error("Unable to write benchmark settings.");
        bBenchmarkMetadataWritten = true;
    }

    void TRenderer::RecordBenchmarkScreenshot(VkCommandBuffer Command, std::uint32_t ImageIndex)
    {
        if (!PerformanceSettings().value("capture_image", false) || BenchmarkScreenshot ||
            NextBenchmarkFrameIndex != BenchmarkWarmupFrames + BenchmarkMeasurementFrames - 1U)
            return;
        if (!SwapchainData.CanReadback())
            throw std::runtime_error("This swapchain cannot capture images.");
        BenchmarkScreenshotExtent = SwapchainData.GetExtent();
        BenchmarkScreenshotFormat = SwapchainData.GetImageFormat();
        BenchmarkScreenshot = std::make_unique<GPU::TGPUBuffer>(
            Context.GetPhysicalDevice(),
            Context.GetDevice(),
            VkDeviceSize(BenchmarkScreenshotExtent.width) * BenchmarkScreenshotExtent.height * 4U,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkImageMemoryBarrier Barrier{};
        Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        Barrier.srcAccessMask = 0; // PRESENT has no associated access type.
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.srcQueueFamilyIndex = Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Barrier.image = SwapchainData.GetImages()[ImageIndex];
        Barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkMemoryBarrier ColorVisibility{};
        ColorVisibility.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        ColorVisibility.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        ColorVisibility.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             1,
                             &ColorVisibility,
                             0,
                             nullptr,
                             1,
                             &Barrier);
        VkBufferImageCopy Copy{};
        Copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        Copy.imageExtent = {BenchmarkScreenshotExtent.width, BenchmarkScreenshotExtent.height, 1};
        vkCmdCopyImageToBuffer(
            Command, Barrier.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, BenchmarkScreenshot->GetHandle(), 1, &Copy);
        VkBufferMemoryBarrier Readback{};
        Readback.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        Readback.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Readback.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        Readback.srcQueueFamilyIndex = Readback.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        Readback.buffer = BenchmarkScreenshot->GetHandle();
        Readback.size = VK_WHOLE_SIZE;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_HOST_BIT,
                             0,
                             0,
                             nullptr,
                             1,
                             &Readback,
                             0,
                             nullptr);
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Barrier.dstAccessMask = 0;
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(Command,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &Barrier);
    }

    void TRenderer::FinalizeBenchmarkCapture()
    {
        if (!bBenchmarkCaptureEnabled)
            return;
        if (BenchmarkScreenshot)
        {
            std::vector<std::uint8_t> Pixels(static_cast<std::size_t>(BenchmarkScreenshot->GetSize()));
            BenchmarkScreenshot->Download(Pixels.data(), BenchmarkScreenshot->GetSize());
            const bool    BGRA = BenchmarkScreenshotFormat == VK_FORMAT_B8G8R8A8_SRGB ||
                                 BenchmarkScreenshotFormat == VK_FORMAT_B8G8R8A8_UNORM;
            std::ofstream Image(BenchmarkOutputPath.string() + ".ppm", std::ios::binary);
            Image << "P6\n" << BenchmarkScreenshotExtent.width << ' ' << BenchmarkScreenshotExtent.height << "\n255\n";
            for (std::size_t P = 0; P < Pixels.size(); P += 4)
            {
                Image.put(char(Pixels[P + (BGRA ? 2 : 0)]));
                Image.put(char(Pixels[P + 1]));
                Image.put(char(Pixels[P + (BGRA ? 0 : 2)]));
            }
            if (!Image)
                throw std::runtime_error("Cannot save scene capture.");
        }
        nlohmann::json Instances = nlohmann::json::array();
        const auto&    Resources = SurfaceStateSystem.GetGPUResources();
        for (std::size_t Index = 0; Index < Resources.GetSceneInstanceCount(); ++Index)
        {
            if (Resources.GetInstanceDescriptors(Index) == nullptr)
                continue;
            const auto&        Buffer = Resources.GetInstanceCurrentStateBuffer(Index);
            std::vector<float> Values(Buffer.GetSize() / sizeof(float));
            Buffer.Download(Values.data(), Buffer.GetSize());
            std::ofstream StateOutput(BenchmarkOutputPath.string() + ".state-" + std::to_string(Index) + ".f32",
                                      std::ios::binary);
            StateOutput.write(reinterpret_cast<const char*>(Values.data()),
                              static_cast<std::streamsize>(Buffer.GetSize()));
            if (!StateOutput)
                throw std::runtime_error("Cannot write benchmark State array.");
            std::uint64_t       Nonfinite = 0, Negative = 0, Positive = 0;
            double              Sum = 0.0;
            std::vector<double> StateSums(Resources.GetInstanceChannelCount(Index), 0.0);
            float               Maximum = 0.0F;
            std::uint64_t       StateHash = 14695981039346656037ULL;
            std::size_t         Scalar = 0;
            for (float Value : Values)
            {
                StateSums[Scalar++ % StateSums.size()] += Value;
                const auto Bits = std::bit_cast<std::uint32_t>(Value);
                for (unsigned Shift = 0; Shift < 32; Shift += 8)
                    StateHash = (StateHash ^ ((Bits >> Shift) & 0xffU)) * 1099511628211ULL;
                if (!std::isfinite(Value))
                {
                    ++Nonfinite;
                    continue;
                }
                Negative += Value < 0.0F;
                Positive += Value > 0.0F;
                Sum += Value;
                Maximum = std::max(Maximum, Value);
            }
            const auto Layout = SurfaceState::GetSurfaceSparseMetadataLayout(Resources.GetInstanceTexelCount(Index));
            std::array<std::uint32_t, 3> HeightDispatch{}, GeometryDispatch{};
            std::size_t                  ReactionBudgetExhausted = 0, ReactionArithmeticFailures = 0;
            if (PerformanceSettings().value("capture_image", false) &&
                Resources.GetInstanceDescriptors(Index)->HasReactions())
            {
                const auto&        Scratch = Resources.GetInstanceReactionScratchBuffer(Index);
                std::vector<float> Data(static_cast<std::size_t>(Scratch.GetSize() / sizeof(float)));
                Scratch.Download(Data.data(), Scratch.GetSize());
                const auto Stride = Data.size() / Resources.GetInstanceTexelCount(Index);
                for (std::size_t T = 0; T < Resources.GetInstanceTexelCount(Index); ++T)
                {
                    ReactionBudgetExhausted += Data[T * Stride + Stride - 2] != 0;
                    ReactionArithmeticFailures += Data[T * Stride + Stride - 1] != 0;
                }
            }
            const auto& Scheduling = Resources.GetInstanceAccumulationHeightBuffer(Index);
            Scheduling.Download(
                HeightDispatch.data(), sizeof(HeightDispatch), Layout.AccumulationCommandWord * sizeof(std::uint32_t));
            Scheduling.Download(
                GeometryDispatch.data(), sizeof(GeometryDispatch), Layout.GeometryCommandWord * sizeof(std::uint32_t));
            Instances.push_back({{"instance", Index},
                                 {"total_workgroups", Layout.WorkgroupCount},
                                 {"final_height_candidate_groups", HeightDispatch[0]},
                                 {"final_geometry_dispatch_groups", GeometryDispatch[0]},
                                 {"elements", Values.size()},
                                 {"nonfinite", Nonfinite},
                                 {"negative", Negative},
                                 {"positive", Positive},
                                 {"sum", Sum},
                                 {"state_sums", StateSums},
                                 {"reaction_budget_exhausted_texels", ReactionBudgetExhausted},
                                 {"reaction_arithmetic_failure_texels", ReactionArithmeticFailures},
                                 {"max", Maximum},
                                 {"state_fnv1a64", std::to_string(StateHash)}});
        }
        const auto           Memory = GPU::TGPUResourceMemoryStats::GetSnapshot();
        const nlohmann::json Result = {{"schema_version", 1},
                                       {"experiment", PerformanceSettings()},
                                       {"runtime_solver", SurfaceStateSystem.IsRuntimeSolverFlagsEnabled()},
                                       {"compact_storage", SurfaceStateSystem.IsCompactScratchStorageEnabled()},
                                       {"adaptive_geometry", SurfaceStateSystem.IsAdaptiveGeometryEnabled()},
                                       {"scope", "after final drain frames; device idle; before resource destruction"},
                                       {"submitted_frames", NextBenchmarkFrameIndex},
                                       {"simulation_step_serial", FrameCoordinator.GetStepSerial()},
                                       {"instances", Instances},
                                       {"allocated_buffer_bytes", Memory.CurrentBufferBytes},
                                       {"allocated_image_bytes", Memory.CurrentImageBytes},
                                       {"allocated_total_bytes", Memory.CurrentTotalBytes},
                                       {"peak_allocated_total_bytes", Memory.PeakTotalBytes},
                                       {"buffer_category_bytes", Memory.BufferCategoryCurrentBytes}};
        std::ofstream        Output(BenchmarkOutputPath.string() + ".validation.json");
        if (!(Output << Result.dump(2) << '\n'))
            throw std::runtime_error("Unable to write benchmark validation.");
    }

#pragma endregion
} // namespace MDSS::Rendering
