/**
 * @file DemoSurfaceEffects.h
 * @brief demo appearance와 Runtime State ID 연결을 제공하며, ID는 로드된 Profile이 결정한다.
 */
#pragma once
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <glm/vec3.hpp>
namespace MDSS::Rendering
{
    struct TDemoStateColorMapping
    {
        float     RampStart = 0.0F;
        float     RampEnd = 1.0F;
        glm::vec3 LowSaturationColor{0.0F};
        glm::vec3 HighSaturationColor{1.0F};
    };

    struct TDemoSurfaceStateBindings
    {
        SurfaceState::TStateId Heat = SurfaceState::InvalidStateId;
        SurfaceState::TStateId Mud = SurfaceState::InvalidStateId;
        SurfaceState::TStateId WaterFilm = SurfaceState::InvalidStateId;
        SurfaceState::TStateId Lava = SurfaceState::InvalidStateId;
    };
    inline TDemoSurfaceStateBindings ResolveDemoSurfaceStates(const SurfaceState::TSurfaceStateRegistry& Registry)
    {
        TDemoSurfaceStateBindings Result;
        for (SurfaceState::TStateId ID = 0; ID < Registry.GetStateCount(); ++ID)
        {
            const auto& Name = Registry.GetStateName(ID);
            if (Name == "heat")
                Result.Heat = ID;
            else if (Name == "mud")
                Result.Mud = ID;
            else if (Name == "waterfilm")
                Result.WaterFilm = ID;
            else if (Name == "lava")
                Result.Lava = ID;
        }
        return Result;
    }
    struct TDemoSurfaceEffectSettings
    {
        bool      bEnabled = true;
        bool      bHeatLayer = true;
        bool      bMudDisplacement = true;
        bool      bWaterFilmDisplacement = true;
        bool      bLavaDisplacement = true;
        // Smoothing is a render-performance experiment now: one global visual toggle per smoothing type.
        // Algorithm selection (sparse/precompute/separable) lives in the Performance tab.
        bool      bHeightFieldSmoothing = true;
        bool      bCoverageSmoothing = true;
        float     DryRoughness = 0.65F;
        float     HeatStrength = 1.0F;
        float     LavaThreshold = 0.02F;
        float     MudRoughness = 0.48F;
        float     WaterFilmOpacity = 1.0F;
        float     WaterFilmRoughness = 0.16F;
        // Render-only fade for accumulation geometry at low normalized State saturation.
        float     LowAmountHeightFade = 0.05F;
        TDemoStateColorMapping HeatColorMap{0.0F, 1.0F,
                                             {0.55F, 0.035F, 0.012F}, {0.95F, 0.075F, 0.025F}};
        TDemoStateColorMapping MudColorMap{0.0F, 1.0F,
                                            {0.30F, 0.20F, 0.12F}, {0.09F, 0.035F, 0.012F}};
        TDemoStateColorMapping WaterFilmColorMap{0.0F, 1.0F,
                                                  {0.35F, 0.53F, 0.68F}, {0.58F, 0.74F, 0.88F}};
        TDemoStateColorMapping LavaColorMap{0.0F, 1.0F,
                                             {0.75F, 0.11F, 0.008F}, {1.0F, 0.36F, 0.035F}};
    };
}
