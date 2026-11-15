/**
 * @file SurfaceStateRegistry.h
 * @brief 로드된 SRProfile 기반 Runtime State registry 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MDSS::SurfaceState
{
    struct FRegisteredSurfaceStateTransition
    {
        FStateId Source = InvalidStateId;
        FStateId Target = InvalidStateId;
        float    Threshold = 0.0F;
        float    TransitionRate = 0.0F;
    };

    struct FRegisteredSurfaceResponseProfileData
    {
        std::vector<std::optional<FSurfaceStateParameters>> States;
        std::vector<FRegisteredSurfaceStateTransition>      Transitions;
    };

    class FSurfaceStateRegistry final
    {
    public:
        // Registry construction
        explicit FSurfaceStateRegistry(const std::vector<FSurfaceResponseProfileData>& Profiles);

        // State lookup and profile resolution
        [[nodiscard]] std::size_t        GetStateCount() const noexcept;
        [[nodiscard]] FStateId           GetStateId(std::string_view Name) const;
        [[nodiscard]] const std::string& GetStateName(FStateId ID) const;
        [[nodiscard]] FRegisteredSurfaceResponseProfileData
        ResolveProfile(const FSurfaceResponseProfileData& Profile) const;

    private:
        std::vector<std::string>                  Names;
        std::unordered_map<std::string, FStateId> IDs;
    };
} // namespace MDSS::SurfaceState
