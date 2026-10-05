/**
 * @file DemoSurfaceEffects.h
 * @brief demo appearance와 Runtime State ID 연결을 제공하며, ID는 로드된 Profile이 결정한다.
 */
#pragma once
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <glm/vec3.hpp>
namespace MDSS::Rendering
{
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
        bool      bMudDisplacement = true;
        bool      bWaterFilmDisplacement = true;
        bool      bLavaDisplacement = true;
        bool      bHeightFieldSmoothing = false;
        bool      bWaterFilmSmoothing = false;
        bool      bMudSmoothing = false;
        bool      bLavaSmoothing = true;
        float     DryRoughness = 0.65F;
        float     HeatStrength = 1.0F;
        float     MudRoughness = 0.48F;
        float     WaterFilmOpacity = 1.0F;
        float     WaterFilmRoughness = 0.16F;
        glm::vec3 HeatTint{0.95F, 0.075F, 0.025F};
        glm::vec3 WaterFilmTint{0.35F, 0.53F, 0.68F};
    };
}
