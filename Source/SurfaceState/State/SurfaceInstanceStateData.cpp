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
// ============================================================
// 00. Instance State Construction
// ============================================================
#pragma region 00_Instance_State_Construction

    FSurfaceInstanceStateData::FSurfaceInstanceStateData(FSurfaceInstanceID                                ID,
                                                         std::shared_ptr<const FSharedSurfaceGeometryData> Geometry,
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

        States.resize(this->Geometry->GetTexelCount(), FSurfaceStateValues(StateCount, 0.0F));
    }
#pragma endregion

// ============================================================
// 01. Instance State Accessors
// ============================================================
#pragma region 01_Instance_State_Accessors

    FSurfaceInstanceID FSurfaceInstanceStateData::GetID() const noexcept
    {
        return ID;
    }

    const FSharedSurfaceGeometryData& FSurfaceInstanceStateData::GetGeometry() const noexcept
    {
        return *Geometry;
    }

    std::size_t FSurfaceInstanceStateData::GetStateCount() const noexcept
    {
        return States.empty() ? 0 : States.front().size();
    }

    const std::vector<FSurfaceStateValues>& FSurfaceInstanceStateData::GetStates() const noexcept
    {
        return States;
    }

    std::vector<FSurfaceStateValues>& FSurfaceInstanceStateData::GetStates() noexcept
    {
        return States;
    }

    FSurfaceProfileIndex FSurfaceInstanceStateData::GetProfileIndex(FLocalTexelIndex Texel) const
    {
        return Geometry->GetProfileIndex(Texel);
    }
#pragma endregion
} // namespace MDSS::SurfaceState
