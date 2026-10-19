/**
 * @file SurfaceInstanceStateData.cpp
 * @brief Surface instance별 상태값과 Surface-to-Profile 연결.
 */

#include "SurfaceState/State/SurfaceInstanceStateData.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace MDSS::SurfaceState
{
#pragma region Instance_State_Construction

    TSurfaceInstanceStateData::TSurfaceInstanceStateData(TSurfaceInstanceID                                ID,
                                                         std::shared_ptr<const TSharedSurfaceGeometryData> Geometry,
                                                         std::size_t                                       StateCount)
        : ID(ID), Geometry(std::move(Geometry))
    {
        if (ID == InvalidSurfaceInstanceID)
        {
            throw std::invalid_argument("InvalidSurfaceInstanceID is reserved.");
        }
        if (!this->Geometry)
        {
            throw std::invalid_argument("TSurfaceInstanceStateData requires TSharedSurfaceGeometryData.");
        }
        States.resize(this->Geometry->GetTexelCount(), TSurfaceStateValues(StateCount, 0.0F));
    }
#pragma endregion

#pragma region Instance_State_Accessors

    TSurfaceInstanceID TSurfaceInstanceStateData::GetID() const noexcept
    {
        return ID;
    }

    const TSharedSurfaceGeometryData& TSurfaceInstanceStateData::GetGeometry() const noexcept
    {
        return *Geometry;
    }

    std::size_t TSurfaceInstanceStateData::GetStateCount() const noexcept
    {
        return States.empty() ? 0 : States.front().size();
    }

    const std::vector<TSurfaceStateValues>& TSurfaceInstanceStateData::GetStates() const noexcept
    {
        return States;
    }

    std::vector<TSurfaceStateValues>& TSurfaceInstanceStateData::GetStates() noexcept
    {
        return States;
    }

    TSurfaceProfileIndex TSurfaceInstanceStateData::GetProfileIndex(TLocalTexelIndex Texel) const
    {
        return Geometry->GetProfileIndex(Texel);
    }
#pragma endregion
} // namespace MDSS::SurfaceState
