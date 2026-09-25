/**
 * @file SurfaceStateRegistry.h
 * @brief Deterministic runtime State IDs built from loaded SRProfiles.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    struct TRegisteredSurfaceStateTransition
    {
        TStateId Source = InvalidStateId;
        TStateId Target = InvalidStateId;
        float   Threshold = 0.0F;
        float   TransitionRate = 0.0F;
    };

    struct TRegisteredSurfaceResponseProfileData
    {
        std::vector<std::optional<TSurfaceStateParameters>> States;
        std::vector<TRegisteredSurfaceStateTransition>      Transitions;
    };

    class TSurfaceStateRegistry final
    {
    public:
        explicit TSurfaceStateRegistry(const std::vector<TSurfaceResponseProfileData>& Profiles);

        [[nodiscard]] std::size_t        GetStateCount() const noexcept;
        [[nodiscard]] TStateId            GetStateId(std::string_view Name) const;
        [[nodiscard]] const std::string& GetStateName(TStateId ID) const;
        [[nodiscard]] TRegisteredSurfaceResponseProfileData
        ResolveProfile(const TSurfaceResponseProfileData& Profile) const;

    private:
        std::vector<std::string>                 Names;
        std::unordered_map<std::string, TStateId> IDs;
    };
} // namespace MDSS
