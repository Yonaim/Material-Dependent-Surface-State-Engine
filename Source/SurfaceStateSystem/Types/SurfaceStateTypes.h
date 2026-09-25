/**
 * @file SurfaceStateTypes.h
 * @brief Data-driven state profile and registry contracts.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    using StateId = std::uint32_t;
    inline constexpr StateId InvalidStateId = UINT32_MAX;

    using SurfaceProfileIndex = std::uint32_t;
    using SurfaceInstanceID = std::uint32_t;
    inline constexpr SurfaceProfileIndex InvalidSurfaceProfileIndex = UINT32_MAX;
    inline constexpr SurfaceInstanceID   InvalidSurfaceInstanceID = UINT32_MAX;

    struct SurfaceStateParameters
    {
        float StateCapacity = 1.0F;
        float InputFactor = 1.0F;
        float SaturationTransferRate = 0.0F;
        float GeometryTransferRate = 0.0F;
        float DecayRate = 0.0F;
        float CavityRetentionFactor = 0.0F;
        float AccumulationFactor = 0.0F;
        float CavityFillFactor = 0.0F;
    };

    /** @brief Source and target names are canonicalized during profile loading. */
    struct SurfaceStateTransition
    {
        std::string Source;
        std::string Target;
        float       Threshold = 0.0F;
        float       TransitionRate = 0.0F;
    };

    /** @brief A profile contains only the states and transitions it defines. */
    struct SurfaceResponseProfileData
    {
        std::unordered_map<std::string, SurfaceStateParameters> States;
        std::vector<SurfaceStateTransition>                     Transitions;
    };

    using SurfaceStateValues = std::vector<float>;

    /** @brief Trim ASCII whitespace and lowercase ASCII letters; preserve all other bytes. */
    [[nodiscard]] std::string NormalizeSurfaceStateName(std::string_view Name);

    /** @throws std::invalid_argument if any parameter or transition is invalid. */
    void ValidateSurfaceResponseProfileData(const SurfaceResponseProfileData& Data);
} // namespace MDSS
