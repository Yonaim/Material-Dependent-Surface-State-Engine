/**
 * @file SurfaceSimulationUI.h
 * @brief Surface Simulation 조작 패널의 입력과 적용할 작업을 정의한다.
 */

#pragma once

#include "Application/Editor/SurfaceSimulation/EditorPanel.h"
#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include "Rendering/Surface/Simulation/SurfaceSimulationDisplay.h"
#include <optional>
#include <string_view>

namespace MDSS::SurfaceSimulation
{
    enum class ESurfaceSimulationAction
    {
        None,
        Apply,
        Restart,
        ResetCamera,
        Capture,
        Snapshot,
        SnapshotWithData,
        CpuCheck,
        GpuCheck
    };

    enum class EInjectionTarget
    {
        PrimaryLiquid,
        Ice,
        Temperature
    };

    struct FGridResolution
    {
        glm::ivec2 Geometry;
        glm::ivec2 Simulation;
        int        SamplesPerCell = 0;

        bool operator==(const FGridResolution &) const = default;

        [[nodiscard]] static FGridResolution From(const FSurfaceSceneSettings &Settings);
        void                                 ApplyTo(FSurfaceSceneSettings &Settings) const;
    };

    /** @brief 계산 격자와 형상 격자의 프리셋 및 직접 입력을 그린다. 기본값이 있으면 복원 항목을 제공한다. */
    bool DrawGridResolution(FGridResolution            &Resolution,
                            int                         ColumnMultiple,
                            const std::array<float, 2> *SizeM = nullptr,
                            const FGridResolution      *SceneDefaults = nullptr);

    struct FSurfaceSimulationUI
    {
        FSurfaceSceneSettings          Draft;
        std::optional<FGridResolution> SceneDefaultGrid;
        FSurfaceSimulationViews        Views;
        std::array<char, 1024>  InputPath{};
        std::string             Message;
        std::string             ProfileError;
        std::string             SelectedAsset;
        int                     ExperimentMode = 0;
        std::filesystem::path   ExperimentPlot;
        std::string             ExperimentTitle;
        std::string             ExperimentStatus;
        std::string             ExperimentSummary;
        VkDescriptorSet         PlotTexture = VK_NULL_HANDLE;
        float                   PlotAspect = 1.F;
        bool                    bExperimentRunning = false;
        bool                    bExpandPlot = false;
        bool                    bPlaying = true;
        bool                    bAnimateCamera = false;
        bool                    bAutoTilt = true;
        bool                    bSingleStep = false;
        bool                    bInjectionEnabled = false;
        EInjectionTarget        InjectionTarget = EInjectionTarget::PrimaryLiquid;
        FInjectionBrush         Brush;
        bool                    bScheduledInlet = true;
        bool                    bScheduledHeat = true;
        bool                    bShowWaterSource = true;
        bool                    bShowOutletEdge = true;
        float                   PlaybackRate = 1.F;
        float                   DoseMilliliters = 0.15F;
        float                   IceDoseGrams = 0.15F;
        float                   TargetTemperatureC = 45.F;
        float                   TemperatureDuration = 1.F;
        std::string             InputMessage;
        float                   ManualTiltDegrees = 0.F;

        void SetDraft(const FSurfaceSceneSettings &Settings, bool bOpeningScene = false);
        [[nodiscard]] ESurfaceSimulationAction DrawExperiments();
        void                                   DrawExpandedExperiment();

        /** @brief 현재 에디터 패널에 해당하는 설정 또는 실험을 그린다. */
        [[nodiscard]] ESurfaceSimulationAction DrawPanel(EEditorPanel                          Panel,
                                                         const FSurfaceSceneSettings          &Active,
                                                         const Liquid::FLiquidMaterial        &ActiveLiquid,
                                                         float                                 Time,
                                                         float                                 TiltDegrees,
                                                         const FSurfaceSimulationDisplayStats &Timing,
                                                         FSceneAssetText                       SceneText);
    };

} // namespace MDSS::SurfaceSimulation
