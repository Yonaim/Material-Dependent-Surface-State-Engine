/**
 * @file SharedSurfaceGeometryData.cpp
 * @brief mesh 공유 Surface 정의와 texel geometry 데이터 계약.
 */

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    std::size_t SurfaceResolution::GetTexelCount() const
    {
        if (Width == 0 || Height == 0)
        {
            throw std::invalid_argument("Surface resolution dimensions must be greater than zero.");
        }

        const std::uint64_t TexelCount = static_cast<std::uint64_t>(Width) * Height;
        if (TexelCount > std::numeric_limits<LocalTexelIndex>::max())
        {
            throw std::overflow_error("Surface resolution exceeds the supported local texel index range.");
        }

        return static_cast<std::size_t>(TexelCount);
    }

    bool SurfaceTexelGeometry::IsValid() const noexcept
    {
        return Surface != InvalidSurfaceID && Triangle != InvalidTriangleID;
    }

    SharedSurfaceGeometryData::SharedSurfaceGeometryData(std::vector<SurfaceDefinition> SurfaceDefinitions)
    {
        if (SurfaceDefinitions.empty())
        {
            throw std::invalid_argument("SharedSurfaceGeometryData requires at least one Surface.");
        }

        std::uint64_t FirstTexel = 0;

        for (const SurfaceDefinition& Definition : SurfaceDefinitions)
        {
            if (Definition.ID == InvalidSurfaceID)
            {
                throw std::invalid_argument("InvalidSurfaceID is reserved and cannot identify a Surface.");
            }
            if (Definition.ID != Surfaces.size())
            {
                throw std::invalid_argument("SurfaceLocalID values must be dense and ordered from zero.");
            }
            const std::size_t SurfaceTexelCount = Definition.Resolution.GetTexelCount();
            FirstTexel += SurfaceTexelCount;
            if (FirstTexel > std::numeric_limits<LocalTexelIndex>::max())
            {
                throw std::overflow_error("Combined Surface resolution exceeds the local texel index range.");
            }

            Surfaces.push_back({Definition.ID,
                                Definition.Resolution,
                                static_cast<LocalTexelIndex>(FirstTexel - SurfaceTexelCount),
                                static_cast<LocalTexelIndex>(SurfaceTexelCount)});
        }

        Texels.resize(static_cast<std::size_t>(FirstTexel));
        ProfileMap.resize(Texels.size(), InvalidSurfaceProfileIndex);
    }

    SharedSurfaceGeometryData::SharedSurfaceGeometryData(std::vector<SurfaceDefinition>   SurfaceDefinitions,
                                                         std::vector<SurfaceProfileIndex> ProfileIndices)
        : SharedSurfaceGeometryData(std::move(SurfaceDefinitions))
    {
        SetProfileMap(std::move(ProfileIndices));
    }

    const std::vector<SurfaceTexelRange>& SharedSurfaceGeometryData::GetSurfaces() const noexcept
    {
        return Surfaces;
    }

    const std::vector<SurfaceTexelGeometry>& SharedSurfaceGeometryData::GetTexels() const noexcept
    {
        return Texels;
    }

    std::vector<SurfaceTexelGeometry>& SharedSurfaceGeometryData::GetTexels() noexcept
    {
        return Texels;
    }

    const std::vector<SurfaceProfileIndex>& SharedSurfaceGeometryData::GetProfileMap() const noexcept
    {
        return ProfileMap;
    }

    void SharedSurfaceGeometryData::SetProfileMap(std::vector<SurfaceProfileIndex> NewProfileMap)
    {
        if (NewProfileMap.size() != Texels.size())
        {
            throw std::invalid_argument("SurfaceProfileMap must contain one entry per texel.");
        }
        for (std::size_t Index = 0; Index < Texels.size(); ++Index)
        {
            const bool bValidTexel = Texels[Index].IsValid();
            const bool bHasProfile = NewProfileMap[Index] != InvalidSurfaceProfileIndex;
            if (bValidTexel != bHasProfile)
            {
                throw std::invalid_argument(bValidTexel ? "Valid texels require a Profile index."
                                                        : "Invalid texels must use InvalidSurfaceProfileIndex.");
            }
        }
        ProfileMap = std::move(NewProfileMap);
    }

    SurfaceProfileIndex SharedSurfaceGeometryData::GetProfileIndex(LocalTexelIndex Texel) const
    {
        if (Texel >= ProfileMap.size())
        {
            throw std::out_of_range("Texel index is not present in SurfaceProfileMap.");
        }
        return ProfileMap[Texel];
    }

    std::size_t SharedSurfaceGeometryData::GetTexelCount() const noexcept
    {
        return Texels.size();
    }

    const SurfaceTexelRange& SharedSurfaceGeometryData::GetSurface(SurfaceLocalID Surface) const
    {
        for (const SurfaceTexelRange& Range : Surfaces)
        {
            if (Range.Surface == Surface)
            {
                return Range;
            }
        }

        throw std::out_of_range("SurfaceLocalID is not present in SharedSurfaceGeometryData.");
    }
} // namespace MDSS
