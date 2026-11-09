#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
// ============================================================
// 00. Render and Debug Settings
// ============================================================
#pragma region 00_Render_and_Debug_Settings

    TRenderViewMode TRenderer::GetRenderViewMode() const noexcept
    {
        return GetRenderViewMode(0);
    }

    TRenderViewMode TRenderer::GetRenderViewMode(std::size_t ViewportIndex) const noexcept
    {
        return ViewportIndex < ViewModes.size() ? ViewModes[ViewportIndex] : TRenderViewMode::Lit;
    }

    void TRenderer::SetWireframeLineWidth(float Width) noexcept
    {
        if (std::isfinite(Width))
        {
            WireframeLineWidth = std::clamp(Width, WireframeLineWidthMin, WireframeLineWidthMax);
        }
    }

    bool TRenderer::IsWorldGridVisible() const noexcept
    {
        return bWorldGridVisible;
    }

    void TRenderer::SetWorldGridVisible(bool bVisible) noexcept
    {
        bWorldGridVisible = bVisible;
    }

    bool TRenderer::IsWorldAxisVisible() const noexcept
    {
        return bWorldAxisVisible;
    }

    void TRenderer::SetWorldAxisVisible(bool bVisible) noexcept
    {
        bWorldAxisVisible = bVisible;
    }

    void TRenderer::SetRenderViewMode(TRenderViewMode Mode)
    {
        SetRenderViewMode(0, Mode);
    }

    void TRenderer::SetRenderViewMode(std::size_t ViewportIndex, TRenderViewMode Mode)
    {
        if (ViewportIndex >= ViewModes.size())
            ViewModes.resize(ViewportIndex + 1U, TRenderViewMode::Lit);
        if (ViewModes[ViewportIndex] == Mode)
            return;

        ViewModes[ViewportIndex] = Mode;
        TLogger::Info("TRenderer",
                      "Viewport " + std::to_string(ViewportIndex + 1U) + " render view mode changed to " +
                          GetRenderViewModeName(Mode) + ".");
    }

    std::uint32_t TRenderer::GetDebugStateChannel() const noexcept
    {
        return DebugStateChannel;
    }

    void TRenderer::SetDebugStateChannel(std::uint32_t Channel)
    {
        if (Channel >= SurfaceData.GetSurfaceStateRegistry().GetStateCount())
        {
            throw std::out_of_range("Debug State channel is outside the registered State range.");
        }
        if (DebugStateChannel == Channel)
        {
            return;
        }
        DebugStateChannel = Channel;
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    bool TRenderer::IsStateHeatmapReliefShadingEnabled() const noexcept
    {
        return bStateHeatmapReliefShadingEnabled;
    }

    bool TRenderer::IsStateHeatmapAllStatesEnabled() const noexcept
    {
        return bStateHeatmapAllStatesEnabled;
    }

    void TRenderer::SetStateHeatmapAllStatesEnabled(bool bEnabled) noexcept
    {
        bStateHeatmapAllStatesEnabled = bEnabled;
    }

    void TRenderer::SetSurfaceDebugDisplaySettings(const TSurfaceDebugDisplaySettings& Settings)
    {
        const auto Positive = [](float Value) { return std::isfinite(Value) && Value >= 1e-8F && Value <= 1e6F; };
        if (!Positive(Settings.DisplacementScale) || Settings.AccumulationComponent > 4 ||
            Settings.HeightGridMode > 2 || Settings.HeightGridBlockSize < 1 || Settings.HeightGridBlockSize > 256)
            throw std::invalid_argument("Invalid Surface debug range, display scale or component.");
        SurfaceDebugSettings = Settings;
    }

    TDemoSurfaceStateBindings TRenderer::GetDemoSurfaceStateBindings() const
    {
        return ResolveDemoSurfaceStates(SurfaceData.GetSurfaceStateRegistry());
    }

    bool TRenderer::CanRenderLitOverlays() const noexcept
    {
        return DemoEffects.bEnabled && SurfaceLitPipelines[0] && BaseSurfaceLitPipelines[0] && TexelGeometryPreview &&
               OverlaySides && HeightFieldSmoothing && MudLayerGeometry && MudOverlayTopPipelines[0] &&
               MudOverlaySidePipelines[0];
    }

    std::array<bool, 3> TRenderer::GetLitOverlayActivity(const TStaticMeshInstance& Instance,
                                                         const SurfaceState::TSurfaceSharedGeometryGPUResources* Shared,
                                                         const TDemoSurfaceStateBindings& Bindings) const
    {
        return {(DemoEffects.bMudDisplacement && Bindings.Mud != SurfaceState::InvalidStateId &&
                 UsesAccumulationGeometry(Instance, Assets, SurfaceData, Shared, "mud")) ||
                    (DemoEffects.bIceDisplacement && Bindings.Ice != SurfaceState::InvalidStateId &&
                     UsesAccumulationGeometry(Instance, Assets, SurfaceData, Shared, "ice")),
                DemoEffects.bWaterFilmDisplacement && Bindings.WaterFilm != SurfaceState::InvalidStateId &&
                    UsesAccumulationGeometry(Instance, Assets, SurfaceData, Shared, "waterfilm"),
                DemoEffects.bLavaDisplacement && Bindings.Lava != SurfaceState::InvalidStateId &&
                    UsesAccumulationGeometry(Instance, Assets, SurfaceData, Shared, "lava")};
    }

    bool TRenderer::IsLitTexelMeshBaseRendered(const TScene& Scene, std::size_t Instance) const
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        const auto& Resources = SurfaceStateSystem.GetGPUResources();
        const bool  bAnyLitView = std::any_of(
            ViewModes.begin(), ViewModes.end(), [](TRenderViewMode Mode) { return Mode == TRenderViewMode::Lit; });
        if (!bAnyLitView || !CanRenderLitOverlays() || Instance >= Instances.size() ||
            Instance >= Resources.GetSceneInstanceCount() || !Resources.GetInstanceDescriptors(Instance))
            return false;
        const auto Activity = GetLitOverlayActivity(
            Instances[Instance], Resources.GetInstanceSharedGeometry(Instance), GetDemoSurfaceStateBindings());
        return Activity[0] || Activity[1] || Activity[2];
    }

    void TRenderer::SetDemoSurfaceEffectSettings(const TDemoSurfaceEffectSettings& Settings)
    {
        const auto Roughness = [](float V) { return std::isfinite(V) && V >= 0.05F && V <= 1.0F; };
        const auto Unit = [](float V) { return std::isfinite(V) && V >= 0.0F && V <= 1.0F; };
        const auto Color = [&](const glm::vec3& V) { return Unit(V.r) && Unit(V.g) && Unit(V.b); };
        const auto ColorMapping = [&](const TDemoStateColorMapping& Mapping)
        {
            return Unit(Mapping.RampStart) && Unit(Mapping.RampEnd) && Mapping.RampStart < Mapping.RampEnd &&
                   Color(Mapping.LowSaturationColor) && Color(Mapping.HighSaturationColor);
        };
        if (!Roughness(Settings.DryRoughness) || !Roughness(Settings.MudRoughness) ||
            !Roughness(Settings.WaterFilmRoughness) || !Unit(Settings.HeatStrength) || !Unit(Settings.LavaThreshold) ||
            !Unit(Settings.MudNoiseAmount) || !Unit(Settings.WaterFilmOpacity) ||
            !std::isfinite(Settings.LowAmountHeightFade) || Settings.LowAmountHeightFade < 0.0F ||
            Settings.LowAmountHeightFade > 0.5F || !ColorMapping(Settings.HeatColorMap) ||
            !ColorMapping(Settings.MudColorMap) || !ColorMapping(Settings.WaterFilmColorMap) ||
            !ColorMapping(Settings.LavaColorMap))
            throw std::invalid_argument("Invalid demo surface effect settings.");
        const bool bHeightSmoothingChanged = DemoEffects.bHeightFieldSmoothing != Settings.bHeightFieldSmoothing;
        const bool bCoverageSmoothingChanged = DemoEffects.bCoverageSmoothing != Settings.bCoverageSmoothing;

        // Smoothing is exposed as an A/B performance experiment.  Drain the two
        // frames-in-flight when its execution mode changes so profiling after the
        // click cannot still contain commands recorded with the previous mode.
        // This is a one-time UI transition cost, never a per-frame wait.
        if ((bHeightSmoothingChanged || bCoverageSmoothingChanged) && Context.GetDevice() != VK_NULL_HANDLE)
        {
            if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
                throw std::runtime_error("Failed to idle Vulkan device while changing smoothing mode.");
        }

        DemoEffects = Settings;
        if (MudLayerGeometry)
            MudLayerGeometry->SetLowAmountHeightFade(DemoEffects.LowAmountHeightFade);
        if (bHeightSmoothingChanged && HeightFieldSmoothing)
        {
            if (DemoEffects.bHeightFieldSmoothing)
            {
                // The next ON frame must rebuild the output from the raw preview.
                HeightFieldSmoothing->Invalidate();
            }
            else
            {
                // Height smoothing allocates one full texel-geometry buffer per
                // active instance/channel on first use.  The old implementation
                // stopped dispatching when OFF but kept those buffers resident,
                // so an ON->OFF A/B run did not return to the original GPU-memory
                // footprint.  The device is idle above, therefore it is safe to
                // reclaim them here.
                HeightFieldSmoothing->ReleaseOutputs();
            }
        }
    }

    void TRenderer::SetSceneLitHeightDisplayScale(TScene& Scene, float Scale)
    {
        const bool bChanged = Scene.GetLitHeightDisplayScale() != Scale;
        Scene.SetLitHeightDisplayScale(Scale);
        if (!bChanged)
            return;
        if (TexelInspector)
        {
            TexelInspector->Invalidate();
        }
        if (MudLayerGeometry)
            MudLayerGeometry->InvalidateSparseOutput();
    }

    bool TRenderer::InspectTexel(const TScene& Scene, std::size_t InstanceIndex, std::uint32_t Triangle, glm::vec2 UV)
    {
        ClearInspectedTexel();
        const auto& Instances = Scene.GetStaticMeshInstances();
        if (!TexelInspector || InstanceIndex >= Instances.size() || !std::isfinite(UV.x) || !std::isfinite(UV.y) ||
            UV.x < 0 || UV.x > 1 || UV.y < 0 || UV.y > 1)
            return false;
        const auto& Instance = Instances[InstanceIndex];
        if (!SurfaceData.HasSurfaceData(Instance.GetSurfaceData()) ||
            !SurfaceStateSystem.GetGPUResources().GetInstanceDescriptors(InstanceIndex))
            return false;
        const auto& Triangles = Assets.GetMesh(Instance.GetMesh()).GetTriangles();
        if (Triangle >= Triangles.size())
            return false;
        const auto& Geometry = *SurfaceData.GetSurfaceData(Instance.GetSurfaceData()).GetSharedGeometry();
        const auto  Surface = Triangles[Triangle].Surface;
        if (Surface >= Geometry.GetSurfaces().size())
            return false;
        const auto& Range = Geometry.GetSurface(Surface);
        const auto X = std::min(static_cast<std::uint32_t>(UV.x * Range.Resolution.Width), Range.Resolution.Width - 1U);
        const auto Y =
            std::min(static_cast<std::uint32_t>(UV.y * Range.Resolution.Height), Range.Resolution.Height - 1U);
        const auto  Texel = Range.FirstTexel + Y * Range.Resolution.Width + X;
        const auto  ProfileIndex = Geometry.GetProfileIndex(Texel);
        const auto& Profiles = SurfaceData.GetSurfaceProfileTable(Instance.GetSurfaceData());
        std::string Profile = "Unassigned";
        if (ProfileIndex < Profiles.size())
            Profile = Assets.GetSRProfile(Profiles[ProfileIndex]).GetName();
        InspectedTexel =
            SurfaceState::TSurfaceTexelSelection{InstanceIndex, Surface, Texel, Triangle, {X, Y}, std::move(Profile)};
        return true;
    }

    void TRenderer::ClearInspectedTexel() noexcept
    {
        InspectedTexel.reset();
        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    const std::optional<SurfaceState::TSurfaceTexelSnapshot>& TRenderer::GetTexelSnapshot() const noexcept
    {
        static const std::optional<SurfaceState::TSurfaceTexelSnapshot> Empty;
        return TexelInspector ? TexelInspector->GetSnapshot() : Empty;
    }

    void TRenderer::SetStateHeatmapReliefShadingEnabled(bool bEnabled)
    {
        if (bStateHeatmapReliefShadingEnabled == bEnabled)
        {
            return;
        }
        bStateHeatmapReliefShadingEnabled = bEnabled;
    }

    TSolverTransferWeightView TRenderer::GetSolverTransferWeightView() const noexcept
    {
        return SolverTransferWeightView;
    }

    void TRenderer::SetSolverTransferWeightView(TSolverTransferWeightView View)
    {
        if (SolverTransferWeightView == View)
        {
            return;
        }
        SolverTransferWeightView = View;
    }

    std::uint32_t TRenderer::GetTexelGridBlockSize() const noexcept
    {
        return TexelGridBlockSize;
    }

    void TRenderer::SetTexelGridBlockSize(std::uint32_t Size)
    {
        if (Size != 8U && Size != 16U)
        {
            throw std::invalid_argument("Texel grid block size must be 8 or 16.");
        }
        if (TexelGridBlockSize == Size)
            return;
        TexelGridBlockSize = Size;
    }

    float TRenderer::GetTexelAreaReference() const noexcept
    {
        return TexelAreaReference;
    }

    void TRenderer::SetTexelAreaReference(float Area)
    {
        if (!std::isfinite(Area) || Area < 1.0e-12F || Area > 1.0e12F)
        {
            throw std::invalid_argument("Texel reference area must be finite and in [1e-12, 1e12].");
        }
        if (TexelAreaReference == Area)
            return;
        TexelAreaReference = Area;
    }

    float TRenderer::GetDebugViewParameter(TRenderViewMode Mode) const noexcept
    {
        switch (Mode)
        {
            case TRenderViewMode::SurfaceTexelGrid:
                return static_cast<float>(TexelGridBlockSize);
            case TRenderViewMode::SurfaceTexelArea:
                return TexelAreaReference;
            default:
                return static_cast<float>(SolverTransferWeightView);
        }
    }

    bool TRenderer::IsDebugGeometryDriveEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm::GeometryDrive);
    }

    void TRenderer::SetDebugGeometryDriveEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    bool TRenderer::IsDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm Term) const noexcept
    {
        return DebugSolverSettings.IsEnabled(Term);
    }

    void TRenderer::SetDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm Term, bool bEnabled)
    {
        DebugSolverSettings.SetEnabled(Term, bEnabled);
        SurfaceStateSystem.SetDebugSolverTermEnabled(Term, bEnabled);
    }

    void TRenderer::SetOverlayOccupancyTileSize(TOverlayOccupancyTileSize Size)
    {
        if (Size != TOverlayOccupancyTileSize::Tile8 && Size != TOverlayOccupancyTileSize::Tile16 &&
            Size != TOverlayOccupancyTileSize::Tile32)
            throw std::invalid_argument("Overlay occupancy tile size must be 8, 16, or 32.");
        if (OverlayOccupancyTileSize == Size)
            return;
        if (vkDeviceWaitIdle(Context.GetDevice()) != VK_SUCCESS)
            throw std::runtime_error("Failed to wait for GPU before changing overlay occupancy tile size.");
        OverlayOccupancyTileSize = Size;
        const std::uint32_t Tile = static_cast<std::uint32_t>(Size);
        if (MudLayerGeometry)
            MudLayerGeometry->SetOccupancyTileSize(Tile);
    }

    bool TRenderer::IsRawFluxCacheEnabled() const noexcept
    {
        return SurfaceStateSystem.IsRawFluxCacheEnabled();
    }

    void TRenderer::SetRawFluxCacheEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetRawFluxCacheEnabled(bEnabled);
    }

    bool TRenderer::IsCoalescedRawFluxLayoutEnabled() const noexcept
    {
        return SurfaceStateSystem.IsCoalescedRawFluxLayoutEnabled();
    }

    void TRenderer::SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetCoalescedRawFluxLayoutEnabled(bEnabled);
    }

    bool TRenderer::IsHalfRawFluxCacheEnabled() const noexcept
    {
        return SurfaceStateSystem.IsHalfRawFluxCacheEnabled();
    }

    void TRenderer::SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetHalfRawFluxCacheEnabled(bEnabled);
    }

    bool TRenderer::IsHalfDynamicWeightsEnabled() const noexcept
    {
        return SurfaceStateSystem.IsHalfDynamicWeightsEnabled();
    }

    void TRenderer::SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetHalfDynamicWeightsEnabled(bEnabled);
    }

    bool TRenderer::IsSparseSolverEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseSolverEnabled();
    }

    void TRenderer::SetSparseSolverEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseSolverEnabled(bEnabled);
    }

    bool TRenderer::IsSparseAccumulationHeightEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseAccumulationHeightEnabled();
    }

    void TRenderer::SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseAccumulationHeightEnabled(bEnabled);
    }

    bool TRenderer::IsActiveChannelMaskEnabled() const noexcept
    {
        return SurfaceStateSystem.IsActiveChannelMaskEnabled();
    }

    void TRenderer::SetActiveChannelMaskEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetActiveChannelMaskEnabled(bEnabled);
    }

    bool TRenderer::IsPerWorkgroupChannelMaskEnabled() const noexcept
    {
        return SurfaceStateSystem.IsPerWorkgroupChannelMaskEnabled();
    }

    void TRenderer::SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetPerWorkgroupChannelMaskEnabled(bEnabled);
    }

    bool TRenderer::IsSparseSimulationGeometryEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseSimulationGeometryEnabled();
    }

    void TRenderer::SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseSimulationGeometryEnabled(bEnabled);
    }

    bool TRenderer::IsStateDrivenGeometryUpdateEnabled() const noexcept
    {
        return DebugSolverSettings.bStateDrivenGeometryUpdateEnabled;
    }

    void TRenderer::SetStateDrivenGeometryUpdateEnabled(bool bEnabled)
    {
        DebugSolverSettings.bStateDrivenGeometryUpdateEnabled = bEnabled;
        SurfaceStateSystem.SetStateDrivenGeometryUpdateEnabled(bEnabled);
    }

    bool TRenderer::IsDebugNormalWeightEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm::NormalWeight);
    }

    void TRenderer::SetDebugNormalWeightEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(SurfaceState::TSurfaceSolverTerm::NormalWeight, bEnabled);
    }

    bool TRenderer::GetFlipNormalY() const noexcept
    {
        return bFlipNormalY;
    }

    void TRenderer::SetFlipNormalY(bool bEnabled)
    {
        if (bFlipNormalY == bEnabled)
        {
            return;
        }

        bFlipNormalY = bEnabled;
        TLogger::Info("TRenderer", std::string("Normal-map Y flip ") + (bFlipNormalY ? "enabled." : "disabled."));
    }

    float TRenderer::GetNormalStrength() const noexcept
    {
        return NormalStrength;
    }

    void TRenderer::SetNormalStrength(float Strength)
    {
        NormalStrength = std::clamp(Strength, 0.0F, 4.0F);
    }

    float TRenderer::GetAmbientLight() const noexcept
    {
        return AmbientLight;
    }

    void TRenderer::SetAmbientLight(float Intensity)
    {
        AmbientLight = std::clamp(Intensity, 0.0F, 1.0F);
    }

#pragma endregion
} // namespace MDSS::Rendering
