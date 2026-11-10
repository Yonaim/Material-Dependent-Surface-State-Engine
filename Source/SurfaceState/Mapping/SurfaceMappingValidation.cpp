/**
 * @file SurfaceMappingValidation.cpp
 * @brief Surface mapping 결과의 texel graph 불변조건 검증.
 */

#include "SurfaceState/Mapping/SurfaceMappingValidation.h"

#include <array>
#include <stdexcept>
#include <string>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Mapping Validation Helpers
// ============================================================
#pragma region 00_Mapping_Validation_Helpers
// mapping의 sentinel·범위·연결 규칙을 검사할 공통 판단을 제공한다.

    namespace
    {
        bool ContainsNeighbor(const FSurfaceMappingTexel& Texel, FLocalTexelIndex Neighbor)
        {
            for (const FLocalTexelIndex Candidate : Texel.Neighbors)
            {
                if (Candidate == Neighbor)
                {
                    return true;
                }
            }

            return false;
        }
    } // namespace
#pragma endregion

// ============================================================
// 01. Surface Mapping Validation
// ============================================================
#pragma region 01_Surface_Mapping_Validation
// 생성한 texel·이웃·seam의 불변식을 검사해 진단을 반환한다.

    void ValidateSurfaceMapping(const FSurfaceMappingData& Mapping)
    {
        std::size_t ExpectedFirstTexel = 0;

        for (std::size_t SurfaceIndex = 0; SurfaceIndex < Mapping.Surfaces.size(); ++SurfaceIndex)
        {
            const FSurfaceTexelRange& Surface = Mapping.Surfaces[SurfaceIndex];

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
            const FSurfaceMappingTexel&                        Texel = Mapping.Texels[Index];
            std::array<FLocalTexelIndex, SurfaceNeighborCount> Seen{};
            std::size_t                                        SeenCount = 0;

            for (const FLocalTexelIndex Neighbor : Texel.Neighbors)
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

                if (!ContainsNeighbor(Mapping.Texels[Neighbor], static_cast<FLocalTexelIndex>(Index)))
                {
                    throw std::invalid_argument("Surface mapping neighbor relationships must be bidirectional.");
                }
            }
        }
    }
#pragma endregion
} // namespace MDSS::SurfaceState
