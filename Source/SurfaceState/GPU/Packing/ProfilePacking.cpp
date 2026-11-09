#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Profile and Buffer Layouts
// ============================================================
#pragma region 00_Profile_and_Buffer_Layouts

    TSurfaceGPUProfileUpload PackSurfaceProfiles(const std::vector<TSurfaceResponseProfileData>& Profiles,
                                                 const TSurfaceStateRegistry&                    Registry)
    {
        if (Profiles.empty())
        {
            throw std::invalid_argument("Cannot pack an empty Surface Profile table.");
        }
        const std::size_t ChannelCount = Registry.GetStateCount();
        if (ChannelCount == 0)
        {
            throw std::invalid_argument("Cannot pack Surface Profiles without registered State channels.");
        }
        if (Profiles.size() > std::numeric_limits<std::size_t>::max() / ChannelCount)
        {
            throw std::overflow_error("Surface Profile table element count overflowed.");
        }

        const std::size_t        RecordCount = Profiles.size() * ChannelCount;
        TSurfaceGPUProfileUpload Result;
        Result.Parameters.resize(RecordCount);
        Result.Supported.resize(RecordCount, 0U);

        for (std::size_t ProfileIndex = 0; ProfileIndex < Profiles.size(); ++ProfileIndex)
        {
            const TRegisteredSurfaceResponseProfileData Resolved = Registry.ResolveProfile(Profiles[ProfileIndex]);
            for (std::size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
            {
                const std::size_t RecordIndex = ProfileIndex * ChannelCount + ChannelIndex;
                if (!Resolved.States[ChannelIndex].has_value())
                {
                    continue;
                }

                const TSurfaceStateParameters& Parameters = *Resolved.States[ChannelIndex];
                Result.Parameters[RecordIndex] = {
                    {Parameters.StateCapacity,
                     Parameters.InputFactor,
                     Parameters.SaturationTransferFactor,
                     Parameters.GeometryTransferFactor},
                    {Parameters.DecayRate,
                     Parameters.CavityRetentionFactor,
                     Parameters.AccumulationFactor,
                     Parameters.CavityFillFactor},
                    {Parameters.ThicknessPerAmount, Parameters.CavityTransportRetentionFactor, 0.0F, 0.0F}};
                Result.Supported[RecordIndex] = 1U;
            }
        }

        // Word-addressed ABI: header[profile count, max rules, reserved, reserved],
        // profile ranges (4 words each), then 16-word rules and variable term lists.
        auto& Words = Result.Reactions;
        Words.resize(4U + Profiles.size() * 4U, 0U);
        Words[0] = static_cast<std::uint32_t>(Profiles.size());
        auto Bits = [](float V) { return std::bit_cast<std::uint32_t>(V); };
        auto Offset = [&]()
        {
            if (Words.size() > UINT32_MAX)
                throw std::overflow_error("Reaction table exceeds uint offsets.");
            return static_cast<std::uint32_t>(Words.size());
        };
        for (std::size_t P = 0; P < Profiles.size(); ++P)
        {
            auto Rules = Profiles[P].Reactions;
            for (const auto& T : Profiles[P].Transitions)
            {
                if (!Profiles[P].States.contains(T.Source) || !Profiles[P].States.contains(T.Target))
                    throw std::invalid_argument("Legacy Transition participants must be supported by their Profile.");
                TSurfaceReaction R;
                R.ID = "legacy_" + std::to_string(Rules.size());
                R.Produces.push_back({T.Target, 1.0F});
                R.RateCoefficient = T.TransitionRate;
                R.Modifiers.push_back({T.Source,
                                       TReactionFunction::Above,
                                       T.Threshold * Profiles[P].States.at(T.Source).StateCapacity,
                                       1.0F});
                Rules.push_back(R);
            }
            std::erase_if(Rules, [](const auto& R) { return !R.Enabled; });
            const auto Count = static_cast<std::uint32_t>(Rules.size());
            Result.MaxReactionCount = std::max(Result.MaxReactionCount, Count);
            Words[4U + P * 4U] = Offset();
            Words[5U + P * 4U] = Count;
            const auto Start = Offset();
            Words.resize(Words.size() + Count * 16U, 0U);
            for (std::uint32_t N = 0; N < Count; ++N)
            {
                const auto& R = Rules[N];
                const auto  B = Start + N * 16U;
                Words[B] = static_cast<std::uint32_t>(R.Mode);
                Words[B + 1] = static_cast<std::uint32_t>(R.RateModel);
                Words[B + 2] = R.RateState.empty() ? InvalidStateId : Registry.GetStateId(R.RateState);
                Words[B + 4] = Bits(R.RateCoefficient);
                Words[B + 5] = Bits(R.Fraction);
                Words[B + 6] = R.InflowSources;
                Words[B + 7] = R.InflowState.empty() ? InvalidStateId : Registry.GetStateId(R.InflowState);
                auto Terms = [&](const auto& Terms, std::uint32_t Field)
                {
                    Words[B + Field] = Offset();
                    Words[B + Field + 1] = static_cast<std::uint32_t>(Terms.size());
                    for (const auto& T : Terms)
                    {
                        Words.push_back(Registry.GetStateId(T.State));
                        Words.push_back(Bits(T.Coefficient));
                    }
                };
                Terms(R.Consumes, 8);
                Terms(R.Produces, 10);
                Words[B + 12] = Offset();
                Words[B + 13] = static_cast<std::uint32_t>(R.Modifiers.size());
                for (const auto& M : R.Modifiers)
                {
                    Words.insert(Words.end(),
                                 {Registry.GetStateId(M.State),
                                  static_cast<std::uint32_t>(M.Function),
                                  Bits(M.Threshold),
                                  Bits(M.Scale)});
                }
                Words[B + 14] = Offset();
                Words[B + 15] = static_cast<std::uint32_t>(R.Factors.size());
                for (const auto& F : R.Factors)
                    Words.insert(Words.end(), {Registry.GetStateId(F.State), Bits(F.Reference), Bits(F.Exponent)});
            }
        }
        Words[1] = Result.MaxReactionCount;
        return Result;
    }

    std::size_t GetSurfaceGPUStateValueIndex(std::size_t TexelIndex, std::size_t ChannelIndex, std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface State channel index is outside the Registry range.");
        }
        if (TexelIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface State scalar index overflowed.");
        }
        return TexelIndex * ChannelCount + ChannelIndex;
    }

    std::size_t
    GetSurfaceGPUProfileRecordIndex(std::size_t ProfileIndex, std::size_t ChannelIndex, std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface Profile channel index is outside the Registry range.");
        }
        if (ProfileIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface Profile record index overflowed.");
        }
        return ProfileIndex * ChannelCount + ChannelIndex;
    }

    std::size_t
    GetSurfaceGPUBufferByteSize(std::size_t ElementCount, std::size_t ElementStride, std::size_t MaxStorageBufferRange)
    {
        if (ElementCount == 0 || ElementStride == 0)
        {
            throw std::invalid_argument("Surface GPU buffers require non-zero element count and stride.");
        }
        if (ElementCount > std::numeric_limits<std::size_t>::max() / ElementStride)
        {
            throw std::overflow_error("Surface GPU buffer byte size overflowed.");
        }
        const std::size_t ByteSize = ElementCount * ElementStride;
        if (ByteSize > MaxStorageBufferRange)
        {
            throw std::length_error("Surface GPU buffer exceeds maxStorageBufferRange.");
        }
        return ByteSize;
    }
#pragma endregion
}
