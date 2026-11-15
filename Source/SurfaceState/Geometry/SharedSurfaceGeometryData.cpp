/**
 * @file SharedSurfaceGeometryData.cpp
 * @brief mesh 공유 Surface 정의와 texel geometry 데이터 계약.
 */

#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Resolution and Texel Validation
// ============================================================
#pragma region 00_Resolution_and_Texel_Validation
// 해상도와 texel 범위를 확인해 형상 자료의 유효성을 검증한다.

    std::size_t FSurfaceResolution::GetTexelCount() const
    {
        if (Width == 0 || Height == 0)
        {
            throw std::invalid_argument("Surface resolution dimensions must be greater than zero.");
        }

        const std::uint64_t TexelCount = static_cast<std::uint64_t>(Width) * Height;

        if (TexelCount > std::numeric_limits<FLocalTexelIndex>::max())
        {
            throw std::overflow_error("Surface resolution exceeds the supported local texel index range.");
        }

        return static_cast<std::size_t>(TexelCount);
    }

    bool FSurfaceTexelGeometry::IsValid() const noexcept
    {
        return Surface != InvalidSurfaceID && Triangle != InvalidTriangleID;
    }
#pragma endregion

// ============================================================
// 01. Shared Geometry Data
// ============================================================
#pragma region 01_Shared_Geometry_Data
// 여러 instance가 공유할 texel·topology·형상 자료를 보관한다.

    FSharedSurfaceGeometryData::FSharedSurfaceGeometryData(std::vector<FSurfaceDefinition> SurfaceDefinitions)
    {
        if (SurfaceDefinitions.empty())
        {
            throw std::invalid_argument("TSharedSurfaceGeometryData requires at least one Surface.");
        }

        std::uint64_t FirstTexel = 0;

        for (const FSurfaceDefinition& Definition : SurfaceDefinitions)
        {
            if (Definition.ID == InvalidSurfaceID)
            {
                throw std::invalid_argument("InvalidSurfaceID is reserved and cannot identify a Surface.");
            }

            if (Definition.ID != Surfaces.size())
            {
                throw std::invalid_argument("TSurfaceLocalID values must be dense and ordered from zero.");
            }

            const std::size_t SurfaceTexelCount = Definition.Resolution.GetTexelCount();
            FirstTexel += SurfaceTexelCount;

            if (FirstTexel > std::numeric_limits<FLocalTexelIndex>::max())
            {
                throw std::overflow_error("Combined Surface resolution exceeds the local texel index range.");
            }

            Surfaces.push_back({Definition.ID,
                                Definition.Resolution,
                                static_cast<FLocalTexelIndex>(FirstTexel - SurfaceTexelCount),
                                static_cast<FLocalTexelIndex>(SurfaceTexelCount)});
        }

        Texels.resize(static_cast<std::size_t>(FirstTexel));
        ProfileMap.resize(Texels.size(), InvalidSurfaceProfileIndex);
    }

    FSharedSurfaceGeometryData::FSharedSurfaceGeometryData(std::vector<FSurfaceDefinition>   SurfaceDefinitions,
                                                           std::vector<FSurfaceProfileIndex> ProfileIndices)
        : FSharedSurfaceGeometryData(std::move(SurfaceDefinitions))
    {
        SetProfileMap(std::move(ProfileIndices));
    }

    const std::vector<FSurfaceTexelRange>& FSharedSurfaceGeometryData::GetSurfaces() const noexcept
    {
        return Surfaces;
    }

    const std::vector<FSurfaceTexelGeometry>& FSharedSurfaceGeometryData::GetTexels() const noexcept
    {
        return Texels;
    }

    std::vector<FSurfaceTexelGeometry>& FSharedSurfaceGeometryData::GetTexels() noexcept
    {
        return Texels;
    }

    const std::vector<FSurfaceProfileIndex>& FSharedSurfaceGeometryData::GetProfileMap() const noexcept
    {
        return ProfileMap;
    }

    void FSharedSurfaceGeometryData::SetProfileMap(std::vector<FSurfaceProfileIndex> NewProfileMap)
    {
        if (NewProfileMap.size() != Texels.size())
        {
            throw std::invalid_argument("SurfaceProfileMap must contain one entry per texel.");
        }

        for (std::size_t Index = 0; Index < Texels.size(); ++Index)
        {
            const bool bValidTexel = Texels[Index].IsValid();
            const bool bHasProfile = NewProfileMap[Index] != InvalidSurfaceProfileIndex;

            if (!bValidTexel && bHasProfile)
            {
                throw std::invalid_argument("Invalid texels must use InvalidSurfaceProfileIndex.");
            }
        }

        ProfileMap = std::move(NewProfileMap);
    }

    FSurfaceProfileIndex FSharedSurfaceGeometryData::GetProfileIndex(FLocalTexelIndex Texel) const
    {
        if (Texel >= ProfileMap.size())
        {
            throw std::out_of_range("Texel index is not present in SurfaceProfileMap.");
        }

        return ProfileMap[Texel];
    }

    std::size_t FSharedSurfaceGeometryData::GetTexelCount() const noexcept
    {
        return Texels.size();
    }

    const FSurfaceTexelRange& FSharedSurfaceGeometryData::GetSurface(FSurfaceLocalID Surface) const
    {
        for (const FSurfaceTexelRange& Range : Surfaces)
        {
            if (Range.Surface == Surface)
            {
                return Range;
            }
        }

        throw std::out_of_range("TSurfaceLocalID is not present in TSharedSurfaceGeometryData.");
    }
#pragma endregion
} // namespace MDSS::SurfaceState
