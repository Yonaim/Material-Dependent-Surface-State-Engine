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

namespace MDSS::SurfaceState
{
    using FSurfaceLocalID = std::uint32_t;
    using FLocalTexelIndex = std::uint32_t;

    inline constexpr FSurfaceLocalID  InvalidSurfaceID = std::numeric_limits<FSurfaceLocalID>::max();
    inline constexpr FLocalTexelIndex InvalidTexelIndex = std::numeric_limits<FLocalTexelIndex>::max();
    inline constexpr std::uint32_t    InvalidTriangleID = std::numeric_limits<std::uint32_t>::max();
    inline constexpr std::size_t      SurfaceNeighborCount = 8;
    inline constexpr std::uint32_t    SurfaceSimulationResolution = 256;

    // Fixed physical reference area; independent of the selected simulation resolution.
    inline constexpr float SurfaceStateReferenceArea = 1.0F / (256.0F * 256.0F);

    struct FSurfaceSimulationResolutionPreset
    {
        const char*   Label;
        std::uint32_t Resolution;
    };

    inline constexpr std::array<FSurfaceSimulationResolutionPreset, 3> SurfaceSimulationResolutionPresets{
        {{"Low", 128}, {"Medium", 256}, {"High", 512}}};

    inline constexpr std::array<FSurfaceSimulationResolutionPreset, 4> SurfaceRenderMeshResolutionPresets{
        {{"64", 64}, {"128", 128}, {"256", 256}, {"512", 512}}};

    [[nodiscard]] constexpr bool IsSurfaceRenderMeshResolution(std::uint32_t Resolution) noexcept
    {
        for (const auto& Preset : SurfaceRenderMeshResolutionPresets)
            if (Preset.Resolution == Resolution)
                return true;
        return false;
    }

    [[nodiscard]] constexpr bool IsSurfaceSimulationResolution(std::uint32_t Resolution) noexcept
    {
        for (const auto& Preset : SurfaceSimulationResolutionPresets)
        {
            if (Preset.Resolution == Resolution)
                return true;
        }

        return false;
    }

    struct FSurfaceResolution
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        [[nodiscard]] bool operator==(const FSurfaceResolution&) const = default;

        /**
         * @brief 해상도의 총 texel 수를 계산한다.
         * @throws std::invalid_argument 너비 또는 높이가 0인 경우.
         * @throws std::overflow_error texel 수가 FLocalTexelIndex 범위를 넘는 경우.
         */
        [[nodiscard]] std::size_t GetTexelCount() const;
    };

    inline constexpr FSurfaceResolution DefaultSurfaceResolution{SurfaceSimulationResolution,
                                                                 SurfaceSimulationResolution};

    /** @brief 하나의 Surface ID와 해당 Surface의 simulation grid 해상도. */
    struct FSurfaceDefinition
    {
        FSurfaceLocalID    ID = InvalidSurfaceID;
        FSurfaceResolution Resolution;
    };

    /** @brief mesh-local texel 배열 안에서 한 Surface가 차지하는 연속 구간. */
    struct FSurfaceTexelRange
    {
        FSurfaceLocalID    Surface = InvalidSurfaceID;
        FSurfaceResolution Resolution;
        FLocalTexelIndex   FirstTexel = InvalidTexelIndex;
        FLocalTexelIndex   TexelCount = 0;
    };

    struct FSurfaceGeometryScalar
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
    struct FSurfaceTexelGeometry
    {
        FSurfaceLocalID Surface = InvalidSurfaceID;
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
        bool      bHasTransferNormal = false;
        /** @brief 높이 미분에서 구한 mesh-local 표면 normal. 없으면 TransferNormal을 사용한다. */
        glm::vec3                                          MesoNormal{0.0F, 0.0F, 1.0F};
        bool                                               bHasMesoNormal = false;
        FSurfaceGeometryScalar                             Geometry;
        std::array<FLocalTexelIndex, SurfaceNeighborCount> NeighborIndices = {
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
