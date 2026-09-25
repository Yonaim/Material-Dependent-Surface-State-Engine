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
    struct RegisteredSurfaceStateTransition
    {
        StateId Source = InvalidStateId;
        StateId Target = InvalidStateId;
        float   Threshold = 0.0F;
        float   TransitionRate = 0.0F;
    };

    struct RegisteredSurfaceResponseProfileData
    {
        std::vector<std::optional<SurfaceStateParameters>> States;
        std::vector<RegisteredSurfaceStateTransition>      Transitions;
    };

    class SurfaceStateRegistry final
    {
    public:
        explicit SurfaceStateRegistry(const std::vector<SurfaceResponseProfileData>& Profiles);

        [[nodiscard]] std::size_t        GetStateCount() const noexcept;
        [[nodiscard]] StateId            GetStateId(std::string_view Name) const;
        [[nodiscard]] const std::string& GetStateName(StateId ID) const;
        [[nodiscard]] RegisteredSurfaceResponseProfileData
        ResolveProfile(const SurfaceResponseProfileData& Profile) const;

    private:
        std::vector<std::string>                 Names;
        std::unordered_map<std::string, StateId> IDs;
    };
} // namespace MDSS
