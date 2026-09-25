/**
 * @file SurfaceInstanceStateData.cpp
 * @brief Surface instance별 상태값과 Surface-to-Profile 연결.
 */

#include "SurfaceStateSystem/SurfaceInstanceStateData.h"

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    SurfaceInstanceStateData::SurfaceInstanceStateData(SurfaceInstanceID                                ID,
                                                       std::shared_ptr<const SharedSurfaceGeometryData> Geometry,
                                                       std::vector<SurfaceProfileIndex> SurfaceProfileIndices)
        : ID(ID), Geometry(std::move(Geometry)), SurfaceProfileIndices(std::move(SurfaceProfileIndices))
    {
        if (ID == InvalidSurfaceInstanceID)
        {
            throw std::invalid_argument("InvalidSurfaceInstanceID is reserved.");
        }
        if (!this->Geometry)
        {
            throw std::invalid_argument("SurfaceInstanceStateData requires SharedSurfaceGeometryData.");
        }
        if (this->SurfaceProfileIndices.size() != this->Geometry->GetSurfaces().size())
        {
            throw std::invalid_argument("Every Surface must have exactly one SRProfile index.");
        }
        if (std::ranges::find(this->SurfaceProfileIndices, InvalidSurfaceProfileIndex) !=
            this->SurfaceProfileIndices.end())
        {
            throw std::invalid_argument("InvalidSurfaceProfileIndex is reserved.");
        }

        States.resize(this->Geometry->GetTexelCount());
    }

    SurfaceInstanceID SurfaceInstanceStateData::GetID() const noexcept
    {
        return ID;
    }

    const SharedSurfaceGeometryData& SurfaceInstanceStateData::GetGeometry() const noexcept
    {
        return *Geometry;
    }

    const std::vector<SurfaceProfileIndex>& SurfaceInstanceStateData::GetSurfaceProfileIndices() const noexcept
    {
        return SurfaceProfileIndices;
    }

    const std::vector<SurfaceStateValues>& SurfaceInstanceStateData::GetStates() const noexcept
    {
        return States;
    }

    std::vector<SurfaceStateValues>& SurfaceInstanceStateData::GetStates() noexcept
    {
        return States;
    }

    SurfaceProfileIndex SurfaceInstanceStateData::GetProfileIndex(SurfaceLocalID Surface) const
    {
        const std::vector<SurfaceTexelRange>& Surfaces = Geometry->GetSurfaces();
        const auto                            Found = std::ranges::find(Surfaces, Surface, &SurfaceTexelRange::Surface);
        if (Found == Surfaces.end())
        {
            throw std::out_of_range("SurfaceLocalID is not present in SurfaceInstanceStateData.");
        }

        const std::size_t SurfaceIndex = static_cast<std::size_t>(std::distance(Surfaces.begin(), Found));
        return SurfaceProfileIndices[SurfaceIndex];
    }
} // namespace MDSS
