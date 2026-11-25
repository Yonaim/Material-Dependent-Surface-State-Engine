/**
 * @file EditorPanel.h
 * @brief 공통 에디터 패널이 박막 세션에 위임할 기능을 지정한다.
 */
#pragma once

#include <string_view>

namespace MDSS::SurfaceSimulation
{
    struct FSceneAssetText
    {
        std::string_view Title;
        const char8_t   *Description = nullptr;
    };

    enum class EEditorPanel
    {
        Simulation,
        Object,
        Camera,
        SceneTracks,
        Assets,
        Conditions,
        Tools,
        Timeline,
        ApplySettings,
        Experiments,
        Profiler
    };

    enum class EEditorCapture
    {
        ActiveView,
        AllViews,
        StateAndPlot
    };
} // namespace MDSS::SurfaceSimulation
