/**
 * @file SurfaceSimulationDomain.h
 * @brief 표면 시뮬레이션의 격자, 저장 영역 및 기하 계산 API를 정의한다.
 */

#pragma once

#include "SurfaceState/Geometry/Heightfield.h"
#include "SurfaceState/Liquid/LiquidMaterial.h"
#include "SurfaceState/Simulation/SurfaceGridLimits.h"
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace MDSS::Liquid
{
    // ============================================================
    // 00. Simulation Limits
    // ============================================================
    #pragma region 00_Simulation_Limits
        // 형상 표본과 계산 셀은 독립적인 축당 상한을 가진다.

        inline constexpr int MaxSamplesPerAxis = MDSS_SURFACE_MAX_SAMPLE_AXIS;
        inline constexpr int MaxCellsPerAxis = MDSS_SURFACE_MAX_CELL_AXIS;
        inline constexpr int MaxSamples = MaxSamplesPerAxis * MaxSamplesPerAxis;
        inline constexpr int MaxCells = MaxCellsPerAxis * MaxCellsPerAxis;
        inline constexpr int MaxRegions = MaxSamples;
        // 모든 표본의 +X/+Y 경계와 +X 외곽을 합친 상한이다.
        inline constexpr int   MaxContacts = 2 * MaxSamples;
        inline constexpr int   MaxFaces = MaxContacts;
        inline constexpr float AmbientTemperatureK = 293.15F;
        inline constexpr float Gravity = 9.81F;
        inline constexpr float StepSeconds = 1.F / 240.F;
        inline constexpr float TiltRadians = 0.08726646F;
    #pragma endregion

    // ============================================================
    // 01. Domain Topology and Parameters
    // ============================================================
    #pragma region 01_Domain_Topology_and_Parameters
        // 저장 영역과 수송 면 및 장면 구성에 필요한 계수를 정의한다.

        /** @brief 구간 끝을 제외하는 열과 행 인덱스로 계산 셀이 덮는 샘플을 지정한다. */
        struct alignas(16) FSimulationCell
        {
            std::int32_t FirstColumn, EndColumn, FirstRow, EndRow;
        };

        /** @brief 두 저장 영역과 접촉 표본 범위. Right가 음수이면 열린 외곽이다. */
        struct alignas(16) FTransportFace
        {
            std::int32_t Left, Right, FirstContact, ContactCount;
        };

        /** @brief 연속 배열에 놓인 표본·면·영역 범위를 참조한다. */
        struct FIndexRange
        {
            int First = 0;
            int Count = 0;

            template <typename T>
            [[nodiscard]] std::span<const T> In(const std::vector<T> &Values) const
            {
                return std::span(Values).subspan(First, Count);
            }
        };

        /** @brief 한 계산 셀에 속하는 독립 액체 저장 영역. */
        struct alignas(16) FStorageRegion
        {
            int Cell = 0;
            int FirstSample = 0;
            int SampleCount = 0;
            int FirstFace = 0;
        };

        /** @brief 기존 셀 단위 수위와 분지 단위 수송을 대조하는 검증 설정. */
        enum class EStorageRegionMode
        {
            SubgridBasins,
            SinglePerCell
        };

        struct alignas(16) FCellUpdateParameters
        {
            bool operator==(const FCellUpdateParameters &) const = default;

            float AbsorbPerSecond = 0.F;
            float DryPerSecond = 0.F;
            float CapacityKgM2 = 0.F;
            float CharPerSecond = 0.F;
            float TemperatureK = AmbientTemperatureK;
            float MeltKgM2SecondK = 0.F;
            float EvaporateKgM2SecondK = 0.F;
            float FreezeKgM2SecondK = 0.F;
        };

        /** @brief 공통 박막식의 한 층. 마찰은 로드 시 tan(안식각)으로 변환한다. */
        struct FFilmLayer
        {
            float                DensityKgM3;
            float                ViscosityPaS;
            float                YieldStressPa;
            float                Friction;
            std::array<float, 3> Color;
            float                DryViscosityMultiplier = 1.F;
            float                DryYieldStressPa = 0.F;
            float                WetWaterFraction = 0.F;
        };

        /** @brief MassKg = rate × projected area × dt. Melt는 빙점 초과 온도를 곱한다. */
        struct FFilmReaction
        {
            int   From; // 변환 전의 상 또는 혼합할 흙
            int   To;   // 변환 후의 상 또는 진흙
            int   Water;
            float Rate;
            float WaterPerSoil;
            float EquilibriumTemperatureK;
            float RewetKgM2Second = 0.F;
            float DryPerSecond = 0.F;
            float RewetStartS = 0.F;
            float TemperatureDirection = 1.F; // 평형 온도 위에서 용융, 아래에서 응고를 구동한다.
        };

        /** @brief 차트 중심을 기준으로 저작한 예약 급수 사각형 [m]. */
        struct FInletFootprint
        {
            std::array<float, 2> CenterM{};
            std::array<float, 2> SizeM{};
        };

        /** @brief 뜨거운 바닥과 물 사이의 면적당 열용량과 유효 접촉 열전달 계수다. */
        struct FSubstrateQuench
        {
            float ArealHeatCapacityJm2K = 0.F;
            float ContactConductanceWm2K = 0.F;
        };

        // 20°C 물의 현열과 대기압에서의 잠열을 기화 질량당 한 값으로 묶는다.
        inline constexpr float WaterVaporizationJPerKg = 2.591e6F;
        inline constexpr float ThermalContactDepthM = 0.00025F;

        struct FSurfaceSimulationDomain
        {
            FHeightfield                 Shape;
            std::vector<FSimulationCell> Cells;
            std::vector<FTransportFace>  Faces;
            std::vector<FStorageRegion>  Regions;
            // 층별 계산에만 쓰는 중력축 높이. 비어 있으면 원래 바닥을 사용한다.
            std::vector<float>       RegionBedLevels;
            std::vector<FIndexRange> CellRegions;
            std::vector<int>         RegionSamples;
            // 접촉의 양의 방향은 하위 비트, 경계 앞쪽 형상 표본은 나머지 비트에 저장한다.
            std::vector<int>                   Contacts;
            std::vector<int>                   RegionFaces;
            std::vector<int>                   FaceCounts;
            std::vector<std::uint32_t>         SampleRegions;
            std::vector<FCellUpdateParameters> Materials;
            FLiquidMaterial                    Liquid;
            std::vector<float>                 InitialIceKgM2;
            std::vector<std::uint32_t>         SampleCells;
            std::vector<float>                 ScheduledInletWeights; // 비어 있으면 SourceSample의 영역에 전량 주입한다.
            std::vector<FFilmLayer>            FilmLayers;
            std::vector<FFilmReaction>         FilmReactions;
            std::vector<float>                 InitialFilmKg;
            bool                               bFilmBedCoupling = true;
            float                              FilmBedFreezeStart = 0.F;
            int                                FilmIceLayer = -1;
            int                                FilmOmittedSupport = -1;
            int                                SourceSample = 0;
            float                              InitialLiquidKg = 0.F;
            float                              InitialTemperatureWeight = 1.F;
            FSubstrateQuench                   SubstrateQuench;
            // 가열된 흡수수도 표층의 기화량 한도에 포함한다.
            bool bEvaporateAbsorbedWater = false;
            bool bTransport = true;
            int  TransportSubsteps = 1;
            bool bExternalExchange = true;
            int  CombustionGridColumns = 0; // 0이면 지정 온도를 사용한다. 양수는 행 우선 격자의 열 수다.

            [[nodiscard]] int SourceCell() const
            {
                return static_cast<int>(SampleCells[SourceSample]);
            }
            [[nodiscard]] int SourceRegion() const
            {
                return static_cast<int>(SampleRegions[SourceSample]);
            }
            [[nodiscard]] FIndexRange RegionFaceRange(int Region) const
            {
                return {Regions[Region].FirstFace, FaceCounts[Region]};
            }
        };

        /** @brief A procedural profile uses the same film solver with one layer from its liquid material. */
        [[nodiscard]] inline FFilmLayer PrimaryFilmMaterial(const FLiquidMaterial &Material)
        {
            return {Material.DensityKgM3, Material.DynamicViscosityPaS, Material.YieldStressPa, Material.Friction, {}};
        }

        struct FSurfaceTilt
        {
            float Sine = 0.F;
            float Cosine = 1.F;
        };
    #pragma endregion

    // ============================================================
    // 02. Grid and Capacity
    // ============================================================
    #pragma region 02_Grid_and_Capacity
        // 표면 격자를 구성하고 질량과 저장 수위의 관계를 계산한다.

        /**
         * @brief 샘플 축을 0부터 Samples까지 덮는 정수 절단점을 반환한다.
         * @throws std::invalid_argument 해상도가 최대치 또는 1 ≤ Cells ≤ Samples 조건을 벗어나는 경우.
         */
        [[nodiscard]] std::vector<int> Partition(int Samples, int Cells);
        /**
         * @brief 절단점에 따라 형상 샘플을 계산 셀로 묶고 셀 사이의 공유 면을 만든다.
         * @param CutsX 첫 값이 0이고 마지막 값이 Shape.Columns인 엄격히 증가하는 열 경계.
         * @param CutsY 첫 값이 0이고 마지막 값이 Shape.Rows인 엄격히 증가하는 행 경계.
         * @param bOpenRight 참이면 +X 쪽 외곽 면으로 액체가 유출될 수 있다.
         * @param ClosedColumns 형상 표본 열에 지정한 경계를 넘는 수송을 막는다. 각 열은 내부 계산 셀 경계여야 한다.
         * @throws std::invalid_argument 차트의 크기, 높이 또는 절단점이 유효하지 않은 경우.
         */
        [[nodiscard]] FSurfaceSimulationDomain MakeGeometry(FHeightfield         Shape,
                                                            std::span<const int> CutsX,
                                                            std::span<const int> CutsY,
                                                            bool                 bOpenRight,
                                                            FLiquidMaterial      Liquid,
                                                            EStorageRegionMode Storage = EStorageRegionMode::SubgridBasins,
                                                            std::span<const int> ClosedColumns = {});
        /** @brief 차트와 겹친 급수 면적을 액체 저장 영역의 정규화한 질량 배분율로 변환한다. */
        void                       SetScheduledInlet(FSurfaceSimulationDomain &Geometry, const FInletFootprint &Footprint);
        [[nodiscard]] FSurfaceTilt MakePose(float Radians);
        [[nodiscard]] double       CellArea(const FSurfaceSimulationDomain &Geometry, int Cell);
        /** @brief 표본의 잠긴 부분을 적분해서 수위 Level [m] 아래의 셀 부피 [m³]를 구한다. */
        [[nodiscard]] double
        VolumeAtLevel(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceTilt Pose, double Level);
        /** @brief 접촉 양쪽의 바닥을 중력 방향에 투영하여 통과 수위 [m]를 구한다. */
        [[nodiscard]] double
        SillElevation(const FSurfaceSimulationDomain &Geometry, FTransportFace Face, int Contact, FSurfaceTilt Pose);
        [[nodiscard]] double
        RegionVolume(const FSurfaceSimulationDomain &Geometry, int Region, FSurfaceTilt Pose, double Level);
        /** @brief 차트 투영 접촉 면적 [m²]. DepthBand [m]가 양수이면 얇은 물막의 전도율을 선형으로 줄인다. */
        [[nodiscard]] double RegionWetArea(const FSurfaceSimulationDomain &Geometry,
                                           int                             Region,
                                           FSurfaceTilt                    Pose,
                                           double                          Level,
                                           double                          DepthBand = 0.0);
        /** @brief 셀 질량(kg)에서 수위(m)를 복원한다. 질량이 0 이하면 가장 낮은 바닥 높이를 반환한다. */
        [[nodiscard]] float
        RestoreLevel(const FSurfaceSimulationDomain &Geometry, int Cell, FSurfaceTilt Pose, double Mass);
        /** @brief 저장 영역의 질량(kg)에서 수위(m)를 복원한다. */
        [[nodiscard]] float
        RestoreRegionLevel(const FSurfaceSimulationDomain &Geometry, int Region, FSurfaceTilt Pose, double Mass);
    #pragma endregion

} // namespace MDSS::Liquid
