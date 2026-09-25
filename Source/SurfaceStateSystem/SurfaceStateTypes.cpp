/**
 * @file SurfaceStateTypes.cpp
 * @brief 상태 채널, profile parameter, transition과 검증 계약.
 */

#include "SurfaceStateSystem/SurfaceStateTypes.h"

#include <array>
#include <cmath>
#include <stdexcept>
#include <string>

namespace MDSS
{
    namespace
    {
        struct SurfaceStateChannelName
        {
            SurfaceStateChannel Channel;
            std::string_view    Name;
        };

        constexpr std::array<SurfaceStateChannelName, SurfaceStateChannelCount> SurfaceStateChannelNames = {{
            {SurfaceStateChannel::Wetness, "wetness"},
            {SurfaceStateChannel::Heat, "heat"},
            {SurfaceStateChannel::Burn, "burn"},
            {SurfaceStateChannel::Mud, "mud"},
        }};

        /** @brief finite이고 0 이상이어야 하는 Profile parameter를 검사한다. */
        void ValidateFiniteNonNegative(float Value, std::string_view FieldName)
        {
            if (!std::isfinite(Value) || Value < 0.0F)
            {
                throw std::invalid_argument(std::string(FieldName) + " must be finite and non-negative.");
            }
        }

        /** @brief finite한 [0, 1] 구간이어야 하는 Profile parameter를 검사한다. */
        void ValidateUnitInterval(float Value, std::string_view FieldName)
        {
            if (!std::isfinite(Value) || Value < 0.0F || Value > 1.0F)
            {
                throw std::invalid_argument(std::string(FieldName) + " must be finite and in [0, 1].");
            }
        }
    } // namespace

    std::string_view GetSurfaceStateChannelName(SurfaceStateChannel Channel)
    {
        for (const SurfaceStateChannelName& Entry : SurfaceStateChannelNames)
        {
            if (Entry.Channel == Channel)
            {
                return Entry.Name;
            }
        }

        throw std::invalid_argument("SurfaceStateChannel is not a valid state channel.");
    }

    SurfaceStateChannel ParseSurfaceStateChannel(std::string_view Name)
    {
        for (const SurfaceStateChannelName& Entry : SurfaceStateChannelNames)
        {
            if (Entry.Name == Name)
            {
                return Entry.Channel;
            }
        }

        throw std::invalid_argument("Unknown SurfaceStateChannel name: " + std::string(Name));
    }

    std::size_t GetSurfaceStateChannelIndex(SurfaceStateChannel Channel)
    {
        const std::uint32_t Index = static_cast<std::uint32_t>(Channel);
        if (Index >= SurfaceStateChannelCount)
        {
            throw std::invalid_argument("SurfaceStateChannel is not a valid state channel.");
        }

        return static_cast<std::size_t>(Index);
    }

    void ValidateSurfaceResponseProfileData(const SurfaceResponseProfileData& Data)
    {
        for (std::size_t Index = 0; Index < Data.States.size(); ++Index)
        {
            const SurfaceStateParameters& State = Data.States[Index];
            const std::string             Prefix = "states." + std::string(SurfaceStateChannelNames[Index].Name) + ".";

            if (!std::isfinite(State.StateCapacity) || State.StateCapacity <= 0.0F)
            {
                throw std::invalid_argument(Prefix + "stateCapacity must be finite and greater than zero.");
            }

            ValidateFiniteNonNegative(State.InputFactor, Prefix + "inputFactor");
            ValidateFiniteNonNegative(State.SaturationTransferRate, Prefix + "saturationTransferRate");
            ValidateFiniteNonNegative(State.GeometryTransferRate, Prefix + "geometryTransferRate");
            ValidateFiniteNonNegative(State.DecayRate, Prefix + "decayRate");
            ValidateUnitInterval(State.CavityRetentionFactor, Prefix + "cavityRetentionFactor");
            ValidateFiniteNonNegative(State.AccumulationFactor, Prefix + "accumulationFactor");
            ValidateUnitInterval(State.CavityFillFactor, Prefix + "cavityFillFactor");
        }

        for (std::size_t Index = 0; Index < Data.Transitions.size(); ++Index)
        {
            const SurfaceStateTransition& Transition = Data.Transitions[Index];
            const std::string             Prefix = "transitions[" + std::to_string(Index) + "].";

            (void)GetSurfaceStateChannelName(Transition.Source);
            (void)GetSurfaceStateChannelName(Transition.Target);
            if (Transition.Source == Transition.Target)
            {
                throw std::invalid_argument(Prefix + "source and target must be different channels.");
            }

            ValidateUnitInterval(Transition.Threshold, Prefix + "threshold");
            ValidateFiniteNonNegative(Transition.TransitionRate, Prefix + "transitionRate");
        }
    }
} // namespace MDSS
