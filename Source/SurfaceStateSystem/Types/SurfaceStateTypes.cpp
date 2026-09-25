/**
 * @file SurfaceStateTypes.cpp
 * @brief Data-driven state profile validation and name normalization.
 */

#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace MDSS
{
    namespace
    {
        void ValidateFiniteNonNegative(float Value, std::string_view FieldName)
        {
            if (!std::isfinite(Value) || Value < 0.0F)
            {
                throw std::invalid_argument(std::string(FieldName) + " must be finite and non-negative.");
            }
        }

        void ValidateUnitInterval(float Value, std::string_view FieldName)
        {
            if (!std::isfinite(Value) || Value < 0.0F || Value > 1.0F)
            {
                throw std::invalid_argument(std::string(FieldName) + " must be finite and in [0, 1].");
            }
        }
    } // namespace

    std::string NormalizeSurfaceStateName(std::string_view Name)
    {
        const auto IsAsciiWhitespace = [](unsigned char Character)
        { return Character == ' ' || (Character >= '\t' && Character <= '\r'); };
        std::size_t First = 0;
        while (First < Name.size() && IsAsciiWhitespace(static_cast<unsigned char>(Name[First])))
        {
            ++First;
        }

        std::size_t Last = Name.size();
        while (Last > First && IsAsciiWhitespace(static_cast<unsigned char>(Name[Last - 1])))
        {
            --Last;
        }

        std::string Result(Name.substr(First, Last - First));
        for (char& Character : Result)
        {
            if (Character >= 'A' && Character <= 'Z')
            {
                Character = static_cast<char>(Character - 'A' + 'a');
            }
        }
        return Result;
    }

    void ValidateSurfaceResponseProfileData(const TSurfaceResponseProfileData& Data)
    {
        for (const auto& [Name, State] : Data.States)
        {
            if (Name.empty() || NormalizeSurfaceStateName(Name) != Name)
            {
                throw std::invalid_argument("State names must be non-empty and normalized.");
            }

            const std::string Prefix = "states." + Name + ".";
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
            const TSurfaceStateTransition& Transition = Data.Transitions[Index];
            const std::string             Prefix = "transitions[" + std::to_string(Index) + "].";
            if (Transition.Source.empty() || NormalizeSurfaceStateName(Transition.Source) != Transition.Source)
            {
                throw std::invalid_argument(Prefix + "source must be non-empty and normalized.");
            }
            if (Transition.Target.empty() || NormalizeSurfaceStateName(Transition.Target) != Transition.Target)
            {
                throw std::invalid_argument(Prefix + "target must be non-empty and normalized.");
            }
            if (Transition.Source == Transition.Target)
            {
                throw std::invalid_argument(Prefix + "source and target must be different states.");
            }

            ValidateUnitInterval(Transition.Threshold, Prefix + "threshold");
            ValidateFiniteNonNegative(Transition.TransitionRate, Prefix + "transitionRate");
        }
    }
} // namespace MDSS
