/**
 * @file SurfaceInstanceStateData.h
 * @brief Surface instance별 상태값과 Surface-to-Profile 연결.
 */

#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace MDSS
{
    /**
     * @brief Shared geometry를 참조하고 Instance별 texel State를 소유한다.
     * @note 각 Surface에는 profile index가 하나씩 연결된다.
     */
    class SurfaceInstanceStateData
    {
    public:
        /**
         * @brief Instance 상태를 생성하고 모든 채널을 0으로 초기화한다.
         * @param ID 유효한 Instance 식별자.
         * @param Geometry 공유 Surface geometry. 수명 동안 유지될 객체를 전달한다.
         * @param SurfaceProfileIndices Surface ID 순서에 맞춘 Profile index 목록.
         * @throws std::invalid_argument ID, geometry 또는 Surface/Profile 대응이 잘못된 경우.
         */
        SurfaceInstanceStateData(SurfaceInstanceID                                ID,
                                 std::shared_ptr<const SharedSurfaceGeometryData> Geometry,
                                 std::vector<SurfaceProfileIndex>                 SurfaceProfileIndices);

        [[nodiscard]] SurfaceInstanceID                       GetID() const noexcept;
        [[nodiscard]] const SharedSurfaceGeometryData&        GetGeometry() const noexcept;
        [[nodiscard]] const std::vector<SurfaceProfileIndex>& GetSurfaceProfileIndices() const noexcept;
        [[nodiscard]] const std::vector<SurfaceStateValues>&  GetStates() const noexcept;
        /** @brief Solver가 Instance별 상태 채널을 갱신할 mutable state 배열을 반환한다. */
        [[nodiscard]] std::vector<SurfaceStateValues>& GetStates() noexcept;
        /**
         * @brief 지정한 Surface에 연결된 Profile index를 조회한다.
         * @throws std::out_of_range Surface ID가 geometry에 없는 경우.
         */
        [[nodiscard]] SurfaceProfileIndex GetProfileIndex(SurfaceLocalID Surface) const;

    private:
        SurfaceInstanceID                                ID = InvalidSurfaceInstanceID;
        std::shared_ptr<const SharedSurfaceGeometryData> Geometry;
        std::vector<SurfaceProfileIndex>                 SurfaceProfileIndices;
        std::vector<SurfaceStateValues>                  States;
    };
} // namespace MDSS
