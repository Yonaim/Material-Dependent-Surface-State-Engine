/**
 * @file DemoSurfaceEffects.h
 * @brief demo appearance와 Runtime State ID 연결을 제공하며, ID는 로드된 Profile이 결정한다.
 */
#pragma once
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <glm/vec3.hpp>

namespace MDSS::Rendering
{
    struct FDemoStateColorMapping
    {
        float     RampStart = 0.0F;
        float     RampEnd = 1.0F;
        glm::vec3 LowSaturationColor{0.0F};
        glm::vec3 HighSaturationColor{1.0F};
    };

    struct FDemoSurfaceStateBindings
    {
        SurfaceState::FStateId Ice = SurfaceState::InvalidStateId;
        SurfaceState::FStateId Heat = SurfaceState::InvalidStateId;
        SurfaceState::FStateId Mud = SurfaceState::InvalidStateId;
        SurfaceState::FStateId WaterFilm = SurfaceState::InvalidStateId;
        SurfaceState::FStateId AbsorbedWetness = SurfaceState::InvalidStateId;
        SurfaceState::FStateId Lava = SurfaceState::InvalidStateId;
        SurfaceState::FStateId Burn = SurfaceState::InvalidStateId;
        SurfaceState::FStateId Scorched = SurfaceState::InvalidStateId;
    };
    inline FDemoSurfaceStateBindings ResolveDemoSurfaceStates(const SurfaceState::FSurfaceStateRegistry& Registry)
    {
        FDemoSurfaceStateBindings Result;

        for (SurfaceState::FStateId ID = 0; ID < Registry.GetStateCount(); ++ID)
        {
            const auto& Name = Registry.GetStateName(ID);

            if (Name == "ice")
                Result.Ice = ID;
            else if (Name == "heat")
                Result.Heat = ID;
            else if (Name == "mud")
                Result.Mud = ID;
            else if (Name == "waterfilm")
                Result.WaterFilm = ID;
            else if (Name == "absorbedwetness")
                Result.AbsorbedWetness = ID;
            else if (Name == "lava")
                Result.Lava = ID;
            else if (Name == "burn")
                Result.Burn = ID;
            else if (Name == "scorched")
                Result.Scorched = ID;
        }

        return Result;
    }

    struct FDemoSurfaceEffectSettings
    {
        bool bEnabled = true;
        bool bHeatLayer = true;
        bool bMudDisplacement = true;
        bool bIceDisplacement = true;
        bool bWaterFilmDisplacement = true;
        bool bEmphasizeWaterFilm = false;
        bool bLavaDisplacement = true;

        // Smoothing is a render-performance experiment now: one global visual toggle per smoothing type.
        // Algorithm selection (sparse/precompute/separable) lives in the Performance tab.
        bool  bHeightFieldSmoothing = true;
        bool  bCoverageSmoothing = true;
        float DryRoughness = 0.65F;
        float HeatStrength = 1.0F;
        float LavaThreshold = 0.1F;
        float MudRoughness = 0.48F;
        float MudNoiseAmount = 0.35F;
        float WaterFilmOpacity = 0.5F;
        float WaterFilmRoughness = 0.16F;

        // Render-only fade for accumulation geometry at low normalized State saturation.
        float                  LowAmountHeightFade = 0.05F;
        FDemoStateColorMapping HeatColorMap{0.0F, 1.0F, {0.55F, 0.035F, 0.012F}, {0.95F, 0.075F, 0.025F}};
        FDemoStateColorMapping MudColorMap{0.25F, 1.0F, {0.16F, 0.09F, 0.045F}, {0.09F, 0.035F, 0.012F}};
        FDemoStateColorMapping WaterFilmColorMap{0.0F, 1.0F, {0.20F, 0.25F, 0.28F}, {0.12F, 0.20F, 0.23F}};
        FDemoStateColorMapping LavaColorMap{0.0F, 1.0F, {0.75F, 0.11F, 0.008F}, {1.0F, 0.36F, 0.035F}};
    };
}
