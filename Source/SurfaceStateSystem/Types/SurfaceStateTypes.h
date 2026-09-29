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
    using TStateId = std::uint32_t;
    inline constexpr TStateId InvalidStateId = UINT32_MAX;

    using TSurfaceProfileIndex = std::uint32_t;
    using TSurfaceInstanceID = std::uint32_t;
    inline constexpr TSurfaceProfileIndex InvalidSurfaceProfileIndex = UINT32_MAX;
    inline constexpr TSurfaceInstanceID   InvalidSurfaceInstanceID = UINT32_MAX;

    struct TSurfaceStateParameters
    {
        // Saturation이 1이 되는 포화 기준량이며 State의 저장 상한은 아니다.
        float StateCapacity = 1.0F;
        float InputFactor = 1.0F;
        // [0, 1] 무차원 계수. Solver가 각 전달 경로의 기준 속도를 곱한다.
        float SaturationTransferFactor = 0.0F;
        float GeometryTransferFactor = 0.0F;
        float DecayRate = 0.0F;
        float CavityRetentionFactor = 0.0F;
        float AccumulationFactor = 0.0F;
        float CavityFillFactor = 0.0F;
    };

    /** @brief Source and target names are canonicalized during profile loading. */
    struct TSurfaceStateTransition
    {
        std::string Source;
        std::string Target;
        float       Threshold = 0.0F;
        float       TransitionRate = 0.0F;
    };

    /** @brief A profile contains only the states and transitions it defines. */
    struct TSurfaceResponseProfileData
    {
        std::unordered_map<std::string, TSurfaceStateParameters> States;
        std::vector<TSurfaceStateTransition>                     Transitions;
    };

    using TSurfaceStateValues = std::vector<float>;

    /** @brief Trim ASCII whitespace and lowercase ASCII letters; preserve all other bytes. */
    [[nodiscard]] std::string NormalizeSurfaceStateName(std::string_view Name);

    /** @throws std::invalid_argument if any parameter or transition is invalid. */
    void ValidateSurfaceResponseProfileData(const TSurfaceResponseProfileData& Data);
} // namespace MDSS
