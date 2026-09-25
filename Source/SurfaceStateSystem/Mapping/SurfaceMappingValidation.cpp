/**
 * @file SurfaceMappingValidation.cpp
 * @brief Surface mapping 결과의 texel graph 불변조건 검증.
 */

#include "SurfaceStateSystem/Mapping/SurfaceMappingValidation.h"

#include <array>
#include <stdexcept>
#include <string>

namespace MDSS
{
    namespace
    {
        bool ContainsNeighbor(const TSurfaceMappingTexel& Texel, TLocalTexelIndex Neighbor)
        {
            for (const TLocalTexelIndex Candidate : Texel.Neighbors)
            {
                if (Candidate == Neighbor)
                {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    void ValidateSurfaceMapping(const TSurfaceMappingData& Mapping)
    {
        std::size_t ExpectedFirstTexel = 0;
        for (std::size_t SurfaceIndex = 0; SurfaceIndex < Mapping.Surfaces.size(); ++SurfaceIndex)
        {
            const TSurfaceTexelRange& Surface = Mapping.Surfaces[SurfaceIndex];
            if (Surface.Surface != SurfaceIndex || Surface.FirstTexel != ExpectedFirstTexel ||
                Surface.TexelCount != Surface.Resolution.GetTexelCount())
            {
                throw std::invalid_argument("Surface mapping ranges must be dense, ordered, and contiguous.");
            }
            ExpectedFirstTexel += Surface.TexelCount;
        }
        if (ExpectedFirstTexel != Mapping.Texels.size())
        {
            throw std::invalid_argument("Surface mapping range size does not match its texel array.");
        }

        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const TSurfaceMappingTexel&                        Texel = Mapping.Texels[Index];
            std::array<TLocalTexelIndex, SurfaceNeighborCount> Seen{};
            std::size_t                                       SeenCount = 0;

            for (const TLocalTexelIndex Neighbor : Texel.Neighbors)
            {
                if (Neighbor == InvalidTexelIndex)
                {
                    continue;
                }
                if (!Texel.IsValid())
                {
                    throw std::invalid_argument("Invalid texel " + std::to_string(Index) + " has a neighbor.");
                }
                if (Neighbor >= Mapping.Texels.size())
                {
                    throw std::invalid_argument("Texel neighbor index is outside the mapping range.");
                }
                if (Neighbor == Index)
                {
                    throw std::invalid_argument("A texel cannot be its own neighbor.");
                }
                if (!Mapping.Texels[Neighbor].IsValid())
                {
                    throw std::invalid_argument("A valid texel references an invalid neighbor.");
                }
                for (std::size_t SeenIndex = 0; SeenIndex < SeenCount; ++SeenIndex)
                {
                    if (Seen[SeenIndex] == Neighbor)
                    {
                        throw std::invalid_argument("A texel contains a duplicate neighbor.");
                    }
                }
                Seen[SeenCount++] = Neighbor;

                if (!ContainsNeighbor(Mapping.Texels[Neighbor], static_cast<TLocalTexelIndex>(Index)))
                {
                    throw std::invalid_argument("Surface mapping neighbor relationships must be bidirectional.");
                }
            }
        }
    }
} // namespace MDSS
