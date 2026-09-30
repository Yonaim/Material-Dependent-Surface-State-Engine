/** @file DemoSurfaceEffects.h
 * @brief Demo appearance adapter; runtime State IDs remain defined by loaded profiles.
 */
#pragma once
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"
#include <glm/vec3.hpp>
namespace MDSS
{
    struct TDemoSurfaceStateBindings
    {
        TStateId Wetness = InvalidStateId;
        TStateId Mud = InvalidStateId;
        TStateId WaterFilm = InvalidStateId;
    };
    inline TDemoSurfaceStateBindings ResolveDemoSurfaceStates(const TSurfaceStateRegistry& Registry)
    {
        TDemoSurfaceStateBindings Result;
        for (TStateId ID = 0; ID < Registry.GetStateCount(); ++ID)
        {
            const auto& Name = Registry.GetStateName(ID);
            if (Name == "wetness")
                Result.Wetness = ID;
            else if (Name == "mud")
                Result.Mud = ID;
            else if (Name == "waterfilm")
                Result.WaterFilm = ID;
        }
        return Result;
    }
    struct TDemoSurfaceEffectSettings
    {
        bool  bEnabled = true;
        bool  bMudDisplacement = true;
        bool  bWaterFilmDisplacement = true;
        bool  bHeightFieldSmoothing = false;
        float DryRoughness = 0.65F;
        float WetRoughness = 0.16F;
        float MudRoughness = 0.48F;
        float WetnessStrength = 1.0F;
        float WetnessSpecularStrength = 1.0F;
        float WaterFilmOpacity = 1.0F;
        float WaterFilmRoughness = 0.16F;
        glm::vec3 WetnessTint{0.44F, 0.56F, 0.68F};
        glm::vec3 WaterFilmTint{0.35F, 0.53F, 0.68F};
    };
}
