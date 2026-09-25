/**
 * @file SharedSurfaceGeometryData.h
 * @brief mesh 공유 Surface 정의와 texel geometry 데이터 계약.
 */

#pragma once

#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <vector>

namespace MDSS
{
    /**
     * @brief Mesh가 공유하는 Surface range와 texel geometry를 소유한다.
     * @note Surface ID는 입력 순서대로 0부터 이어지는 dense 값이어야 한다.
     */
    class SharedSurfaceGeometryData
    {
    public:
        /**
         * @brief Surface별 texel range를 만들고 공유 texel 저장 공간을 초기화한다.
         * @param Surfaces dense ID 순서로 정의된 Surface 목록.
         * @throws std::invalid_argument 목록이 비었거나 ID·해상도가 유효하지 않은 경우.
         * @throws std::overflow_error 전체 texel 수가 지원 범위를 넘는 경우.
         */
        explicit SharedSurfaceGeometryData(std::vector<SurfaceDefinition> Surfaces);
        SharedSurfaceGeometryData(std::vector<SurfaceDefinition> Surfaces, std::vector<SurfaceProfileIndex> ProfileMap);

        [[nodiscard]] const std::vector<SurfaceTexelRange>& GetSurfaces() const noexcept;
        /** @brief 전처리 코드가 채우는 mesh-local texel geometry 배열에 접근한다. */
        [[nodiscard]] const std::vector<SurfaceTexelGeometry>& GetTexels() const noexcept;
        /** @brief 전처리 단계에서 mapping 결과를 채울 mutable texel geometry 배열에 접근한다. */
        [[nodiscard]] std::vector<SurfaceTexelGeometry>&      GetTexels() noexcept;
        [[nodiscard]] const std::vector<SurfaceProfileIndex>& GetProfileMap() const noexcept;
        /** @brief Install the static texel-to-profile map after Geometry has been populated. */
        void                              SetProfileMap(std::vector<SurfaceProfileIndex> ProfileMap);
        [[nodiscard]] SurfaceProfileIndex GetProfileIndex(LocalTexelIndex Texel) const;
        [[nodiscard]] std::size_t         GetTexelCount() const noexcept;
        /**
         * @brief ID에 해당하는 Surface의 연속 texel range를 반환한다.
         * @throws std::out_of_range Surface ID가 이 Mesh에 없는 경우.
         */
        [[nodiscard]] const SurfaceTexelRange& GetSurface(SurfaceLocalID Surface) const;

    private:
        std::vector<SurfaceTexelRange>    Surfaces;
        std::vector<SurfaceTexelGeometry> Texels;
        std::vector<SurfaceProfileIndex>  ProfileMap;
    };
} // namespace MDSS
