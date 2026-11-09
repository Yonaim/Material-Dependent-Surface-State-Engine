/**
 * @file SurfaceStateTypes.h
 * @brief 데이터 기반 State profile 및 registry 계약 자료형을 선언한다.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace MDSS::SurfaceState
{
    using FStateId = std::uint32_t;
    inline constexpr FStateId InvalidStateId = UINT32_MAX;

    using FSurfaceProfileIndex = std::uint32_t;
    using FSurfaceInstanceID = std::uint32_t;
    inline constexpr FSurfaceProfileIndex InvalidSurfaceProfileIndex = UINT32_MAX;
    inline constexpr FSurfaceInstanceID   InvalidSurfaceInstanceID = UINT32_MAX;

    struct FSurfaceStateParameters
    {
        // Fixed reference-area Capacity; texel total Capacity = StateCapacity * WorldArea / ReferenceArea.
        // State stores total amount; Capacity is a saturation reference, not an upper storage bound.
        float StateCapacity = 1.0F;
        float InputFactor = 1.0F;

        // [0, 1] 무차원 계수. Solver가 각 전달 경로의 기준 속도를 곱한다.
        float SaturationTransferFactor = 0.0F;
        float GeometryTransferFactor = 0.0F;

        // Amount per second per fixed reference area; converted to the actual texel area by the Solver.
        float DecayRate = 0.0F;
        float CavityRetentionFactor = 0.0F;
        float AccumulationFactor = 0.0F;
        float CavityFillFactor = 0.0F;

        // World-length thickness per reference-area accumulation amount.
        float ThicknessPerAmount = 0.01F;

        // Directional suppression of raw flux when transport leaves a more concave texel.
        float CavityTransportRetentionFactor = 0.0F;
    };

    /** @brief Source and target names are canonicalized during profile loading. */
    struct FSurfaceStateTransition
    {
        std::string Source;
        std::string Target;
        float       Threshold = 0.0F;
        float       TransitionRate = 0.0F;
    };

    enum class EReactionMode : std::uint32_t
    {
        Continuous,
        InflowFraction
    };
    enum class EReactionRateModel : std::uint32_t
    {
        Constant,
        StateProportional,
        MassAction
    };
    enum class EReactionFunction : std::uint32_t
    {
        Above,
        Below,
        RampAbove,
        RampBelow,
        Linear,
        Inhibit
    };
    struct FReactionTerm
    {
        std::string State;
        float       Coefficient = 1.0F;
    };
    struct FReactionModifier
    {
        std::string       State;
        EReactionFunction Function = EReactionFunction::Linear;

        // Both quantities refer to the fixed reference area, independently of Capacity.
        float Threshold = 0.0F;
        float Scale = 1.0F;
    };
    struct FReactionFactor
    {
        std::string State;
        float       Reference = 1.0F;
        float       Exponent = 1.0F;
    };
    struct FSurfaceReaction
    {
        std::string                    ID;
        bool                           bEnabled = true;
        EReactionMode                  Mode = EReactionMode::Continuous;
        std::vector<FReactionTerm>     Consumes;
        std::vector<FReactionTerm>     Produces;
        EReactionRateModel             RateModel = EReactionRateModel::Constant;
        std::string                    RateState;
        float                          RateCoefficient = 0.0F;
        std::vector<FReactionModifier> Modifiers;
        std::vector<FReactionFactor>   Factors;
        std::string                    InflowState;
        std::uint32_t                  InflowSources = 3U; // positive event input / neighbor incoming
        float                          Fraction = 0.0F;
    };

    /** @brief A profile contains only the states and transitions it defines. */
    struct FSurfaceResponseProfileData
    {
        std::unordered_map<std::string, FSurfaceStateParameters> States;
        std::vector<FSurfaceStateTransition>                     Transitions;
        std::vector<FSurfaceReaction>                            Reactions;
    };

    using FSurfaceStateValues = std::vector<float>;

    /** @brief Trim ASCII whitespace and lowercase ASCII letters; preserve all other bytes. */
    // State name normalization and profile validation
    [[nodiscard]] std::string NormalizeSurfaceStateName(std::string_view Name);

    /** @throws std::invalid_argument if any parameter or transition is invalid. */
    void ValidateSurfaceResponseProfileData(const FSurfaceResponseProfileData& Data);
    void ValidateSurfaceReactions(const FSurfaceResponseProfileData& Data);
} // namespace MDSS::SurfaceState
