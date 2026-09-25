/**
 * @file SurfaceStateRegistry.cpp
 * @brief Deterministic runtime State IDs built from loaded SRProfiles.
 */

#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

namespace MDSS
{
    SurfaceStateRegistry::SurfaceStateRegistry(const std::vector<SurfaceResponseProfileData>& Profiles)
    {
        std::set<std::string> StateNames;
        for (const SurfaceResponseProfileData& Profile : Profiles)
        {
            ValidateSurfaceResponseProfileData(Profile);
            for (const auto& Entry : Profile.States)
            {
                StateNames.insert(Entry.first);
            }
        }

        if (StateNames.size() >= static_cast<std::size_t>(InvalidStateId))
        {
            throw std::overflow_error("SurfaceStateRegistry exhausted the StateId range.");
        }

        Names.assign(StateNames.begin(), StateNames.end());
        IDs.reserve(Names.size());
        for (std::size_t Index = 0; Index < Names.size(); ++Index)
        {
            IDs.emplace(Names[Index], static_cast<StateId>(Index));
        }

        for (const SurfaceResponseProfileData& Profile : Profiles)
        {
            for (const SurfaceStateTransition& Transition : Profile.Transitions)
            {
                if (!IDs.contains(Transition.Source))
                {
                    throw std::invalid_argument("Transition references unknown source State '" + Transition.Source +
                                                "'.");
                }
                if (!IDs.contains(Transition.Target))
                {
                    throw std::invalid_argument("Transition references unknown target State '" + Transition.Target +
                                                "'.");
                }
            }
        }
    }

    std::size_t SurfaceStateRegistry::GetStateCount() const noexcept
    {
        return Names.size();
    }

    StateId SurfaceStateRegistry::GetStateId(std::string_view Name) const
    {
        const std::string CanonicalName = NormalizeSurfaceStateName(Name);
        const auto        Found = IDs.find(CanonicalName);
        if (Found == IDs.end())
        {
            throw std::out_of_range("Unknown State name: '" + CanonicalName + "'.");
        }
        return Found->second;
    }

    const std::string& SurfaceStateRegistry::GetStateName(StateId ID) const
    {
        if (ID >= Names.size())
        {
            throw std::out_of_range("Invalid StateId.");
        }
        return Names[ID];
    }

    RegisteredSurfaceResponseProfileData
    SurfaceStateRegistry::ResolveProfile(const SurfaceResponseProfileData& Profile) const
    {
        ValidateSurfaceResponseProfileData(Profile);

        RegisteredSurfaceResponseProfileData Result;
        Result.States.resize(Names.size());
        for (const auto& [Name, Parameters] : Profile.States)
        {
            const auto Found = IDs.find(Name);
            if (Found == IDs.end())
            {
                throw std::invalid_argument("Profile contains State not registered in this registry: '" + Name + "'.");
            }
            Result.States[Found->second] = Parameters;
        }

        Result.Transitions.reserve(Profile.Transitions.size());
        for (const SurfaceStateTransition& Transition : Profile.Transitions)
        {
            const auto Source = IDs.find(Transition.Source);
            const auto Target = IDs.find(Transition.Target);
            if (Source == IDs.end() || Target == IDs.end())
            {
                throw std::invalid_argument("Profile transition references a State absent from this registry.");
            }
            Result.Transitions.push_back(
                {Source->second, Target->second, Transition.Threshold, Transition.TransitionRate});
        }
        return Result;
    }
} // namespace MDSS
