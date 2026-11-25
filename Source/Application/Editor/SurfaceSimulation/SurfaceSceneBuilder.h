/**
 * @file SurfaceSceneBuilder.h
 * @brief Surface Simulation의 장면 설정과 시간에 따른 입력을 정의한다.
 */

#pragma once

#include "Rendering/Surface/StateVisualization.h"
#include "Rendering/Surface/SurfaceSimulationAppearance.h"
#include "SurfaceState/Geometry/HeightfieldSources.h"
#include "SurfaceState/Simulation/SurfaceSimulationState.h"
#include <array>
#include <glm/vec4.hpp>
#include <optional>
#include <string_view>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Scene Catalog
    // ============================================================
    #pragma region 00_Scene_Catalog
        // UI 표시 이름과 CLI 키를 같은 장면 ID에 대응시킨다.

        enum class EScene : int
        {
            Runoff,
            Textile,
            Charring,
            Terrace,
            Compound,
            Relief,
            IceBed,
            Cavity,
            FreezeThaw,
            Meadow,
            IceGate,
            SoilPath,
            AbsorptionComparison
        };

        inline constexpr std::array<const char *, 13> SceneNames{"Grooved brick",
                                                                 "Snow on Fabric",
                                                                 "Charring wood",
                                                                 "Terraced catchment",
                                                                 "Heated Water Channel",
                                                                 "Canyon cascade",
                                                                 "Glacier cut",
                                                                 "Cavity fill & spill",
                                                                 "Freeze and thaw",
                                                                 "Meadow Firebreak",
                                                                 "Ice gate fork",
                                                                 "Soil-covered stone path",
                                                                 "Substrate Absorption"};
        inline constexpr std::array<const char *, 13> SceneKeys{"runoff",
                                                                "textile",
                                                                "charring",
                                                                "terrace",
                                                                "kiln",
                                                                "cascade",
                                                                "ice",
                                                                "cavity",
                                                                "freeze-thaw",
                                                                "meadow",
                                                                "ice-gate",
                                                                "soil-path",
                                                                "absorption-comparison"};
        inline constexpr std::array<const char *, 13> ScenePurpose{
            "Check cavity storage, tilt and sill crossing.",
            "See the weave through the ice, then track melting, absorption and drying.",
            "Watch wood warm up, ignite, and retain char as flame activity fades.",
            "Check side-to-side transport and retention in a 2D grid.",
            "Watch runoff cool incandescent steel and generate steam at the contact area.",
            "Follow runoff from a high ledge through three pools. Use Elevation contours to read the relief.",
            "Follow liquid water through a channel carved into a solid, optically thick slab of ice.",
            "Fill a pocket described by a height map and watch it spill over the lowest notch.",
            "Follow a sealed mass of water as it freezes, rises against the marks, and melts back down.",
            "Follow a wet band across a grassy slope and watch how it redirects the flame front.",
            "Watch the shortcut open as the ice gate melts.",
            "Watch soil become mud, accumulate in a channel, and affect the second pour.",
            "Compare water retained on flat sponge and steel after equal doses."};

    #pragma endregion

    // ============================================================
    // 01. Scene Inputs and Runtime Data
    // ============================================================
    #pragma region 01_Scene_Inputs_and_Runtime_Data
        // 장면 설정, 생성된 형상 및 표시 카메라에 쓰는 값을 모은다.

        struct FInteractionBinding
        {
            bool operator==(const FInteractionBinding &) const = default;

            std::string Rule;
            std::string From;
            std::string To;
            std::string Field;
            std::string SceneField;
        };

        /** @brief PhysicalMaterial에서 읽은 바닥 외관이다. 수송과 반응 규칙은 프로파일이 따로 연결한다. */
        struct FSubstrateAppearanceSettings
        {
            bool operator==(const FSubstrateAppearanceSettings &) const = default;

            std::string                      Id;
            Rendering::FSubstrateOptics      Optics;
            Rendering::FAbsorbedWetnessDepth WetnessDepth;
            std::filesystem::path            ColorTexture;
            std::filesystem::path            NormalTexture;
            bool                             bMetallic = false;
        };

        // 저작 자산의 연결과 현재 세션에 적용할 물리량을 함께 보관한다.
        struct FSurfaceProfileSettings
        {
            bool operator==(const FSurfaceProfileSettings &) const = default;

            FSubstrateAppearanceSettings  Substrate;
            std::string                   LiquidMaterial;
            std::string                   LiquidState;
            std::string                   IceState;
            std::string                   AbsorbedState;
            Liquid::FLiquidMaterial       Liquid{};
            Liquid::FCellUpdateParameters Cell;
            FInteractionBinding           Melt;
            FInteractionBinding           Absorption;
            FInteractionBinding           Drying;
            float                         DryingSplitV = 0.F;
            float                         DryingLow = 1.F;
            float                         DryingHigh = 1.F;
            float                         WetClothReferenceKgM2 = 0.F;
            float                         LiquidOpticalDepthScale = 1.F;
            float                         LiquidCoverageDepthMeters = Rendering::DefaultLiquidCoverageDepthMeters;
        };

        struct FProceduralUpdateRates
        {
            bool operator==(const FProceduralUpdateRates &) const = default;

            float MeltKgM2SecondK = 0.F;
            float FreezeKgM2SecondK = 0.F;
            float EvaporateKgM2SecondK = 0.F;
            float CharPerSecond = 0.F;
        };

        struct FShallowBasin
        {
            bool operator==(const FShallowBasin &) const = default;

            std::array<float, 2> CenterUV;
            float                RadiusMeters;
            float                DepthMeters;
        };

        struct FSurfaceRelief
        {
            bool operator==(const FSurfaceRelief &) const = default;

            float                AmplitudeMeters;
            std::array<float, 2> Cycles;
        };

        struct FSubstrateQuenchSettings
        {
            std::array<float, 2>     CenterUV;
            std::array<float, 2>     RadiusUV;
            float                    PeakTemperatureK;
            Liquid::FSubstrateQuench Material;
        };

        struct FFilmPatch
        {
            int                  Layer;
            std::array<float, 4> Bounds;
            float                ThicknessM;
        };

        struct FTemperatureRamp
        {
            std::array<float, 2> TargetK;
            float                RiseSeconds;
            bool                 bFromEnergyPulse = false;
        };

        inline constexpr std::array GeometrySamplesPerCellPresets{1, 2, 4, 8};
        inline constexpr std::array PlaybackRates{0.05F, 0.1F, 0.25F, 0.5F, 1.F, 2.F, 4.F};

        struct FSurfaceSceneSettings
        {
            EScene                                      Scene = EScene::Runoff;
            int                                         ShapeColumns = 9;
            int                                         ShapeRows = 5;
            int                                         GridColumns = 3;
            int                                         GridRows = 1;
            int                                         GeometrySamplesPerCell = 0; ///< 0은 전체 형상 해상도 직접 지정이다.
            SurfaceState::EHeightfieldSource            Source = SurfaceState::EHeightfieldSource::Procedural;
            std::filesystem::path                       AssetPath;
            std::filesystem::path                       Path;
            std::filesystem::path                       LiquidMaterialPath;
            std::filesystem::path                       SurfaceTexturePath;
            std::filesystem::path                       SurfaceNormalPath;
            Rendering::FEnvironmentReflection           Environment;
            std::filesystem::path                       ChartAlbedoPath;
            std::filesystem::path                       ChartNormalPath;
            std::optional<float>                        BodyThicknessMeters;
            std::optional<float>                        WetnessRedistributionStartSeconds;
            Rendering::FSurfaceSectionWindow            SectionWindow;
            bool                                        bStartPaused = false;
            std::optional<float>                        InitialPlaybackRate;
            std::string                                 DefaultSubstrateViewState;
            float                                       Scale = 0.006F;
            float                                       HeightOffsetM = 0.F;
            int                                         BaseFilter = 80;
            int                                         FineFilter = 0;
            bool                                        bBaseOnly = false;
            bool                                        bSmoothDisplay = false;
            std::string                                 ProfileId;
            std::optional<FSurfaceProfileSettings>      Profile;
            FSubstrateAppearanceSettings                ProceduralSubstrate;
            Rendering::FSnowSurfaceAppearance           Snow;
            std::vector<FSurfaceProfileSettings>        ComparisonProfiles;
            float                                       ComparisonGapMeters = 0.F;
            std::optional<Liquid::FLiquidMaterial>      LiquidOverride;
            std::optional<FProceduralUpdateRates>       ProceduralUpdateOverride;
            std::vector<Rendering::FStateVisualization> States;
            std::optional<bool>                         OpenOutletOverride;
            bool                                        bSplitSubgridBasins = true;
            bool                                        bTransport = true;
            int                                         TransportSubsteps = 1;
            bool                                        bLocalUpdates = true;
            bool                                        bExternalExchange = true;
            std::array<float, 2>                        SizeM{0.09F, 0.05F};
            std::optional<Liquid::FInletFootprint>      ScheduledInlet;
            std::optional<FShallowBasin>                ShallowBasin;
            std::optional<FSurfaceRelief>               SurfaceRelief;
            std::optional<FSubstrateQuenchSettings>     SubstrateQuench;
            std::optional<FTemperatureRamp>             TemperatureRamp;
            bool                                        bSubstrateThermalEmission = false;
            glm::vec4                                   LiquidTint{0.F};
            std::vector<Liquid::FFilmLayer>             FilmLayers;
            std::vector<Liquid::FFilmReaction>          FilmReactions;
            std::vector<FFilmPatch>                     FilmPatches;
            bool                                        bFilmBedCoupling = true;
            int                                         FilmIceLayer = -1;
            int                                         FilmOmittedSupport = -1;
            float                                       SecondPourStart = 0.F;
            float                                       BranchRiseM = 0.0025F;
            float                                       DepositionBasinDepthM = 0.004F;
            float                FilmWaterCoverageDepthM = Rendering::DefaultLiquidCoverageDepthMeters;
            float                PourKg = 0.F;
            float                PourStartSeconds = 0.F;
            float                PourSeconds = 2.F;
            float                ContactHeatFadeSeconds = 3.2F;
            float                IceMassScale = 1.F;
            float                MaxTiltRadians = 0.F;
            bool                 bOpenOutlet = false;
            float                InitialLiquidKg = 0.F;
            float                InitialIcePeakKgM2 = 0.F;
            std::array<float, 2> InitialIceCenter{};
            float                InitialIceInverseRadius = 0.F;

            [[nodiscard]] int SpecimenCount() const noexcept
            {
                return 1 + static_cast<int>(ComparisonProfiles.size());
            }
        };

        struct FSurfaceSceneRuntime
        {
            Liquid::FSurfaceSimulationDomain        Geometry;
            float                                   PourKg = 0.F;
            float                                   MaxTilt = 0.F;
            Rendering::FSurfaceSimulationAppearance Appearance;
            float                                   PourSeconds = 2.F;
            EScene                                  Kind = EScene::Runoff;
            float                                   ContactHeatFadeSeconds = 3.2F;
            float                                   SecondPourStart = 0.F;
            float                                   TemperatureRiseSeconds = 0.F;
            float                                   PourStartSeconds = 0.F;
        };

        struct FDemoCamera
        {
            float Azimuth = 0.F;
            float Elevation = 0.F;
            float DistanceScale = 1.F;
        };

    #pragma endregion

    // ============================================================
    // 02. Scene Setup and Schedule
    // ============================================================
    #pragma region 02_Scene_Setup_and_Schedule
        // 장면을 생성한 뒤 같은 경과 시간으로 주입과 기울기 및 온도 입력을 재현한다.

        /** @brief 에디터와 CLI가 공유하는 데모의 .Scene 설정을 읽는다. */
        [[nodiscard]] FSurfaceSceneSettings Defaults(EScene Scene);
        /** @brief 씬에서 읽은 카탈로그의 형상 세트 ID를 선택한다. */
        /** @brief 선택한 형상 세트의 파일과 변환 배율을 적용한다. */
        /** @brief 씬의 지정 온도장과 함께 사용하는 국소 갱신 계수의 기본값을 반환한다. */
        [[nodiscard]] FProceduralUpdateRates DefaultProceduralUpdateRates(EScene Scene);
        [[nodiscard]] FSurfaceSceneRuntime   MakeScene(const FSurfaceSceneSettings &Settings);
        /** @brief 경과 시간 Time(s)의 고정 스텝에 주입할 질량(kg)을 반환한다. */
        [[nodiscard]] float ScheduledInput(const FSurfaceSceneRuntime &Scene, float Time);
        /** @brief 경과 시간 Time(s)에 적용할 표면 기울기(rad)를 반환한다. */
        [[nodiscard]] float ScheduledAngle(const FSurfaceSceneRuntime &Scene, float Time);
        /** @brief 경과 시간 Time(s)에 적용할 지정 온도의 보간 비율과 오프셋(K)을 반환한다. */
        [[nodiscard]] Liquid::FThermalInput ScheduledTemperature(const FSurfaceSceneRuntime &Scene, float Time);
        [[nodiscard]] FDemoCamera           ScheduledCamera(EScene Scene, float Time);

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
