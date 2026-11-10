/**
 * @file RendererSettings.cpp
 * @brief 렌더·진단 옵션을 적용하고 필요한 표시 자원과 cache를 갱신한다.
 */

#include "Rendering/Core/RendererShared.h"

namespace MDSS::Rendering
{
// ============================================================
// 00. Render and Debug Settings
// ============================================================
#pragma region 00_Render_and_Debug_Settings
// 변경된 표시 옵션을 해당 pipeline·cache·Surface 설정에 반영한다.

    ERenderViewMode FRenderer::GetRenderViewMode() const noexcept
    {
        return GetRenderViewMode(0);
    }

    ERenderViewMode FRenderer::GetRenderViewMode(std::size_t ViewportIndex) const noexcept
    {
        return ViewportIndex < ViewModes.size() ? ViewModes[ViewportIndex] : ERenderViewMode::Lit;
    }

    void FRenderer::SetWireframeLineWidth(float Width) noexcept
    {
        if (std::isfinite(Width))
        {
            WireframeLineWidth = std::clamp(Width, WireframeLineWidthMin, WireframeLineWidthMax);
        }
    }

    bool FRenderer::IsWorldGridVisible() const noexcept
    {
        return bWorldGridVisible;
    }

    void FRenderer::SetWorldGridVisible(bool bVisible) noexcept
    {
        bWorldGridVisible = bVisible;
    }

    bool FRenderer::IsWorldAxisVisible() const noexcept
    {
        return bWorldAxisVisible;
    }

    void FRenderer::SetWorldAxisVisible(bool bVisible) noexcept
    {
        bWorldAxisVisible = bVisible;
    }

    void FRenderer::SetRenderViewMode(ERenderViewMode Mode)
    {
        SetRenderViewMode(0, Mode);
    }

    void FRenderer::SetRenderViewMode(std::size_t ViewportIndex, ERenderViewMode Mode)
    {
        if (ViewportIndex >= ViewModes.size())
            ViewModes.resize(ViewportIndex + 1U, ERenderViewMode::Lit);
        if (ViewModes[ViewportIndex] == Mode)
            return;

        ViewModes[ViewportIndex] = Mode;
        FLogger::Info("TRenderer",
                      "Viewport " + std::to_string(ViewportIndex + 1U) + " render view mode changed to " +
                          GetRenderViewModeName(Mode) + ".");
    }

    std::uint32_t FRenderer::GetDebugStateChannel() const noexcept
    {
        return DebugStateChannel;
    }

    void FRenderer::SetDebugStateChannel(std::uint32_t Channel)
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

    bool FRenderer::IsStateHeatmapReliefShadingEnabled() const noexcept
    {
        return bStateHeatmapReliefShadingEnabled;
    }

    bool FRenderer::IsStateHeatmapAllStatesEnabled() const noexcept
    {
        return bStateHeatmapAllStatesEnabled;
    }

    void FRenderer::SetStateHeatmapAllStatesEnabled(bool bEnabled) noexcept
    {
        bStateHeatmapAllStatesEnabled = bEnabled;
    }

    void FRenderer::SetSurfaceDebugDisplaySettings(const FSurfaceDebugDisplaySettings& Settings)
    {
        const auto Positive = [](float Value) { return std::isfinite(Value) && Value >= 1e-8F && Value <= 1e6F; };

        if (!Positive(Settings.DisplacementScale) || Settings.AccumulationComponent > 4 ||
            Settings.HeightGridMode > 2 || Settings.HeightGridBlockSize < 1 || Settings.HeightGridBlockSize > 256)
            throw std::invalid_argument("Invalid Surface debug range, display scale or component.");
        SurfaceDebugSettings = Settings;
    }

    FDemoSurfaceStateBindings FRenderer::GetDemoSurfaceStateBindings() const
    {
        return ResolveDemoSurfaceStates(SurfaceData.GetSurfaceStateRegistry());
    }

    bool FRenderer::CanRenderLitOverlays() const noexcept
    {
        return DemoEffects.bEnabled && SurfaceLitPipelines[0] && BaseSurfaceLitPipelines[0] && TexelGeometryPreview &&
               OverlaySides && HeightFieldSmoothing && MudLayerGeometry && MudOverlayTopPipelines[0] &&
               MudOverlaySidePipelines[0];
    }

    std::array<bool, 3> FRenderer::GetLitOverlayActivity(const FStaticMeshInstance& Instance,
                                                         const SurfaceState::FSurfaceSharedGeometryGPUResources* Shared,
                                                         const FDemoSurfaceStateBindings& Bindings) const
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

