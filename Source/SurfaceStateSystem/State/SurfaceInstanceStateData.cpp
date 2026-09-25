/**
 * @file SurfaceInstanceStateData.cpp
 * @brief Surface instance별 상태값과 Surface-to-Profile 연결.
 */

#include "SurfaceStateSystem/State/SurfaceInstanceStateData.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    SurfaceInstanceStateData::SurfaceInstanceStateData(SurfaceInstanceID                                ID,
                                                       std::shared_ptr<const SharedSurfaceGeometryData> Geometry,
                                                       std::size_t                                      StateCount)
        : ID(ID), Geometry(std::move(Geometry))
    {
        if (ID == InvalidSurfaceInstanceID)
        {
            throw std::invalid_argument("InvalidSurfaceInstanceID is reserved.");
        }
        if (!this->Geometry)
        {
            throw std::invalid_argument("SurfaceInstanceStateData requires SharedSurfaceGeometryData.");
        }
        States.resize(this->Geometry->GetTexelCount(), SurfaceStateValues(StateCount, 0.0F));
    }

    SurfaceInstanceID SurfaceInstanceStateData::GetID() const noexcept
    {
        return ID;
    }

    const SharedSurfaceGeometryData& SurfaceInstanceStateData::GetGeometry() const noexcept
    {
        return *Geometry;
    }

    std::size_t SurfaceInstanceStateData::GetStateCount() const noexcept
    {
        return States.empty() ? 0 : States.front().size();
    }

    const std::vector<SurfaceStateValues>& SurfaceInstanceStateData::GetStates() const noexcept
    {
        return States;
    }

    std::vector<SurfaceStateValues>& SurfaceInstanceStateData::GetStates() noexcept
    {
        return States;
    }

    SurfaceProfileIndex SurfaceInstanceStateData::GetProfileIndex(LocalTexelIndex Texel) const
    {
        return Geometry->GetProfileIndex(Texel);
    }
} // namespace MDSS
