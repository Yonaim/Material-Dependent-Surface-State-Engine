#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
#pragma region Runtime_Settings_and_Benchmark_Capture

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

    void TRenderer::FinalizeBenchmarkCapture()
    {
        if (!bBenchmarkCaptureEnabled)
            return;
        nlohmann::json Instances = nlohmann::json::array();
        const auto&    Resources = SurfaceStateSystem.GetGPUResources();
        for (std::size_t Index = 0; Index < Resources.GetSceneInstanceCount(); ++Index)
        {
            if (Resources.GetInstanceDescriptors(Index) == nullptr)
                continue;
            const auto&        Buffer = Resources.GetInstanceCurrentStateBuffer(Index);
            std::vector<float> Values(Buffer.GetSize() / sizeof(float));
            Buffer.Download(Values.data(), Buffer.GetSize());
            std::uint64_t Nonfinite = 0, Negative = 0, Positive = 0;
            double        Sum = 0.0;
            float         Maximum = 0.0F;
            std::uint64_t StateHash = 14695981039346656037ULL;
            for (float Value : Values)
            {
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
            Instances.push_back({{"instance", Index},
                                 {"elements", Values.size()},
                                 {"nonfinite", Nonfinite},
                                 {"negative", Negative},
                                 {"positive", Positive},
                                 {"sum", Sum},
                                 {"max", Maximum},
                                 {"state_fnv1a64", std::to_string(StateHash)}});
        }
        const auto           Memory = GPU::TGPUResourceMemoryStats::GetSnapshot();
        const nlohmann::json Result = {{"schema_version", 1},
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
