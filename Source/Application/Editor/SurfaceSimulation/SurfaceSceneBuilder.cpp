/**
 * @file SurfaceSceneBuilder.cpp
 * @brief 시연 장면의 형상과 재질을 구성하고 시간에 따른 입력과 카메라를 결정한다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <stdexcept>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Demonstration Shapes and Materials
    // ============================================================
    #pragma region 00_Demonstration_Shapes_and_Materials
        // 장면별 표면 높이와 재질 초기값을 로컬 차트에서 정의한다.

        namespace
        {
            float SmoothStep(float Lower, float Upper, float Value)
            {
                const float T = std::clamp((Value - Lower) / (Upper - Lower), 0.F, 1.F);
                return T * T * (3.F - 2.F * T);
            }

            float ReliefBed(float U, float V)
            {
                const float Axis = 0.49F + 0.115F * std::sin(U * 6.7F - 0.45F);
                const float Side = std::abs(V - Axis);
                const float Drops = 0.018F * SmoothStep(0.20F, 0.30F, U) + 0.016F * SmoothStep(0.45F, 0.53F, U) +
                                    0.012F * SmoothStep(0.69F, 0.77F, U);
                const float Shelves = 0.052F - 0.005F * U - Drops;
                const float Wall = 0.015F * SmoothStep(0.10F, 0.32F, Side);
                const float Rock = 0.0028F * std::sin(U * 38.F + V * 22.F) * std::cos(V * 29.F - U * 6.F);
                const float Basin = -0.0018F * std::sin(13.F * U) * std::sin(13.F * U);
                return Shelves + Basin + Wall + Rock * SmoothStep(0.045F, 0.17F, Side);
            }

            float GlacialBed(float U, float V)
            {
                const float Axis = 0.52F + 0.11F * std::sin(6.0F * U - 0.52F);
                const float Side = std::abs(V - Axis);
                const float Flank = SmoothStep(0.076F, 0.265F, Side);
                const float Head =
                    0.005F + 0.042F * (1.F - SmoothStep(0.03F, 0.79F, U)) + 0.012F * SmoothStep(0.81F, 0.98F, U);
                const float Ridge =
                    0.019F + 0.0055F * std::sin(U * 26.F + V * 7.F) + 0.0030F * std::sin(U * 53.F + V * 37.F);
                const float Strata = 0.0012F * std::sin(U * 29.F - 0.7F) * std::cos(V * 10.F);
                return Head + (Ridge + Strata) * Flank;
            }

            float LavaContact(float U, float V)
            {
                // 용암이 덮는 가느다란 상류 수로만 지정 열원과 반응한다.
                const float Axis = 0.52F + 0.11F * std::sin(6.F * U - 0.52F);
                return SmoothStep(0.06F, 0.16F, U) * (1.F - SmoothStep(0.49F, 0.62F, U)) *
                       (1.F - SmoothStep(0.035F, 0.105F, std::abs(V - Axis)));
            }

            float Bed(const FSurfaceSceneSettings &Settings, float U, float V)
            {
                const auto Scene = Settings.Scene;
                if (Scene == EScene::IceGate || Scene == EScene::SoilPath)
                {
                    // Y자 수로의 공용 입구에서 낮은 지름길과 높이가 있는 우회로가 갈라진다.
                    const float Diverge = SmoothStep(0.17F, 0.37F, U);
                    const float Short = 0.50F - 0.21F * Diverge;
                    const float Long = 0.50F + 0.21F * Diverge;
                    const float LeftBank = SmoothStep(0.070F, 0.135F, std::abs(V - Short));
                    const float RightBank = SmoothStep(0.065F, 0.135F, std::abs(V - Long));
                    const float Rim = 0.001F * (1.F - SmoothStep(0.0F, 0.14F, U));
                    const float MainSlope = 0.020F * (1.F - U);
                    const float Pocket = Scene == EScene::SoilPath
                                             ? Settings.DepositionBasinDepthM * SmoothStep(0.38F, 0.49F, U) *
                                                   (1.F - SmoothStep(0.59F, 0.72F, U))
                                             : 0.F;
                    return Rim + MainSlope +
                           std::min(0.019F * LeftBank - Pocket, 0.019F * RightBank + Settings.BranchRiseM * Diverge);
                }
                if (Scene == EScene::Runoff)
                {
                    constexpr std::array<float, 9> Floor{5.F, 1.F, 0.F, 1.F, 2.7F, 0.F, 0.F, 1.F, 2.7F};
                    return 0.001F * (V < 0.2F || V >= 0.8F ? 5.F : Floor[static_cast<int>(9.F * U)]);
                }
                if (Scene == EScene::Textile || Scene == EScene::AbsorptionComparison)
                    return 0.F;
                if (Scene == EScene::Charring)
                    return 0.0018F + 0.00028F * std::cos(42.F * V + 3.F * U) + 0.0002F * std::sin(26.F * V);
                if (Scene == EScene::Terrace)
                {
                    const float Rim = (V < 0.08F || V > 0.92F || U < 0.08F) ? 0.006F : 0.F;
                    const float Divide = std::abs(V - 0.34F - U * 0.22F) < 0.055F && U > 0.25F && U < 0.8F ? 0.003F : 0.F;
                    return Rim + Divide + 0.001F * std::sin(18.F * U) * std::sin(14.F * V);
                }
                if (Scene == EScene::Relief)
                    return ReliefBed(U, V);
                if (Scene == EScene::IceBed)
                    return GlacialBed(U, V);
                if (Scene == EScene::FreezeThaw)
                {
                    const bool bWell = U >= 1.F / 6.F && U < 5.F / 6.F && V >= 1.F / 7.F && V < 6.F / 7.F;
                    return bWell ? 0.F : V < 1.F / 7.F ? 0.0085F : 0.012F;
                }
                if (Scene == EScene::Meadow)
                {
                    const float Axis = 0.50F + 0.095F * std::sin(5.F * U + 0.3F);
                    const float Bank = SmoothStep(0.055F, 0.25F, std::abs(V - Axis));
                    return 0.019F * (1.F - U) + 0.008F * Bank +
                           0.00045F * std::sin(24.F * U + 7.F * V) * std::cos(19.F * V);
                }
                const float Bend = V - 0.50F - 0.19F * std::sin((U + 0.08F) * 6.28318F);
                const float Banks = 0.007F * std::clamp(std::abs(Bend) * 3.5F, 0.F, 1.F);
                const float Sill = 0.002F * (1.F - SmoothStep(0.018F, 0.053F, std::abs(U - 0.48F)));
                return Banks + Sill - 0.013F * U + 0.00012F * std::sin(67.F * U + V * 25.F) * std::sin(66.F * V);
            }

            Liquid::FCellUpdateParameters Material(EScene Scene, const FProceduralUpdateRates &Rates, float U, float V)
            {
                Liquid::FCellUpdateParameters Result;
                Result.CharPerSecond = Rates.CharPerSecond;
                Result.FreezeKgM2SecondK = Rates.FreezeKgM2SecondK;
                Result.MeltKgM2SecondK = Rates.MeltKgM2SecondK;
                Result.EvaporateKgM2SecondK = Rates.EvaporateKgM2SecondK;
                if (Scene == EScene::Charring || Scene == EScene::Compound)
                {
                    const float Heat = Scene == EScene::Charring
                                           ? std::max(0.F, 1.F - std::hypot((U - 0.49F) * 1.35F, (V - 0.56F) * 1.70F))
                                           : std::max(0.F, 1.F - std::hypot((U - 0.82F) * 2.0F, (V - 0.53F) * 1.55F));
                    const float AmbientK = Scene == EScene::Charring ? Liquid::AmbientTemperatureK : 285.15F;
                    Result.TemperatureK = AmbientK + 450.F * Heat * Heat;
                }
                if (Scene == EScene::Meadow)
                {
                    const float Igniter =
                        1.F - SmoothStep(0.35F, 1.F, std::hypot((U - 0.54F) / 0.22F, (V - 0.14F) / 0.13F));
                    Result.TemperatureK = Liquid::AmbientTemperatureK + 520.F * Igniter;
                    Result.AbsorbPerSecond = 3.2F;
                    Result.CapacityKgM2 = 2.8F;
                    Result.DryPerSecond = 0.018F;
                }
                if (Scene == EScene::IceBed)
                {
                    const float Contact = LavaContact(U, V);
                    Result.TemperatureK = 268.15F + 360.F * Contact;
                    if (Contact <= 0.F)
                    {
                        Result.MeltKgM2SecondK = 0.F;
                        Result.EvaporateKgM2SecondK = 0.F;
                    }
                }
                if (Scene == EScene::FreezeThaw)
                    Result.TemperatureK = 278.15F;
                return Result;
            }

            float InitialIce(EScene Scene, float U, float V)
            {
                if (Scene == EScene::Compound)
                    return 1.25F * std::max(0.F, 1.F - std::hypot((U - 0.14F) * 7.F, (V - 0.64F) * 4.F));
                if (Scene == EScene::IceBed)
                    return 3.6F * LavaContact(U, V);
                return 0.F;
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Scene Defaults and Input Selection
    // ============================================================
    #pragma region 01_Scene_Defaults_and_Input_Selection
        // 장면과 UI가 사용하는 포켓 입력 종류를 동일한 형상 파일에 연결한다.

        FProceduralUpdateRates DefaultProceduralUpdateRates(EScene Scene)
        {
            switch (Scene)
            {
                case EScene::Charring:
                return {.CharPerSecond = 0.53F};
                case EScene::Compound:
                return {.MeltKgM2SecondK = 0.032F, .EvaporateKgM2SecondK = 0.0022F};
                case EScene::Meadow:
                return {.EvaporateKgM2SecondK = 0.0011F, .CharPerSecond = 1.30F};
                case EScene::IceBed:
                return {.MeltKgM2SecondK = 0.006F, .EvaporateKgM2SecondK = 0.0032F};
                case EScene::FreezeThaw:
                return {.MeltKgM2SecondK = 0.22F, .FreezeKgM2SecondK = 0.25F};
                default:
                return {};
            }
        }

    #pragma endregion

    // ============================================================
    // 02. Scene Construction
    // ============================================================
    #pragma region 02_Scene_Construction
        // 장면의 높이 표본과 재질 및 초기 물량을 계산 격자에 배치한다.

        FSurfaceSceneRuntime MakeScene(const FSurfaceSceneSettings &Settings)
        {
            const auto                             Scene = Settings.Scene;
            const auto                            *Profile = Settings.Profile ? &*Settings.Profile : nullptr;
            constexpr std::array<glm::vec3, 13>    Colors{{{0.49F, 0.194F, 0.093F},
                                                           {0.72F, 0.61F, 0.43F},
                                                           {0.42F, 0.235F, 0.106F},
                                                           {0.19F, 0.28F, 0.24F},
                                                           {0.48F, 0.48F, 0.48F},
                                                           {0.30F, 0.24F, 0.17F},
                                                           {0.13F, 0.44F, 0.65F},
                                                           {0.42F, 0.22F, 0.12F},
                                                           {0.062F, 0.085F, 0.108F},
                                                           {0.22F, 0.35F, 0.12F},
                                                           {0.25F, 0.29F, 0.35F},
                                                           {0.36F, 0.34F, 0.30F},
                                                           {0.65F, 0.65F, 0.65F}}};
            constexpr std::array<const char *, 13> Textures{"FiredClay.png",
                                                            "",
                                                            "FineWood.png",
                                                            "WeatheredStone.png",
                                                            "BrushedSteel.png",
                                                            "WeatheredStone.png",
                                                            "",
                                                            "FiredClay.png",
                                                            "",
                                                            "MeadowGrass.png",
                                                            "WeatheredStone.png",
                                                            "WeatheredStone.png",
                                                            ""};
            constexpr std::array<float, 13>
                RepeatsPerMeter{9.F, 6.F, 5.F, 4.F, 7.F, 9.F, 0.F, 13.F, 0.F, 5.F, 6.F, 8.F, 1.F};
            constexpr std::array<float, 13> DisplayThickness{0.008F,
                                                             0.0012F,
                                                             0.008F,
                                                             0.008F,
                                                             0.0045F,
                                                             0.006F,
                                                             0.017F,
                                                             0.006F,
                                                             0.004F,
                                                             0.009F,
                                                             0.009F,
                                                             0.009F,
                                                             0.006F};
            const auto                      SceneIndex = static_cast<std::size_t>(Scene);
            const bool                      bIce = Scene == EScene::IceBed;
            const auto                     &Substrate = Profile ? Profile->Substrate : Settings.ProceduralSubstrate;
            const bool                      bIceSubstrate = bIce && Substrate.Id.empty();
            const bool                      bElevated = Scene == EScene::Relief || bIce;
            const bool                      bCavity = Scene == EScene::Cavity;
            const bool                      bPhase = Scene == EScene::FreezeThaw;
            const auto                     &Quench = Settings.SubstrateQuench;
            const bool                      bLegacySpillwayIce =
                Scene == EScene::Compound && !Quench && !Settings.TemperatureRamp &&
                std::ranges::any_of(Settings.States,
                                    [](const auto &State)
                                    { return State.Channel == Rendering::ESurfaceStateChannel::IceMass; });
            const bool bChartAlbedo = !Settings.ChartAlbedoPath.empty();
            const auto TextureDirectory = std::filesystem::path(MDSS_ASSET_DIR) / "Textures" / "SurfaceSimulation";
            const auto TexturePath = bChartAlbedo                           ? Settings.ChartAlbedoPath
                                     : !Settings.SurfaceTexturePath.empty() ? Settings.SurfaceTexturePath
                                     : !Substrate.ColorTexture.empty()      ? Substrate.ColorTexture
                                     : (bIce || bPhase || Textures[SceneIndex][0] == 0)
                                         ? std::filesystem::path{}
                                         : TextureDirectory / Textures[SceneIndex];
            auto       WetnessDepth = Rendering::FAbsorbedWetnessDepth{};
            if (Settings.WetnessRedistributionStartSeconds)
            {
                if (!Profile || Profile->WetClothReferenceKgM2 <= 0.F || Substrate.WetnessDepth.DiffusivityM2S <= 0.F ||
                    !Settings.BodyThicknessMeters || Settings.SpecimenCount() != 1)
                    throw std::invalid_argument(
                        "Wetness redistribution needs one absorbing specimen with a body thickness.");
                WetnessDepth = Substrate.WetnessDepth;
                WetnessDepth.RedistributionStartSeconds = *Settings.WetnessRedistributionStartSeconds;
            }
            const auto NormalTexturePath = bChartAlbedo                          ? Settings.ChartNormalPath
                                           : !Settings.SurfaceNormalPath.empty() ? Settings.SurfaceNormalPath
                                           : Settings.SurfaceTexturePath.empty() ? Substrate.NormalTexture
                                                                                 : std::filesystem::path{};
            const int  SpecimenCount = Settings.SpecimenCount();
            const bool bComparison = SpecimenCount > 1;
            if (bComparison && (Settings.ShapeColumns % SpecimenCount != 0 || Settings.GridColumns < SpecimenCount ||
                                Settings.GridColumns % SpecimenCount != 0 ||
                                Settings.Source != SurfaceState::EHeightfieldSource::Procedural || Settings.ShallowBasin))
                throw std::invalid_argument("Both grid widths must be divisible by the number of procedural specimens.");
            const int        SpecimenWidth = Settings.ShapeColumns / SpecimenCount;
            std::vector<int> ClosedColumns;
            for (int Index = 1; Index < SpecimenCount; ++Index)
                ClosedColumns.push_back(Index * SpecimenWidth);
            const auto           Rows = Liquid::Partition(Settings.ShapeRows, Settings.GridRows);
            const auto           Columns = Liquid::Partition(Settings.ShapeColumns, Settings.GridColumns);
            Liquid::FHeightfield Shape{Settings.ShapeColumns, Settings.ShapeRows, Settings.SizeM[0], Settings.SizeM[1], {}};
            if (Settings.Source == SurfaceState::EHeightfieldSource::Procedural)
            {
                for (int Y = 0; Y < Shape.Rows; ++Y)
                    for (int X = 0; X < Shape.Columns; ++X)
                    {
                        const float U = (X + 0.5F) / Shape.Columns;
                        const float V = (Y + 0.5F) / Shape.Rows;
                        float       Height = Bed(Settings, U, V);
                        if (const auto &Basin = Settings.ShallowBasin)
                        {
                            const float Dx = (U - Basin->CenterUV[0]) * Shape.Width / Basin->RadiusMeters;
                            const float Dy = (V - Basin->CenterUV[1]) * Shape.Length / Basin->RadiusMeters;
                            Height -= Basin->DepthMeters * (1.F - std::min(1.F, Dx * Dx + Dy * Dy));
                        }
                        if (const auto &Relief = Settings.SurfaceRelief)
                        {
                            constexpr double Turn = 2.0 * std::numbers::pi;
                            // 대응하는 형상 샘플에 같은 높이를 넣어 재질의 효과를 비교한다.
                            const float LocalU = (X % SpecimenWidth + 0.5F) / SpecimenWidth;
                            Height += Relief->AmplitudeMeters *
                                      static_cast<float>(std::cos(Turn * Relief->Cycles[0] * (LocalU - 0.5F)) *
                                                         std::cos(Turn * Relief->Cycles[1] * (V - 0.5F)));
                        }
                        Shape.Heights.push_back(Height);
                    }
            }
            else
            {
                if (!std::isfinite(Settings.HeightOffsetM + Settings.Scale))
                    throw std::runtime_error("The height map elevation range must be finite.");
                Shape = SurfaceState::LoadHeightfield(Settings.Source,
                                                      Settings.Path,
                                                      Shape.Columns,
                                                      Shape.Rows,
                                                      Shape.Width,
                                                      Shape.Length,
                                                      Settings.Scale);
                const auto Bands = SurfaceState::SplitHeightfield(Shape, Settings.BaseFilter, Settings.FineFilter);
                for (std::size_t Index = 0; Index < Shape.Heights.size(); ++Index)
                    Shape.Heights[Index] = Bands.Base.Heights[Index] +
                                           (Settings.bBaseOnly ? 0.F : Bands.Meso.Heights[Index]) + Settings.HeightOffsetM;
            }
            if (bCavity)
            {
                const float Floor = *std::ranges::min_element(Shape.Heights);
                for (float &Height : Shape.Heights)
                    Height -= Floor;
            }
            auto       LiquidMaterial = Settings.LiquidOverride ? *Settings.LiquidOverride
                                        : Profile && Settings.LiquidMaterialPath.empty()
                                            ? Profile->Liquid
                                            : Asset::LoadLiquidMaterial(Settings.LiquidMaterialPath);
            const bool bEnergyPulse = Settings.TemperatureRamp && Settings.TemperatureRamp->bFromEnergyPulse;
            if (bEnergyPulse)
                LiquidMaterial.LocalRule = Liquid::ELocalMaterialRule::None;
            const auto StorageMode = Settings.bSplitSubgridBasins ? Liquid::EStorageRegionMode::SubgridBasins
                                                                  : Liquid::EStorageRegionMode::SinglePerCell;
            auto       Geometry = Liquid::MakeGeometry(std::move(Shape),
                                                       Columns,
                                                       Rows,
                                                       !bComparison && Settings.bExternalExchange &&
                                                           Settings.OpenOutletOverride.value_or(Settings.bOpenOutlet),
                                                       LiquidMaterial,
                                                       StorageMode,
                                                       ClosedColumns);
            Geometry.FilmLayers = Settings.FilmLayers;
            Geometry.bFilmBedCoupling = Settings.bFilmBedCoupling;
            Geometry.FilmBedFreezeStart = Settings.SecondPourStart;
            Geometry.FilmOmittedSupport = Settings.FilmOmittedSupport;
            Geometry.FilmIceLayer = Settings.FilmIceLayer;
            if (Settings.bLocalUpdates)
                Geometry.FilmReactions = Settings.FilmReactions;
            if (!Geometry.FilmLayers.empty())
            {
                // 에디터에서 바꾼 대표 액체의 물성도 같은 층별 수송 입력으로 전달한다.
                auto &Primary = Geometry.FilmLayers.back();
                Primary.DensityKgM3 = LiquidMaterial.DensityKgM3;
                Primary.ViscosityPaS = LiquidMaterial.DynamicViscosityPaS;
                Primary.YieldStressPa = LiquidMaterial.YieldStressPa;
                Primary.Friction = LiquidMaterial.Friction;
                const auto         &Shape = Geometry.Shape;
                const double        Area = static_cast<double>(Shape.PitchX()) * Shape.PitchY();
                const auto          Count = Geometry.Regions.size();
                std::vector<double> Mass(Geometry.FilmLayers.size() * Count);
                for (const auto &Patch : Settings.FilmPatches)
                {
                    for (int Y = 0; Y < Shape.Rows; ++Y)
                        for (int X = 0; X < Shape.Columns; ++X)
                        {
                            const auto  &B = Patch.Bounds;
                            const double U = std::max(0.0,
                                                      std::min((X + 1.) / Shape.Columns, double(B[2])) -
                                                          std::max(X / double(Shape.Columns), double(B[0])));
                            const double V = std::max(0.0,
                                                      std::min((Y + 1.) / Shape.Rows, double(B[3])) -
                                                          std::max(Y / double(Shape.Rows), double(B[1])));
                            const auto   Region = Geometry.SampleRegions[Y * Shape.Columns + X];
                            Mass[Patch.Layer * Count + Region] += U * V * (Area * Shape.Columns * Shape.Rows) *
                                                                  Patch.ThicknessM *
                                                                  Geometry.FilmLayers[Patch.Layer].DensityKgM3;
                        }
                }
                Geometry.InitialFilmKg.assign(Mass.begin(), Mass.end());
            }
            Geometry.bTransport = Settings.bTransport;
            Geometry.TransportSubsteps = Settings.TransportSubsteps;
            Geometry.bExternalExchange = Settings.bExternalExchange;
            if (bComparison)
            {
                // 같은 투영 면적에 같은 면밀도로 물을 배분하고 시편 사이의 수송은 차단한다.
                Liquid::SetScheduledInlet(Geometry, {{0.F, 0.F}, Settings.SizeM});
            }
            else if (Settings.ScheduledInlet)
                Liquid::SetScheduledInlet(Geometry, *Settings.ScheduledInlet);
            if (Quench)
            {
                if (!Liquid::UsesWaterMaterial(Geometry))
                    Geometry.Liquid.LocalRule = Liquid::ELocalMaterialRule::Water;
                Geometry.SubstrateQuench = Quench->Material;
                if (!Settings.bLocalUpdates)
                    Geometry.SubstrateQuench.ContactConductanceWm2K = 0.F;
            }
            if (Liquid::UsesWaterMaterial(Geometry))
            {
                const auto Rates = Settings.ProceduralUpdateOverride.value_or(DefaultProceduralUpdateRates(Scene));
                for (std::size_t Cell = 0; Cell < Geometry.Cells.size(); ++Cell)
                {
                    const auto  Bounds = Geometry.Cells[Cell];
                    const float U = (Bounds.FirstColumn + Bounds.EndColumn) / (2.F * Geometry.Shape.Columns);
                    const float V = (Bounds.FirstRow + Bounds.EndRow) / (2.F * Geometry.Shape.Rows);
                    if (Quench)
                    {
                        const float Distance = std::hypot((U - Quench->CenterUV[0]) / Quench->RadiusUV[0],
                                                          (V - Quench->CenterUV[1]) / Quench->RadiusUV[1]);
                        Geometry.Materials[Cell].TemperatureK = std::lerp(Liquid::AmbientTemperatureK,
                                                                          Quench->PeakTemperatureK,
                                                                          1.F - SmoothStep(0.12F, 1.F, Distance));
                    }
                    else if (Profile)
                    {
                        auto &CellMaterial = Geometry.Materials[Cell];
                        CellMaterial = Profile->Cell;
                        const int Specimen = Bounds.FirstColumn / SpecimenWidth;
                        if (Specimen > 0)
                            CellMaterial.CapacityKgM2 = Settings.ComparisonProfiles[Specimen - 1].Cell.CapacityKgM2;
                        const float Drying = V < Profile->DryingSplitV ? Profile->DryingLow : Profile->DryingHigh;
                        CellMaterial.DryPerSecond *= Drying;
                        Geometry.InitialIceKgM2[Cell] =
                            Settings.InitialIcePeakKgM2 *
                            std::max(0.F,
                                     1.F - Settings.InitialIceInverseRadius * std::hypot(U - Settings.InitialIceCenter[0],
                                                                                         V - Settings.InitialIceCenter[1]));
                    }
                    else
                    {
                        Geometry.Materials[Cell] = Material(Scene, Rates, U, V);
                        if (Scene != EScene::Compound || bLegacySpillwayIce)
                            Geometry.InitialIceKgM2[Cell] = Settings.IceMassScale * InitialIce(Scene, U, V);
                    }
                    if (!Settings.bLocalUpdates)
                    {
                        // 초기 상태와 지정 온도는 유지하고 스텝 동안의 국소 변환만 끈다.
                        auto &CellMaterial = Geometry.Materials[Cell];
                        CellMaterial.MeltKgM2SecondK = 0.F;
                        CellMaterial.FreezeKgM2SecondK = 0.F;
                        CellMaterial.AbsorbPerSecond = 0.F;
                        CellMaterial.DryPerSecond = 0.F;
                        CellMaterial.EvaporateKgM2SecondK = 0.F;
                        CellMaterial.CharPerSecond = 0.F;
                    }
                }
            }
            if (const auto &Ramp = Settings.TemperatureRamp)
            {
                Geometry.InitialTemperatureWeight = 0.F;
                for (std::size_t Cell = 0; Cell < Geometry.Cells.size(); ++Cell)
                {
                    const auto &Bounds = Geometry.Cells[Cell];
                    const float U = (Bounds.FirstColumn + Bounds.EndColumn) / (2.F * Geometry.Shape.Columns);
                    Geometry.Materials[Cell].TemperatureK = std::lerp(Ramp->TargetK[0], Ramp->TargetK[1], U);
                }
            }
            if (Scene == EScene::Terrace || Scene == EScene::Compound)
            {
                const auto X = Geometry.Shape.Columns / 7;
                const auto Y = Geometry.Shape.Rows * 2 / 3;
                Geometry.SourceSample = Y * Geometry.Shape.Columns + X;
            }
            if (bElevated)
            {
                const int X = Geometry.Shape.Columns / 13;
                const int Y = Geometry.Shape.Rows / 2;
                Geometry.SourceSample = Y * Geometry.Shape.Columns + X;
            }
            if (bCavity)
            {
                const int X = Geometry.Shape.Columns / 4;
                const int Y = Geometry.Shape.Rows / 2;
                Geometry.SourceSample = Y * Geometry.Shape.Columns + X;
            }
            if (!Quench && Scene == EScene::Meadow && Settings.bLocalUpdates)
            {
                Geometry.CombustionGridColumns = static_cast<int>(Columns.size()) - 1;
                Geometry.bEvaporateAbsorbedWater = true;
            }
            if (!Quench && (Scene == EScene::Charring || Scene == EScene::Meadow || bIce))
                Geometry.InitialTemperatureWeight = 0.F;
            Geometry.InitialLiquidKg = Settings.InitialLiquidKg;
            std::vector<Rendering::FSurfaceSpecimenAppearance> Specimens;
            if (bComparison && !bChartAlbedo)
            {
                const auto AddAppearance = [&](const FSurfaceProfileSettings &Specimen)
                {
                    Specimens.push_back(
                        {Specimen.Substrate.ColorTexture, Specimen.Substrate.NormalTexture, Specimen.Substrate.bMetallic});
                };
                AddAppearance(*Profile);
                for (const auto &Specimen : Settings.ComparisonProfiles)
                    AddAppearance(Specimen);
            }
            return {std::move(Geometry),
                    Settings.PourKg,
                    Settings.MaxTiltRadians,
                    {{bIce && Substrate.bMetallic ? glm::vec3(0.57F, 0.59F, 0.61F) : Colors[SceneIndex],
                        TexturePath,
                        bIce && !bIceSubstrate ? 7.F : RepeatsPerMeter[SceneIndex],
                        Substrate.Optics,
                        NormalTexturePath},
                     Settings.BodyThicknessMeters.value_or(DisplayThickness[SceneIndex]),
                     bIceSubstrate,
                     Scene == EScene::Charring,
                     Profile                   ? Profile->WetClothReferenceKgM2
                     : Scene == EScene::Meadow ? 1.05F
                                               : 0.F,
                     bIce,
                     Profile ? Profile->LiquidOpticalDepthScale : 1.F,
                     Profile ? Profile->LiquidCoverageDepthMeters : Settings.FilmWaterCoverageDepthM,
                     Scene == EScene::Compound || (!bComparison && Substrate.bMetallic),
                     bChartAlbedo,
                     Scene == EScene::Meadow ? 1.F : 0.F,
                     Settings.Environment,
                     Settings.ComparisonGapMeters,
                     SpecimenCount,
                     std::move(Specimens),
                     Settings.bSubstrateThermalEmission,
                     Settings.LiquidTint,
                     Settings.SectionWindow,
                     WetnessDepth,
                     Settings.Snow},
                    Settings.PourSeconds,
                    Scene,
                    Settings.ContactHeatFadeSeconds,
                    Settings.SecondPourStart,
                    Settings.TemperatureRamp ? Settings.TemperatureRamp->RiseSeconds : 0.F,
                    Settings.PourStartSeconds};
        }

    #pragma endregion

    // ============================================================
    // 03. Time Based Inputs and Camera
    // ============================================================
    #pragma region 03_Time_Based_Inputs_and_Camera
        // 프레임 간격과 독립적으로 시뮬레이션 입력 및 카메라를 경과 시간으로 평가한다.

        float ScheduledInput(const FSurfaceSceneRuntime &Scene, float Time)
        {
            if (!Scene.Geometry.bExternalExchange || Scene.PourSeconds <= 0.F)
                return 0.F;
            // 급수와 현재 스텝이 겹친 시간에만 질량을 배분한다.
            const float Start = Scene.PourStartSeconds;
            float       Duration = std::clamp(Start + Scene.PourSeconds - Time, 0.F, Liquid::StepSeconds) -
                                   std::clamp(Start - Time, 0.F, Liquid::StepSeconds);
            if (Scene.SecondPourStart > 0.F)
                Duration += std::max(0.F,
                                     std::min(Time + Liquid::StepSeconds, Scene.SecondPourStart + Scene.PourSeconds) -
                                         std::max(Time, Scene.SecondPourStart));
            return Scene.PourKg / Scene.PourSeconds * Duration;
        }

        float ScheduledAngle(const FSurfaceSceneRuntime &Scene, float Time)
        {
            return Scene.MaxTilt * std::clamp((Time - 3.F) / 5.F, 0.F, 1.F);
        }

        Liquid::FThermalInput ScheduledTemperature(const FSurfaceSceneRuntime &Scene, float Time)
        {
            if (Scene.TemperatureRiseSeconds > 0.F)
                return {0.F, std::clamp(Time / Scene.TemperatureRiseSeconds, 0.F, 1.F)};
            if (Scene.Kind == EScene::IceGate)
                return {-24.F + 40.F * SmoothStep(2.5F, 3.25F, Time), 1.F};
            if (Scene.Kind == EScene::Charring)
                return {0.F, SmoothStep(0.15F, 2.65F, Time)};
            if (Scene.Kind == EScene::Meadow)
                return {0.F, SmoothStep(3.25F, 3.55F, Time) * (1.F - SmoothStep(4.0F, 4.45F, Time))};
            if (Scene.Kind == EScene::FreezeThaw)
                return {-13.F * SmoothStep(0.9F, 1.5F, Time) + 24.F * SmoothStep(4.1F, 4.8F, Time), 1.F};
            if (Scene.Kind == EScene::IceBed)
            {
                const float ContactHeat =
                    SmoothStep(0.30F, 1.65F, Time) * (1.F - SmoothStep(2.F, 2.F + Scene.ContactHeatFadeSeconds, Time));
                // Heat가 꺼진 구간을 빙점 아래로 되돌린다. 물의 재동결 계수는 별도로 제어한다.
                return {-25.F * (1.F - ContactHeat), ContactHeat};
            }
            return {};
        }

        FDemoCamera ScheduledCamera(EScene Scene, float Time)
        {
            const float Reveal = SmoothStep(0.25F, 6.0F, Time);
            const float Inspect = SmoothStep(0.05F, 0.85F, Time);
            switch (Scene)
            {
                case EScene::Relief:
                case EScene::IceBed:
                return {-21.F * Reveal, 11.F * Reveal, 1.F};
                case EScene::Charring:
                return {14.F * Reveal, -10.F * Reveal, 1.F - 0.13F * Reveal};
                case EScene::Terrace:
                case EScene::Textile:
                return {12.F * Reveal, 18.F * Reveal, 1.F};
                case EScene::FreezeThaw:
                return {0.F, 0.F, 1.10F - 0.10F * Inspect};
                default:
                return {};
            }
        }

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
