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
     * @note State channel count는 Profile에서 구축한 registry의 크기다.
     */
    class TSurfaceInstanceStateData
    {
    public:
        /**
         * @brief Instance 상태를 생성하고 모든 채널을 0으로 초기화한다.
         * @param ID 유효한 Instance 식별자.
         * @param Geometry 공유 Surface geometry. 수명 동안 유지될 객체를 전달한다.
         * @param StateCount Registry가 결정한 동적 State 채널 수.
         * @throws std::invalid_argument ID, geometry 또는 Surface/Profile 대응이 잘못된 경우.
         */
        TSurfaceInstanceStateData(TSurfaceInstanceID                                ID,
                                 std::shared_ptr<const TSharedSurfaceGeometryData> Geometry,
                                 std::size_t                                      StateCount);

        [[nodiscard]] TSurfaceInstanceID                      GetID() const noexcept;
        [[nodiscard]] const TSharedSurfaceGeometryData&       GetGeometry() const noexcept;
        [[nodiscard]] std::size_t                            GetStateCount() const noexcept;
        [[nodiscard]] const std::vector<TSurfaceStateValues>& GetStates() const noexcept;
        /** @brief Solver가 Instance별 상태 채널을 갱신할 mutable state 배열을 반환한다. */
        [[nodiscard]] std::vector<TSurfaceStateValues>& GetStates() noexcept;
        /**
         * @brief texel의 정적 SurfaceProfileMap index를 조회한다.
         * @throws std::out_of_range texel index가 geometry에 없는 경우.
         */
        [[nodiscard]] TSurfaceProfileIndex GetProfileIndex(TLocalTexelIndex Texel) const;

    private:
        TSurfaceInstanceID                                ID = InvalidSurfaceInstanceID;
        std::shared_ptr<const TSharedSurfaceGeometryData> Geometry;
        std::vector<TSurfaceStateValues>                  States;
    };
} // namespace MDSS
