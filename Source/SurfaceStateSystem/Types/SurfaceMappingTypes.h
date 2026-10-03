/**
 * @file SurfaceMappingTypes.h
 * @brief Surface mapping 해상도·texel·neighbor 자료형과 sentinel 값.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <limits>
#include <vector>

namespace MDSS
{
    using TSurfaceLocalID = std::uint32_t;
    using TLocalTexelIndex = std::uint32_t;

    inline constexpr TSurfaceLocalID  InvalidSurfaceID = std::numeric_limits<TSurfaceLocalID>::max();
    inline constexpr TLocalTexelIndex InvalidTexelIndex = std::numeric_limits<TLocalTexelIndex>::max();
    inline constexpr std::uint32_t    InvalidTriangleID = std::numeric_limits<std::uint32_t>::max();
    inline constexpr std::size_t      SurfaceNeighborCount = 8;
    inline constexpr std::uint32_t    SurfaceSimulationResolution = 256;
    // Fixed physical reference area; independent of the selected simulation resolution.
    inline constexpr float SurfaceStateReferenceArea = 1.0F / (256.0F * 256.0F);

    struct TSurfaceSimulationResolutionPreset
    {
        const char*   Label;
        std::uint32_t Resolution;
    };

    inline constexpr std::array<TSurfaceSimulationResolutionPreset, 3> SurfaceSimulationResolutionPresets{
        {{"Low", 128}, {"Medium", 256}, {"High", 512}}};

    [[nodiscard]] constexpr bool IsSurfaceSimulationResolution(std::uint32_t Resolution) noexcept
    {
        for (const auto& Preset : SurfaceSimulationResolutionPresets)
        {
            if (Preset.Resolution == Resolution)
                return true;
        }
        return false;
    }

    struct TSurfaceResolution
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        [[nodiscard]] bool operator==(const TSurfaceResolution&) const = default;

        /**
         * @brief 해상도의 총 texel 수를 계산한다.
         * @throws std::invalid_argument 너비 또는 높이가 0인 경우.
         * @throws std::overflow_error texel 수가 TLocalTexelIndex 범위를 넘는 경우.
         */
        [[nodiscard]] std::size_t GetTexelCount() const;
    };

    inline constexpr TSurfaceResolution DefaultSurfaceResolution{SurfaceSimulationResolution,
                                                                 SurfaceSimulationResolution};

    /** @brief 하나의 Surface ID와 해당 Surface의 simulation grid 해상도. */
    struct TSurfaceDefinition
    {
        TSurfaceLocalID    ID = InvalidSurfaceID;
        TSurfaceResolution Resolution;
    };

    /** @brief mesh-local texel 배열 안에서 한 Surface가 차지하는 연속 구간. */
    struct TSurfaceTexelRange
    {
        TSurfaceLocalID    Surface = InvalidSurfaceID;
        TSurfaceResolution Resolution;
        TLocalTexelIndex   FirstTexel = InvalidTexelIndex;
        TLocalTexelIndex   TexelCount = 0;
    };

    struct TSurfaceGeometryScalar
    {
        float MesoVirtualHeight = 0.0F;
        float ConcavityWeight = 0.0F;
        float MesoMeanCurvature = 0.0F;
        float MesoGaussianCurvature = 0.0F;
    };

    /**
     * @brief Texel별 CPU-side surface mapping과 geometry 데이터.
     * @note GPU ABI 구조체가 아니다. 업로드 layout은 별도로 정의한다.
     */
    struct TSurfaceTexelGeometry
    {
        TSurfaceLocalID Surface = InvalidSurfaceID;
        std::uint32_t   Triangle = InvalidTriangleID;
        std::uint32_t   Chart = std::numeric_limits<std::uint32_t>::max();
        glm::vec3       Barycentric{0.0F};
        glm::vec3       Position{0.0F};
        // Oriented mesh-local UV texel footprint; transforms with the linear cofactor matrix.
        glm::vec3 AreaVector{0.0F};
        glm::vec3 Normal{0.0F, 0.0F, 1.0F};
        /** @brief TransferWeight용으로 mesh-local 변환한 Normal Map normal. 없으면 기하 normal을 사용한다.
         */
        glm::vec3 TransferNormal{0.0F, 0.0F, 1.0F};
        bool      HasTransferNormal = false;
        /** @brief 높이 미분에서 구한 mesh-local 표면 normal. 없으면 TransferNormal을 사용한다. */
        glm::vec3                                          MesoNormal{0.0F, 0.0F, 1.0F};
        bool                                               HasMesoNormal = false;
        TSurfaceGeometryScalar                             Geometry;
        std::array<TLocalTexelIndex, SurfaceNeighborCount> NeighborIndices = {
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
            InvalidTexelIndex,
        };

        /** @brief Surface와 triangle sentinel이 모두 유효한지 확인한다. */
        [[nodiscard]] bool IsValid() const noexcept;
    };

} // MDSS 네임스페이스
