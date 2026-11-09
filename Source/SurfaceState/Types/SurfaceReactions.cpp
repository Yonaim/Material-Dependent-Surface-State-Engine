#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <cmath>
#include <set>
#include <stdexcept>

namespace MDSS::SurfaceState
{
    void ValidateSurfaceReactions(const FSurfaceResponseProfileData& Data)
    {
        std::set<std::string> IDs;

        for (const auto& R : Data.Reactions)
        {
            const std::string P = "reactions." + R.ID + ".";
            auto              Require = [&](bool bValid, const std::string& Field)
            {
                if (!bValid)
                    throw std::invalid_argument(P + Field + " is invalid.");
            };
            auto State = [&](const std::string& Name)
            { Require(Data.States.contains(Name), "State '" + Name + "' must be supported by this Profile"); };
            Require(!R.ID.empty() && IDs.insert(R.ID).second, "id must be nonempty and unique");
            Require(R.Mode <= EReactionMode::InflowFraction, "mode");
            Require(R.RateModel <= EReactionRateModel::MassAction, "rate.model");
            Require(!R.Consumes.empty() || !R.Produces.empty(), "consumes/produces");
            auto Terms = [&](const auto& Values)
            {
                std::set<std::string> Seen;

                for (const auto& T : Values)
                {
                    State(T.State);
                    Require(Seen.insert(T.State).second, "duplicate State term");
                    Require(std::isfinite(T.Coefficient) && T.Coefficient > 0, "coefficient");
                }
            };
            Terms(R.Consumes);
            Terms(R.Produces);
            Require(std::isfinite(R.RateCoefficient) && R.RateCoefficient >= 0, "rate.coefficient");

            if (R.Mode == EReactionMode::Continuous && R.RateModel == EReactionRateModel::StateProportional)
            {
                State(R.RateState);
                bool bFound = false;

                for (const auto& T : R.Consumes)
                    bFound |= T.State == R.RateState;
                Require(bFound, "rate.state must be consumed");
            }

            if (R.RateModel == EReactionRateModel::MassAction)
                Require(!R.Factors.empty(), "rate.factors");
            for (const auto& F : R.Factors)
            {
                State(F.State);
                Require(std::isfinite(F.Reference) && F.Reference > 0, "factor.reference");
                Require(std::isfinite(F.Exponent) && F.Exponent > 0, "factor.exponent");
            }

            for (const auto& M : R.Modifiers)
            {
                State(M.State);
                Require(M.Function <= EReactionFunction::Inhibit, "modifier.function");
                Require(std::isfinite(M.Threshold) && M.Threshold >= 0, "modifier.threshold");
                Require(std::isfinite(M.Scale) && M.Scale > 0, "modifier.scale");
            }

            if (R.Mode == EReactionMode::InflowFraction)
            {
                State(R.InflowState);
                bool bFound = false;

                for (const auto& T : R.Consumes)
                    bFound |= T.State == R.InflowState;
                Require(bFound, "inflowState must be consumed");
                Require(R.InflowSources > 0 && R.InflowSources <= 3, "inflowSources");
                Require(std::isfinite(R.Fraction) && R.Fraction >= 0 && R.Fraction <= 1, "fraction");
                Require(R.Modifiers.empty(), "inflowFraction does not accept modifiers");
            }
        }
    }
}
