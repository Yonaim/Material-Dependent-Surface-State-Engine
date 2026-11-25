/**
 * @file SurfaceSimulationAppearance.h
 * @brief Surface Simulation 장면의 공용 표면 외관과 실험 전용 표시 옵션을 묶는다.
 */

#pragma once

#include "Rendering/Surface/SurfaceAppearance.h"
#include <filesystem>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace MDSS::Rendering
{
    inline constexpr float DefaultLiquidCoverageDepthMeters = 0.00085F;

    /** @brief 월드 방향과 선형 밝기에 대응하는 2:1 HDR 반사 파노라마다. */
    struct FEnvironmentReflection
    {
        std::filesystem::path TexturePath;
        float                 Exposure = 1.F;
        float                 YawRadians = 0.F;
    };

    inline constexpr int MaxSurfaceSpecimens = 8;

    /** @brief 시편의 PBR 맵은 시뮬레이션에 쓰이는 공통 형상과 독립이다. */
    struct FSurfaceSpecimenAppearance
    {
        std::filesystem::path ColorTexture;
        std::filesystem::path NormalTexture;
        bool                  bMetallic = false;
    };

    /** @brief 시뮬레이션을 유지하며 앞 벽을 투명 단면으로 표시한다. */
    struct FSurfaceSectionWindow
    {
        float     FrontV = 0.F;       // 음의 Y쪽에서 시작하는 차트 좌표 [0, 1]
        float     HeightMeters = 0.F; // 기준 바닥부터 창의 위쪽까지 [m], 0이면 비활성
        float     GraduationMeters = 0.F;
        glm::vec4 VesselInteriorUV{}; // [uMin, vMin, uMax, vMax]
        float     GlassThicknessMeters = 0.F;
        glm::vec2 SolidBedRangeU{}; // 차트의 선택 구간에서 높은 바닥 단면을 불투명하게 표시한다.
    };

    /** @brief 흡수수의 평균 포화도에서 깊이별 젖음을 그리는 데 쓰는 값이다. */
    struct FAbsorbedWetnessDepth
    {
        bool operator==(const FAbsorbedWetnessDepth &) const = default;

        float InitialContrast = 0.F;
        float DiffusivityM2S = 0.F;
        float RedistributionStartSeconds = 0.F;
    };

    /** @brief 눈 표층의 타일형 PBR 맵과 실제 길이 기준 매핑을 지정한다. */
    struct FSnowSurfaceAppearance
    {
        bool operator==(const FSnowSurfaceAppearance &) const = default;

        std::filesystem::path ColorTexture;
        std::filesystem::path NormalTexture;
        /** @brief 양수이면 kg/m²를 눈의 표시 두께로 환산하는 벌크 밀도 [kg/m³]다. */
        float BulkDensityKgM3 = 0.F;
        float RepeatsPerMeter = 0.F;
    };

    /** @brief 바닥 메시의 표시 두께와 얼음 및 가열 표현을 설정한다. */
    struct FSurfaceSimulationAppearance
    {
        FSurfaceAppearance Surface;
        float              DisplayThicknessMeters = 0.008F;
        bool               bIceSubstrate = false;
        bool               bHeaterLighting = false;
        /** @brief 양수이면 천 음영이 최대에 도달하는 흡수수 면밀도(kg/m²)다. */
        float WetClothReferenceKgM2 = 0.F;
        /** @brief 고체 물 표층과 접촉 열원의 용암 침착 형상을 표시한다. */
        bool bLavaDeposit = false;
        /** @brief 굴절 수면의 색에만 적용하는 광학 경로 배율이다. */
        float LiquidOpticalDepthScale = 1.F;
        /** @brief 보간한 수면이 완전히 합성되는 깊이(m)다. */
        float LiquidCoverageDepthMeters = DefaultLiquidCoverageDepthMeters;
        /** @brief 바닥 전체에 금속 반사 모델을 적용한다. */
        bool bMetalSubstrate = false;
        /** @brief RGB 텍스처를 높이장과 같은 UV 범위에 한 번 투영한다. */
        bool bChartAlbedo = false;
        /** @brief 탄화층의 흑화·무광 대비를 강화하는 렌더링 배율이다. 범위는 0–1이다. */
        float                  CharContrast = 0.F;
        FEnvironmentReflection Environment;
        /** @brief 이웃 시편이 화면에서 떨어지는 거리 [m]. */
        float                                   DisplayGapMeters = 0.F;
        int                                     SpecimenCount = 1;
        std::vector<FSurfaceSpecimenAppearance> Specimens;
        bool                                    bIncandescentSubstrate = false;
        glm::vec4                               LiquidTint{0.F}; // 알파 0은 기본 액체 광학을 유지한다.
        FSurfaceSectionWindow                   SectionWindow;
        FAbsorbedWetnessDepth                   AbsorbedWetnessDepth;
        FSnowSurfaceAppearance                  Snow;
    };
} // namespace MDSS::Rendering
