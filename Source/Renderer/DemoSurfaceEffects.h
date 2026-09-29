/** @file DemoSurfaceEffects.h
 * @brief Demo appearance adapter; runtime State IDs remain defined by loaded profiles.
 */
#pragma once
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"
namespace MDSS
{
    struct TDemoSurfaceStateBindings
    {
        TStateId Wetness = InvalidStateId;
        TStateId Mud = InvalidStateId;
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
        }
        return Result;
    }
    struct TDemoSurfaceEffectSettings
    {
        bool  bEnabled = true;
        bool  bMudDisplacement = true;
        float DryRoughness = 0.65F;
        float WetRoughness = 0.16F;
        float MudRoughness = 0.48F;
        // Local-space visualization reference; not a new physics/profile contract.
        float MudHeightReference = 0.01F;
    };
}