    bool FRenderer::IsLitTexelMeshBaseRendered(const FScene& Scene, std::size_t Instance) const
    {
        const auto& Instances = Scene.GetStaticMeshInstances();
        const auto& Resources = SurfaceStateSystem.GetGPUResources();
        const bool  bAnyLitView = std::any_of(
            ViewModes.begin(), ViewModes.end(), [](ERenderViewMode Mode) { return Mode == ERenderViewMode::Lit; });
        if (!bAnyLitView || !CanRenderLitOverlays() || Instance >= Instances.size() ||
            Instance >= Resources.GetSceneInstanceCount() || !Resources.GetInstanceDescriptors(Instance))
            return false;
        const auto Activity = GetLitOverlayActivity(
            Instances[Instance], Resources.GetInstanceSharedGeometry(Instance), GetDemoSurfaceStateBindings());
        return Activity[0] || Activity[1] || Activity[2];
    }

#pragma endregion

// ============================================================
// 01. Surface Effect Configuration
// ============================================================
#pragma region 01_Surface_Effect_Configuration
// State별 외형 설정을 GPU material 입력에 연결한다.

    void FRenderer::SetDemoSurfaceEffectSettings(const FDemoSurfaceEffectSettings& Settings)
    {
        const auto Roughness = [](float V) { return std::isfinite(V) && V >= 0.05F && V <= 1.0F; };
        const auto Unit = [](float V) { return std::isfinite(V) && V >= 0.0F && V <= 1.0F; };
        const auto Color = [&](const glm::vec3& V) { return Unit(V.r) && Unit(V.g) && Unit(V.b); };
        const auto ColorMapping = [&](const FDemoStateColorMapping& Mapping)
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

    void FRenderer::SetSceneLitHeightDisplayScale(FScene& Scene, float Scale)
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

    bool FRenderer::InspectTexel(const FScene& Scene, std::size_t InstanceIndex, std::uint32_t Triangle, glm::vec2 UV)
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
            SurfaceState::FSurfaceTexelSelection{InstanceIndex, Surface, Texel, Triangle, {X, Y}, std::move(Profile)};
        return true;
    }

    void FRenderer::ClearInspectedTexel() noexcept
    {
        InspectedTexel.reset();

        if (TexelInspector)
            TexelInspector->Invalidate();
    }

    const std::optional<SurfaceState::FSurfaceTexelSnapshot>& FRenderer::GetTexelSnapshot() const noexcept
    {
        static const std::optional<SurfaceState::FSurfaceTexelSnapshot> Empty;
        return TexelInspector ? TexelInspector->GetSnapshot() : Empty;
    }

    void FRenderer::SetStateHeatmapReliefShadingEnabled(bool bEnabled)
    {
        if (bStateHeatmapReliefShadingEnabled == bEnabled)
        {
            return;
        }

        bStateHeatmapReliefShadingEnabled = bEnabled;
    }

    ESolverTransferWeightView FRenderer::GetSolverTransferWeightView() const noexcept
    {
        return SolverTransferWeightView;
    }

    void FRenderer::SetSolverTransferWeightView(ESolverTransferWeightView View)
    {
        if (SolverTransferWeightView == View)
        {
            return;
        }

        SolverTransferWeightView = View;
    }

    std::uint32_t FRenderer::GetTexelGridBlockSize() const noexcept
    {
        return TexelGridBlockSize;
    }

    void FRenderer::SetTexelGridBlockSize(std::uint32_t Size)
    {
        if (Size != 8U && Size != 16U)
        {
            throw std::invalid_argument("Texel grid block size must be 8 or 16.");
        }

        if (TexelGridBlockSize == Size)
            return;
        TexelGridBlockSize = Size;
    }

    float FRenderer::GetTexelAreaReference() const noexcept
    {
        return TexelAreaReference;
    }

    void FRenderer::SetTexelAreaReference(float Area)
    {
        if (!std::isfinite(Area) || Area < 1.0e-12F || Area > 1.0e12F)
        {
            throw std::invalid_argument("Texel reference area must be finite and in [1e-12, 1e12].");
        }

        if (TexelAreaReference == Area)
            return;
        TexelAreaReference = Area;
    }

    float FRenderer::GetDebugViewParameter(ERenderViewMode Mode) const noexcept
    {
        switch (Mode)
        {
            case ERenderViewMode::SurfaceTexelGrid:
                return static_cast<float>(TexelGridBlockSize);
            case ERenderViewMode::SurfaceTexelArea:
                return TexelAreaReference;
            default:
                return static_cast<float>(SolverTransferWeightView);
        }
    }

