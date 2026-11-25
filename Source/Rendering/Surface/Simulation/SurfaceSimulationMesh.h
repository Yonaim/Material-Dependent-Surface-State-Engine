/**
 * @file SurfaceSimulationMesh.h
 * @brief Surface Simulation의 표시 메시와 셰이더가 읽는 형상 필드를 정의한다.
 */

#pragma once

#include "Rendering/Surface/StateVisualization.h"
#include "Rendering/Surface/SurfaceSimulationAppearance.h"
#include "SurfaceState/Simulation/SurfaceSimulationState.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Display Mesh
    // ============================================================
    #pragma region 00_Display_Mesh
        // 화면 레이어별 인덱스 범위와 정점에서 읽을 계산 셀을 보존한다.

        struct FSurfaceSimulationVertex
        {
            glm::vec3     Position;
            glm::vec3     Normal;
            std::uint32_t Cell;
            std::uint32_t Region;
        };

        struct FSurfaceSimulationMesh
        {
            std::vector<FSurfaceSimulationVertex> Vertices;
            std::vector<std::uint32_t>            Indices;
            std::uint32_t                         TopIndexCount = 0;
            std::uint32_t                         SolidIndexCount = 0;
            std::uint32_t                         EffectIndexCount = 0;
            std::uint32_t                         BackdropIndexCount = 0;
            std::uint32_t                         SectionIndexCount = 0;
        };

    #pragma endregion

    // ============================================================
    // 01. Shader Geometry Field
    // ============================================================
    #pragma region 01_Shader_Geometry_Field
        // SurfaceDisplayField.glsl의 std430 버퍼와 같은 순서로 차트 좌표와 높이를 제공한다.

        /** @brief SurfaceDisplayField.glsl이 set 0 / binding 4에서 읽는 로컬 차트 정보다. */
        struct alignas(16) FSurfaceRenderMetadata
        {
            glm::vec4                   BoundsMin{};
            glm::vec4                   BoundsMax{};
            glm::vec4                   SampleLayout{}; // 차트 너비·길이와 형상 표본 간격 [m]
            std::array<std::int32_t, 4> Counts{};       // 형상 W/H, 계산 W/H
            glm::vec4                   BedMetrics{};   // 높이 최솟값·최댓값·밑면 [m], 얼음 바닥 여부
            glm::vec4                   IceProperties{};
            std::array<int, 12>         StateChannels{};
            glm::ivec4                  StateGroups{};
            std::array<glm::vec4, 12>   StateVisuals{};
            std::array<float, 12>       StateThicknessInverseMeters{};
            glm::vec4                   LiquidOptics{};
            glm::vec4                   SubstrateOptics{};
            glm::vec2                   IceOptics{};
            float                       SubstrateReliefMeters = 0.F;
            float                       DisplayGapMeters = 0.F;
            glm::vec2                   PhaseLevels{}; // 액상과 고체의 기준 높이 [m]
            float                       CharContrast = 0.F;
            int                         RegionCount = 0;
            glm::vec2                   EnvironmentOptics{}; // 선형 노출 배율, 회전 [rad]
            std::uint32_t               SpecimenCount = 1;
            std::uint32_t               MetallicSpecimens = 0;
            glm::vec4                   LiquidTint{};
            glm::vec4                   SectionWindow{};        // 단면 Y, 창 높이, 눈금 간격, 유리 두께 [m]
            glm::vec4                   VesselBounds{};         // 표시용 유리 용기 내부의 로컬 XY 경계 [m]
            glm::vec4                   SectionSolidBed{};      // x/y는 단면 돌 받침의 로컬 X 구간 [m]
            glm::vec4                   AbsorbedWetnessDepth{}; // 초기 대비, 역 시간 척도 [1/s], 시작 시각 [s], 예약값
        };

        struct FSurfaceRenderField : FSurfaceRenderMetadata
        {
            std::vector<float>         CenterX;
            std::vector<float>         CenterY;
            std::vector<float>         Heights;
            std::vector<float>         EdgeX;
            std::vector<float>         EdgeY;
            std::vector<int>           CellRegionCounts;
            std::vector<std::uint32_t> SampleRegions;
            std::vector<float>         RegionInverseAreas;
            std::vector<int>           ContactIndices;
            std::vector<float>         CellMaxBedHeights;
            int                        FilmDisplayWord = -1;
            int                        ReactionContactWord = -1;
            std::vector<float>         AbsorbCapacities;
            int                        FilmIceLayer = -1;
            std::vector<float>         FilmDensities;
        };

        /** @brief 정적 표시 헤더와 실제 크기의 배열을 하나의 SSBO로 묶는다. */
        [[nodiscard]] std::vector<std::byte> PackSurfaceRenderField(const FSurfaceRenderField &Field);

        /** @brief 셰이더가 읽는 바닥 형상, 계산 셀 좌표, 효과 영역을 만든다. */
        [[nodiscard]] FSurfaceRenderField MakeSurfaceRenderField(const Liquid::FSurfaceSimulationDomain        &Geometry,
                                                                 const Rendering::FSurfaceSimulationAppearance &Appearance);

        /** @brief 샘플 구역의 바닥, 수면 드로우 범위, 상태를 읽을 이펙트 정점을 만든다. */
        [[nodiscard]] FSurfaceSimulationMesh MakeSurfaceSimulationMesh(const Liquid::FSurfaceSimulationDomain &Geometry,
                                                                       const FSurfaceRenderField              &Field,
                                                                       bool  bSmoothDisplay,
                                                                       float ThicknessMeters);

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