    bool FRenderer::IsDebugGeometryDriveEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm::GeometryDrive);
    }

    void FRenderer::SetDebugGeometryDriveEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm::GeometryDrive, bEnabled);
    }

    bool FRenderer::IsDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm Term) const noexcept
    {
        return DebugSolverSettings.IsEnabled(Term);
    }

    void FRenderer::SetDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm Term, bool bEnabled)
    {
        DebugSolverSettings.SetEnabled(Term, bEnabled);
        SurfaceStateSystem.SetDebugSolverTermEnabled(Term, bEnabled);
    }

    void FRenderer::SetOverlayOccupancyTileSize(EOverlayOccupancyTileSize Size)
    {
        if (Size != EOverlayOccupancyTileSize::Tile8 && Size != EOverlayOccupancyTileSize::Tile16 &&
            Size != EOverlayOccupancyTileSize::Tile32)
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

    bool FRenderer::IsRawFluxCacheEnabled() const noexcept
    {
        return SurfaceStateSystem.IsRawFluxCacheEnabled();
    }

    void FRenderer::SetRawFluxCacheEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetRawFluxCacheEnabled(bEnabled);
    }

    bool FRenderer::IsCoalescedRawFluxLayoutEnabled() const noexcept
    {
        return SurfaceStateSystem.IsCoalescedRawFluxLayoutEnabled();
    }

    void FRenderer::SetCoalescedRawFluxLayoutEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetCoalescedRawFluxLayoutEnabled(bEnabled);
    }

    bool FRenderer::IsHalfRawFluxCacheEnabled() const noexcept
    {
        return SurfaceStateSystem.IsHalfRawFluxCacheEnabled();
    }

    void FRenderer::SetHalfRawFluxCacheEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetHalfRawFluxCacheEnabled(bEnabled);
    }

    bool FRenderer::IsHalfDynamicWeightsEnabled() const noexcept
    {
        return SurfaceStateSystem.IsHalfDynamicWeightsEnabled();
    }

    void FRenderer::SetHalfDynamicWeightsEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetHalfDynamicWeightsEnabled(bEnabled);
    }

    bool FRenderer::IsSparseSolverEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseSolverEnabled();
    }

    void FRenderer::SetSparseSolverEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseSolverEnabled(bEnabled);
    }

    bool FRenderer::IsSparseAccumulationHeightEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseAccumulationHeightEnabled();
    }

    void FRenderer::SetSparseAccumulationHeightEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseAccumulationHeightEnabled(bEnabled);
    }

    bool FRenderer::IsActiveChannelMaskEnabled() const noexcept
    {
        return SurfaceStateSystem.IsActiveChannelMaskEnabled();
    }

    void FRenderer::SetActiveChannelMaskEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetActiveChannelMaskEnabled(bEnabled);
    }

    bool FRenderer::IsPerWorkgroupChannelMaskEnabled() const noexcept
    {
        return SurfaceStateSystem.IsPerWorkgroupChannelMaskEnabled();
    }

    void FRenderer::SetPerWorkgroupChannelMaskEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetPerWorkgroupChannelMaskEnabled(bEnabled);
    }

    bool FRenderer::IsSparseSimulationGeometryEnabled() const noexcept
    {
        return SurfaceStateSystem.IsSparseSimulationGeometryEnabled();
    }

    void FRenderer::SetSparseSimulationGeometryEnabled(bool bEnabled) noexcept
    {
        SurfaceStateSystem.SetSparseSimulationGeometryEnabled(bEnabled);
    }

    bool FRenderer::IsStateDrivenGeometryUpdateEnabled() const noexcept
    {
        return DebugSolverSettings.bStateDrivenGeometryUpdateEnabled;
    }

    void FRenderer::SetStateDrivenGeometryUpdateEnabled(bool bEnabled)
    {
        DebugSolverSettings.bStateDrivenGeometryUpdateEnabled = bEnabled;
        SurfaceStateSystem.SetStateDrivenGeometryUpdateEnabled(bEnabled);
    }

    bool FRenderer::IsDebugNormalWeightEnabled() const noexcept
    {
        return IsDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm::NormalWeight);
    }

    void FRenderer::SetDebugNormalWeightEnabled(bool bEnabled)
    {
        SetDebugSolverTermEnabled(SurfaceState::ESurfaceSolverTerm::NormalWeight, bEnabled);
    }

    bool FRenderer::GetFlipNormalY() const noexcept
    {
        return bFlipNormalY;
    }

    void FRenderer::SetFlipNormalY(bool bEnabled)
    {
        if (bFlipNormalY == bEnabled)
        {
            return;
        }

        bFlipNormalY = bEnabled;
        FLogger::Info("TRenderer", std::string("Normal-map Y flip ") + (bFlipNormalY ? "enabled." : "disabled."));
    }

    float FRenderer::GetNormalStrength() const noexcept
    {
        return NormalStrength;
    }

    void FRenderer::SetNormalStrength(float Strength)
    {
        NormalStrength = std::clamp(Strength, 0.0F, 4.0F);
    }

    float FRenderer::GetAmbientLight() const noexcept
    {
        return AmbientLight;
    }

    void FRenderer::SetAmbientLight(float Intensity)
    {
        AmbientLight = std::clamp(Intensity, 0.0F, 1.0F);
    }

#pragma endregion
} // namespace MDSS::Rendering
