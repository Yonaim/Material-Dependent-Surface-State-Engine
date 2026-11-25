/**
 * @file Experiments.cpp
 * @brief Surface Simulation의 질량 보존, 수위 복원 및 CPU와 GPU 정합을 검사한다.
 */

#include "Application/Editor/SurfaceSimulation/Experiments.h"
#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Rendering/Surface/Simulation/SurfaceSimulationTiming.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include "SurfaceState/Simulation/SurfaceSimulationGpu.h"
#include "SurfaceState/Simulation/SurfaceSimulationReference.h"
#include "SurfaceState/Simulation/SurfaceSimulationSnapshot.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <spawn.h>
    #include <sys/wait.h>
extern char **environ;
#endif

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Experiment Metrics and Acceptance Bounds
    // ============================================================
    #pragma region 00_Experiment_Metrics_and_Acceptance_Bounds
        // 셀 상태와 물 장부를 집계하고 시나리오마다 비교 허용 오차를 적용한다.

        namespace
        {
            constexpr int    Frames = 1920;
            constexpr int    SampleInterval = 48;
            constexpr double VisibleSpillKg = 0.00001;

            struct FCavityMetrics
            {
                double DepthMillimeters;
                double CapacityMilliliters;
            };

            FCavityMetrics MeasureCavity(const Liquid::FSurfaceSimulationDomain &Geometry)
            {
                // 첫 셀의 작은 함몰들을 모두 고려해 소스에서 셀 밖으로 나가는 최소 통과 높이를 구한다.
                const auto         &Shape = Geometry.Shape;
                const int           Parent = Geometry.SourceCell();
                const auto          Bounds = Geometry.Cells[Parent];
                const auto          Area = static_cast<double>(Shape.PitchX()) * Shape.PitchY();
                const double        Infinity = std::numeric_limits<double>::infinity();
                std::vector<double> Reach(Shape.Heights.size(), Infinity);
                using FVisit = std::pair<double, int>;
                std::priority_queue<FVisit, std::vector<FVisit>, std::greater<>> Queue;
                Queue.emplace(Shape.Heights[Geometry.SourceSample], Geometry.SourceSample);
                Reach[Geometry.SourceSample] = Shape.Heights[Geometry.SourceSample];
                double Sill = Infinity;
                double Floor = Infinity;
                while (!Queue.empty())
                {
                    const auto [Level, Sample] = Queue.top();
                    Queue.pop();
                    if (Level != Reach[Sample] || Level > Sill)
                        continue;
                    const int X = Sample % Shape.Columns, Y = Sample / Shape.Columns;
                    Floor = std::min(Floor, static_cast<double>(Shape.Heights[Sample]));
                    const std::array<std::array<int, 2>, 4> Neighbors{{{X - 1, Y}, {X + 1, Y}, {X, Y - 1}, {X, Y + 1}}};
                    for (const auto [NX, NY] : Neighbors)
                    {
                        if (NY < 0 || NY >= Shape.Rows || NX < 0 || NX >= Shape.Columns)
                            continue;
                        const int    Next = NY * Shape.Columns + NX;
                        const double Candidate = std::max(Level, static_cast<double>(Shape.Heights[Next]));
                        if (NX < Bounds.FirstColumn || NX >= Bounds.EndColumn || NY < Bounds.FirstRow ||
                            NY >= Bounds.EndRow)
                            Sill = std::min(Sill, Candidate);
                        else if (Candidate < Reach[Next])
                        {
                            Reach[Next] = Candidate;
                            Queue.emplace(Candidate, Next);
                        }
                    }
                }
                double Volume = 0.0;
                if (std::isfinite(Sill))
                    for (std::size_t Sample = 0; Sample < Reach.size(); ++Sample)
                        if (Reach[Sample] <= Sill)
                            Volume += Area * std::max(0.0, Sill - Shape.Heights[Sample]);
                return {std::isfinite(Sill) ? 1000.0 * (Sill - Floor) : 0.0, 1.e6 * Volume};
            }

            struct FMetrics
            {
                double Surface = 0.0;
                double Ice = 0.0;
                double Absorbed = 0.0;
                double Outflow = 0.0;
                double Evaporated = 0.0;
                double Reconstruction = 0.0;
                double Char = 0.0;
                double Flame = 0.0;
                double Steam = 0.0;
                double HottestK = 0.0;
                double TemperatureDeviationK = 0.0;

                double Total() const
                {
                    return Surface + Ice + Absorbed + Outflow + Evaporated;
                }
            };

            FMetrics Sum(const Liquid::FSurfaceSimulationDomain &Geometry,
                         const Liquid::FSurfaceSimulationState  &State,
                         Liquid::FSurfaceTilt                    Pose)
            {
                FMetrics Metrics;
                for (int Cell = 0; Cell < static_cast<int>(State.Cells.size()); ++Cell)
                {
                    const auto &S = State.Cells[Cell];
                    Metrics.Surface += S.Mass;
                    Metrics.Ice += S.Ice;
                    Metrics.Absorbed += S.Absorbed;
                    Metrics.Outflow += S.Outflow;
                    Metrics.Evaporated += S.Evaporated;
                    Metrics.Char = std::max(Metrics.Char, static_cast<double>(S.Char));
                    Metrics.Flame = std::max(Metrics.Flame, static_cast<double>(S.Flame));
                    Metrics.Steam += S.SteamRate;
                    Metrics.HottestK = std::max(Metrics.HottestK, static_cast<double>(S.TemperatureK));
                    Metrics.TemperatureDeviationK =
                        std::max(Metrics.TemperatureDeviationK,
                                 static_cast<double>(std::abs(S.TemperatureK - Liquid::AmbientTemperatureK)));
                    if (std::min({S.Mass, S.Ice, S.Absorbed, S.Outflow, S.Evaporated}) < 0.F || S.Char < 0.F ||
                        S.Char > 1.F || !std::isfinite(S.Level) || !std::isfinite(S.TemperatureK) ||
                        !std::isfinite(Metrics.Total()))
                        throw std::runtime_error("State left its admissible range.");
                }
                const auto Coating = Geometry.FilmLayers.size() > 1 ? Liquid::MakeFilmDisplay(Geometry, State, Pose)
                                                                    : std::vector<std::array<float, 6>>{};
                for (int Region = 0; Region < static_cast<int>(State.Regions.size()); ++Region)
                {
                    const auto Water = State.Regions[Region];
                    if (Water.Mass > 0.F)
                    {
                        const double Volume =
                            Liquid::RegionVolume(Geometry, Region, Pose, Water.Level) -
                            (Coating.empty() ? 0.0 : Liquid::RegionVolume(Geometry, Region, Pose, Coating[Region][5]));
                        Metrics.Reconstruction += std::abs(Geometry.Liquid.DensityKgM3 * Volume - Water.Mass);
                    }
                    if (!std::isfinite(Water.Level) || !std::isfinite(Water.Mass) || Water.Mass < 0.F)
                        throw std::runtime_error("Invalid liquid basin state.");
                }
                return Metrics;
            }

            void Require(bool Condition, const char *Message)
            {
                if (!Condition)
                    throw std::runtime_error(Message);
            }

            void SubmitForReadback(const GPU::FVulkanContext &Context, VkCommandBuffer Buffer)
            {
                VkMemoryBarrier Host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                Host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                Host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(Buffer,
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                     VK_PIPELINE_STAGE_HOST_BIT,
                                     0,
                                     1,
                                     &Host,
                                     0,
                                     nullptr,
                                     0,
                                     nullptr);
                Context.GetCommands().EndSingleTime(Buffer, Context.GetQueues().GetGraphics());
            }

            struct FReactionTolerance
            {
                float TemperatureK = 1.e-4F;
                float Char = 3.e-4F;
                float Flame = 3.e-4F;
                float SteamKgS = 3.e-6F;
            };

            template <typename TActual, typename TExpected>
            double Compare(const TActual &Gpu, const TExpected &Cpu, FReactionTolerance Reaction = {})
            {
                double MaxMassError = 0.0;
                for (std::size_t Cell = 0; Cell < Cpu.State.Cells.size(); ++Cell)
                {
                    const auto  &A = Gpu.State.Cells[Cell], &B = Cpu.State.Cells[Cell];
                    const double Error = std::max({std::abs(A.Mass - B.Mass),
                                                   std::abs(A.Ice - B.Ice),
                                                   std::abs(A.Absorbed - B.Absorbed),
                                                   std::abs(A.Outflow - B.Outflow),
                                                   std::abs(A.Evaporated - B.Evaporated)});
                    MaxMassError = std::max(MaxMassError, Error);
                    if (Error >= 2.e-6 || std::abs(A.Level - B.Level) >= 8.e-6 ||
                        std::abs(A.PhaseLevel - B.PhaseLevel) >= 8.e-6 || std::abs(A.Char - B.Char) >= Reaction.Char ||
                        std::abs(A.SteamRate - B.SteamRate) >= Reaction.SteamKgS ||
                        std::abs(A.Flame - B.Flame) >= Reaction.Flame ||
                        std::abs(A.TemperatureK - B.TemperatureK) >= Reaction.TemperatureK)
                    {
                        std::ostringstream Message;
                        Message << std::scientific << "CPU/GPU disagreement at cell " << Cell
                                << ", mass delta [kg] = " << Error << ", level delta [m] = " << std::abs(A.Level - B.Level)
                                << ", temperature [K] GPU / CPU = " << A.TemperatureK << " / " << B.TemperatureK
                                << ", temperature delta [K] = " << std::abs(A.TemperatureK - B.TemperatureK)
                                << ", steam rate delta [kg/s] = " << std::abs(A.SteamRate - B.SteamRate)
                                << ", char delta = " << std::abs(A.Char - B.Char)
                                << ", flame delta = " << std::abs(A.Flame - B.Flame);
                        throw std::runtime_error(Message.str());
                    }
                }
                for (std::size_t Region = 0; Region < Cpu.State.Regions.size(); ++Region)
                {
                    const auto   A = Gpu.State.Regions[Region], B = Cpu.State.Regions[Region];
                    const double MassError = std::abs(A.Mass - B.Mass);
                    MaxMassError = std::max(MaxMassError, MassError);
                    Require(MassError < 2.e-6 && std::abs(A.Level - B.Level) < 8.e-6, "CPU/GPU basin disagreement.");
                    Require(std::abs(Gpu.RegionOutflowScale[Region] - Cpu.RegionOutflowScale[Region]) < 0.01F,
                            "CPU/GPU donor scale disagreement.");
                }
                for (std::size_t Index = 0; Index < Cpu.FaceCandidateMassKg.size(); ++Index)
                    Require(std::abs(Gpu.FaceCandidateMassKg[Index] - Cpu.FaceCandidateMassKg[Index]) < 1.e-6F,
                            "CPU/GPU face mass disagreement.");
                return MaxMassError;
            }

            void VerifyActiveStateDeclarations(const FSurfaceSceneSettings &Settings,
                                               double                       PeakLiquid,
                                               double                       PeakIce,
                                               double                       PeakAbsorbed,
                                               double                       Char,
                                               double                       TemperatureDeviationK)
            {
                if (Settings.States.empty())
                    return;

                using enum Rendering::ESurfaceStateChannel;
                const auto Has = [&](Rendering::ESurfaceStateChannel Channel)
                {
                    return std::ranges::find(Settings.States, Channel, &Rendering::FStateVisualization::Channel) !=
                           Settings.States.end();
                };
                constexpr double MassEpsilonKg = 1.e-8;
                Require(PeakLiquid <= MassEpsilonKg || Has(LiquidMass),
                        "Active surface liquid is missing from the scene states.");
                Require(PeakIce <= MassEpsilonKg || Has(IceMass), "Active ice or snow is missing from the scene states.");
                Require(PeakAbsorbed <= MassEpsilonKg || Has(AbsorbedMass),
                        "Active absorbed water is missing from the scene states.");
                Require(Char == 0.0 || Has(CharFraction), "Active char is missing from the scene states.");
                const auto TemperatureState =
                    std::ranges::find(Settings.States, Temperature, &Rendering::FStateVisualization::Channel);
                Require(TemperatureState == Settings.States.end() ? TemperatureDeviationK <= 1.e-3
                                                                  : TemperatureState->bSubstrate,
                        "Active temperature must be declared as a substrate state.");
            }

            void VerifyScenario(EScene                                 Scene,
                                const FSurfaceSceneRuntime            &Setup,
                                const Liquid::FSurfaceSimulationState &State,
                                const FMetrics                        &Final,
                                double                                 InitialIce,
                                double                                 PeakAbsorbed,
                                double                                 PeakFlame,
                                double                                 PeakSteam)
            {
                using namespace Liquid;
                switch (Scene)
                {
                    case EScene::Runoff:
                    {
                        const auto &Geometry = Setup.Geometry;
                        const bool  bReference = Geometry.Shape.Columns == 9 && Geometry.Shape.Rows == 5 &&
                                             Geometry.Cells.size() == 3 &&
                                             std::abs(Geometry.Shape.Height(4, 2) - 0.0027F) < 1.e-7F;
                        double      Downstream = Final.Outflow;
                        if (bReference)
                        Downstream += State.Cells.back().Mass;
                        else
                        for (std::size_t Cell = 0; Cell < State.Cells.size(); ++Cell)
                            if (Geometry.Cells[Cell].FirstColumn >= Geometry.Shape.Columns / 2)
                                Downstream += State.Cells[Cell].Mass;
                        Require(Downstream > 0.00012F, "Tilt did not move visible water beyond the down-slope sill.");
                        break;
                    }
                    case EScene::AbsorptionComparison:
                    break; // 양쪽 시편의 개별 질량 수지는 실험 요약에서 확인한다.
                    case EScene::Textile:
                    Require(InitialIce > 0.0 && Final.Ice < InitialIce * 0.12 && PeakAbsorbed > InitialIce * 0.20 &&
                                Final.Absorbed < PeakAbsorbed && Final.Evaporated > InitialIce * 0.10,
                            "Snow demo did not exercise melting, retention and drying.");
                    break;
                    case EScene::Charring:
                    Require(Final.Char > 0.65 && PeakFlame > 0.15 && Final.Flame < PeakFlame * 0.45 &&
                                Final.Total() < 1.e-8,
                            "Charring did not outlast flame activity.");
                    break;
                    case EScene::Terrace:
                    {
                        const auto Source = Setup.Geometry.Cells[Setup.Geometry.SourceCell()];
                        double     TransverseMass = 0.0;
                        for (std::size_t Cell = 0; Cell < State.Cells.size(); ++Cell)
                        if (Setup.Geometry.Cells[Cell].FirstRow != Source.FirstRow)
                            TransverseMass += State.Cells[Cell].Mass;
                        Require(TransverseMass > Setup.PourKg * 0.20F,
                            "The four-neighbor grid did not transport along both axes.");
                        break;
                    }
                    case EScene::Relief:
                    {
                        const auto &Shape = Setup.Geometry.Shape;
                        const auto  Heights = std::ranges::minmax_element(Shape.Heights);
                        Require(*Heights.max - *Heights.min > 0.043F, "Raised fixture lost its modeled vertical relief.");
                        double Downstream = Final.Outflow;
                        for (std::size_t Index = 0; Index < State.Cells.size(); ++Index)
                        if (Setup.Geometry.Cells[Index].FirstColumn >= Shape.Columns / 2)
                            Downstream += State.Cells[Index].Mass;
                        Require(Downstream > Setup.PourKg * 0.1, "Water has not reached the lower half of the raised fixture.");
                        break;
                    }
                    case EScene::IceBed:
                    {
                        Require(Setup.Appearance.bLavaDeposit && InitialIce > 0.0 && Final.Ice < InitialIce * 0.65 &&
                                PeakSteam > 1.e-6,
                            "Glacier contact did not melt the ice skin and emit steam.");
                        Require(Final.Surface > 1.e-4 && Final.Evaporated > 1.e-5 && Final.Outflow == 0.0 &&
                                Final.Absorbed == 0.0 && Setup.PourKg == 0.F,
                            "Glacier meltwater did not remain in the closed, non-absorbing basin.");
                        break;
                    }
                    case EScene::Cavity:
                    {
                        const double Retained =
                        State.Cells.size() == 2 ? State.Cells[Setup.Geometry.SourceCell()].Mass : Final.Surface;
                        Require(Final.Outflow > Setup.PourKg * 0.15 && Retained > Setup.PourKg * 0.15,
                            "Cavity fixture did not retain water and feed the outlet.");
                        break;
                    }
                    case EScene::FreezeThaw:
                    Require(Final.Ice < 1.e-7 && std::abs(Final.Surface - Setup.Geometry.InitialLiquidKg) < 5.e-7 &&
                                Final.Outflow == 0.0 && Final.Evaporated == 0.0,
                            "The closed phase cycle did not return the original liquid mass.");
                    break;
                    case EScene::Compound:
                    Require(Setup.MaxTilt == 0.F && PeakSteam > 1.e-7 && Final.Evaporated > 1.e-6 &&
                                Final.Absorbed == 0.0 && Final.Char == 0.0 && PeakFlame == 0.0,
                            "The fixed steel channel must emit steam during runoff.");
                    if (Setup.Geometry.SubstrateQuench.ArealHeatCapacityJm2K > 0.F)
                        Require(InitialIce == 0.0 && Final.Ice == 0.0 && Final.Surface + Final.Outflow > 1.e-4,
                                "The steel quench must use water only and retain or discharge liquid.");
                    break;
                    case EScene::Meadow:
                    Require(Setup.MaxTilt == 0.F && PeakAbsorbed > Setup.PourKg * 0.15 && PeakFlame > 0.1 &&
                                Final.Char > 0.2 && PeakSteam > 1.e-6,
                            "The wet meadow must retain runoff, sustain combustion and release steam near the front.");
                    break;
                }
            }

            nlohmann::json Describe(const FSurfaceSceneRuntime            &Scene,
                                    const Liquid::FSurfaceSimulationState &State,
                                    Liquid::FSurfaceTilt                   Pose)
            {
                const auto    &Shape = Scene.Geometry.Shape;
                nlohmann::json Height = nlohmann::json::array(), Depth = nlohmann::json::array();
                nlohmann::json Absorbed = nlohmann::json::array(), Char = nlohmann::json::array();
                const auto     Coating = Liquid::MakeFilmDisplay(Scene.Geometry, State, Pose);
                for (int Y = 0; Y < Shape.Rows; ++Y)
                    for (int X = 0; X < Shape.Columns; ++X)
                    {
                        const auto   Cell = Scene.Geometry.SampleCells[Y * Shape.Columns + X];
                        const auto  &S = State.Cells[Cell];
                        const auto   Region = Scene.Geometry.SampleRegions[Y * Shape.Columns + X];
                        const float  XOffset = (Shape.EdgeX(X) + Shape.PitchX() * 0.5F) * Pose.Sine;
                        const float  Base = Shape.Height(X, Y) * Pose.Cosine - XOffset;
                        const float  DisplayBed = Coating.empty() ? Base : std::max(Base, Coating[Region][3]);
                        const float  SolverBed = Coating.empty() ? Base : std::max(Base, Coating[Region][5]);
                        const float  Z = (DisplayBed + XOffset) / Pose.Cosine;
                        const double Area = Liquid::CellArea(Scene.Geometry, static_cast<int>(Cell));
                        const auto   Water = State.Regions[Scene.Geometry.SampleRegions[Y * Shape.Columns + X]];
                        Height.push_back(Z * 1000.F);
                        Depth.push_back(Water.Mass > 0.F ? std::max(0.F, Water.Level - SolverBed) * 1000.F : 0.F);
                        Absorbed.push_back(S.Absorbed / Area);
                        Char.push_back(S.Char);
                    }
                return {{"columns", Shape.Columns},
                        {"rows", Shape.Rows},
                        {"height_mm", Height},
                        {"depth_mm", Depth},
                        {"absorbed_kg_m2", Absorbed},
                        {"char_fraction", Char}};
            }

            void Save(const nlohmann::json &Data, const std::filesystem::path &Path)
            {
                if (!Path.parent_path().empty())
                    std::filesystem::create_directories(Path.parent_path());
                std::ofstream Stream(Path);
                Stream << Data.dump() << '\n';
                Require(static_cast<bool>(Stream), "Cannot write the experiment report.");
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Reference Invariant Checks
    // ============================================================
    #pragma region 01_Reference_Invariant_Checks
        // 공유 면의 출발량 제한과 국소 장부를 작은 고정 입력으로 확인한다.

        namespace
        {
            void CheckFilmTransport(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const auto Grid = Partition(4, 4);
                auto       Geometry = MakeGeometry({4, 4, 0.10F, 0.10F, std::vector<float>(16)},
                                                   Grid,
                                                   Grid,
                                                   false,
                                                   {1000.F, 0.001F, ELocalMaterialRule::None});
                Geometry.InitialLiquidKg = 0.0015F;
                auto       SingleLayer = Geometry;
                const auto Legacy = StepReference(Geometry, InitialState(Geometry), {}, StepSeconds, 0.F);
                Geometry.FilmLayers = {{1000.F, 0.001F, 0.F, 0.F, {0.2F, 0.4F, 0.6F}}};
                Geometry.InitialFilmKg.resize(Geometry.Regions.size());
                const auto Run = [&]() { return StepReference(Geometry, InitialState(Geometry), {}, StepSeconds, 0.F); };
                const auto Newtonian = Run();
                for (std::size_t I = 0; I < Geometry.Regions.size(); ++I)
                    Require(std::abs(Newtonian.State.Film[I].Mass - Legacy.State.Regions[I].Mass) < 1.e-9F,
                            "Layered zero friction and yield must match the original water path.");

                Geometry.FilmLayers[0].Friction = 0.25F;
                const auto Stopped = Run();
                Require(std::ranges::all_of(Stopped.FaceCandidateMassKg, [](float Value) { return Value == 0.F; }),
                        "Friction must stop the shallow layer below its slope threshold.");
                Geometry.FilmLayers[0].Friction = 0.01F;
                Geometry.FilmLayers[0].YieldStressPa = 0.35F;
                const auto Moves = Run();
                Require(std::ranges::any_of(Moves.FaceCandidateMassKg, [](float Value) { return Value != 0.F; }),
                        "Sufficient shear must move the layer above its combined threshold.");
                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, Geometry);
                    const auto            Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F);
                    SubmitForReadback(*Context, Commands);
                    const auto Actual = Gpu.Readback().State.Film;
                    for (std::size_t I = 0; I < Geometry.Regions.size(); ++I)
                        Require(std::abs(Actual[I].Mass - Moves.State.Film[I].Mass) < 1.e-8F,
                                "Vulkan friction and yield failed the fixed numerical test.");
                }

                // 층 개수가 달라도 같은 물성에서 같은 가용량과 이동량을 계산한다.
                constexpr std::array Resistances{std::array{0.01F, 0.35F}, std::array{0.25F, 0.F}, std::array{0.F, 1000.F}};
                for (const auto &[Friction, Yield] : Resistances)
                {
                    SingleLayer.Liquid.Friction = Geometry.FilmLayers.front().Friction = Friction;
                    SingleLayer.Liquid.YieldStressPa = Geometry.FilmLayers.front().YieldStressPa = Yield;
                    const auto Single = StepReference(SingleLayer, InitialState(SingleLayer), {}, StepSeconds, 0.F);
                    const auto Layered = Run();
                    for (std::size_t I = 0; I < Geometry.Regions.size(); ++I)
                        Require(std::abs(Single.State.Regions[I].Mass - Layered.State.Film[I].Mass) < 1.e-9F,
                                "Identical film material resistance must give identical single and layered transport.");
                    if (Context)
                    {
                        FSurfaceSimulationGpu Gpu(*Context, SingleLayer);
                        const auto            Commands = Context->GetCommands().BeginSingleTime();
                        Gpu.RecordStep(Commands, {}, 0.F);
                        SubmitForReadback(*Context, Commands);
                        Compare(Gpu.Readback(), Single);
                    }
                }
                std::cout << "contracts: single and layered Newtonian, friction and yield transport verified.\n";
            }

            void CheckWettedContactAndLayeredSill(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const auto One = Partition(1, 1);
                const auto Two = Partition(2, 2);
                auto       Flat =
                    MakeGeometry({1, 1, 0.01F, 0.01F, {0.F}}, One, One, false, {1000.F, 0.001F, ELocalMaterialRule::Water});
                const FSurfaceTilt Pose = MakePose(0.3F);
                constexpr float    TargetFraction = 0.25F;
                const double       HalfSpan = 0.005 * Pose.Sine;
                const double       Level = HalfSpan * (2 * TargetFraction - 1);
                Flat.InitialLiquidKg = static_cast<float>(1000.0 * RegionVolume(Flat, 0, Pose, Level));
                Flat.Materials[0].AbsorbPerSecond = 3.F;
                Flat.Materials[0].CapacityKgM2 = 100.F;
                const auto Wet = StepReference(Flat, InitialState(Flat), Pose, StepSeconds, 0.F);
                Require(std::abs(Wet.ContactAreaM2 / CellArea(Flat, 0) - TargetFraction) < 1.e-5,
                        "An inclined sample must contribute only its wetted fraction.");
                Flat.TransportSubsteps = 4;
                const auto Split = StepReference(Flat, InitialState(Flat), Pose, StepSeconds, 0.F);
                Require(Wet.State.Cells.front().Absorbed == Split.State.Cells.front().Absorbed,
                        "A transport-only substep must not repeat the local absorption rule.");
                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, Flat);
                    auto                  Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, Pose, 0.F);
                    SubmitForReadback(*Context, Commands);
                    Compare(Gpu.Readback(), Split);
                }

                const auto Four = Partition(4, 1);
                auto       Groove = MakeGeometry({4, 1, 0.04F, 0.01F, {0.003F, 0.F, 0.F, 0.003F}},
                                                 Four,
                                                 One,
                                                 false,
                                                 {1000.F, 0.001F, ELocalMaterialRule::None});
                Groove.FilmLayers = {{1000.F, 1.F, 1000.F, 0.F, {}}, {1000.F, 0.001F, 0.F, 0.F, {}}};
                Groove.InitialFilmKg = {0.0004F, 0.F};
                Groove.InitialLiquidKg = 0.0002F;
                Require(Groove.Regions.size() == 1, "The footprint fixture must share a single storage region.");
                for (const float Radians : {0.F, 0.025F})
                {
                    const auto Tilt = MakePose(Radians);
                    const auto Cpu = StepReference(Groove, InitialState(Groove), Tilt, StepSeconds, 0.F);
                    const auto VerifyFootprint = [&](const auto &State)
                    {
                        const auto &Mud = State.Film[0];
                        const auto &Water = State.Regions[0];
                        Require(std::abs(Mud.Mass - 0.0004F) < 1.e-9F && std::abs(Water.Mass - 0.0002F) < 1.e-9F,
                                "The two-layer groove must conserve each layer's mass.");
                        if (Radians == 0.F)
                            Require(std::abs(Mud.Level - 0.002F) < 1.e-7F && std::abs(Water.Level - 0.003F) < 1.e-7F,
                                    "Mud must fill only the groove, and water must reach the bare shoulders at 3 mm.");
                        const double WaterVolume =
                            RegionVolume(Groove, 0, Tilt, Water.Level) - RegionVolume(Groove, 0, Tilt, Mud.Level);
                        Require(
                            std::abs(1000.0 * WaterVolume - Water.Mass) < 1.e-8,
                            "The upper water level must invert the volume above the reconstructed mud, including tilt.");
                    };
                    VerifyFootprint(Cpu.State);
                    if (Context)
                    {
                        FSurfaceSimulationGpu Gpu(*Context, Groove);
                        auto                  Commands = Context->GetCommands().BeginSingleTime();
                        Gpu.RecordStep(Commands, Tilt, 0.F);
                        SubmitForReadback(*Context, Commands);
                        const auto Actual = Gpu.Readback();
                        VerifyFootprint(Actual.State);
                        Compare(Actual, Cpu);
                    }
                }
                std::cout << "contracts: layered groove footprint and upper volume verified flat and tilted.\n";

                auto Shelf = MakeGeometry({2, 1, 0.02F, 0.01F, {0.F, 0.F}},
                                          Two,
                                          One,
                                          false,
                                          {1000.F, 0.001F, ELocalMaterialRule::None});
                Shelf.FilmLayers = {{1700.F, 1.F, 1000.F, 0.F, {}}, {1000.F, 0.001F, 0.F, 0.F, {}}};
                Shelf.InitialFilmKg = {0.F, 0.00085F, 0.F, 0.F};
                Shelf.SourceSample = 0;
                Shelf.TransportSubsteps = 4;
                for (const float DepthM : {0.004F, 0.006F})
                {
                    Shelf.InitialLiquidKg = 1000.F * static_cast<float>(CellArea(Shelf, 0)) * DepthM;
                    const auto Cpu = StepReference(Shelf, InitialState(Shelf), {}, StepSeconds, 0.F);
                    const bool bAbove = DepthM > 0.005F;
                    Require(std::abs(Cpu.State.Film[1].Mass - Shelf.InitialFilmKg[1]) < 1.e-9F,
                            "An immobile lower layer must keep its mass while contributing to the upper bed.");
                    Require((Cpu.State.Regions[1].Mass > 0.F) == bAbove,
                            "Free water must first cross the crest raised by the lower layer.");
                    if (Context)
                    {
                        FSurfaceSimulationGpu Gpu(*Context, Shelf);
                        Gpu.BeginDisplayInterval();
                        auto Commands = Context->GetCommands().BeginSingleTime();
                        Gpu.RecordStep(Commands, {}, 0.F, {}, {}, nullptr, true);
                        SubmitForReadback(*Context, Commands);
                        const auto Actual = Gpu.Readback();
                        Require(std::abs(Actual.State.Film[1].Mass - Shelf.InitialFilmKg[1]) < 1.e-9F,
                                "Vulkan must preserve the immobile lower layer.");
                        Require(std::abs(Actual.State.Regions[1].Mass - Cpu.State.Regions[1].Mass) < 1.e-8,
                                "Vulkan did not match the substepped layered crest transfer.");
                        const auto Transfers = Gpu.ReadOverflow();
                        const bool bRecorded =
                            std::ranges::any_of(Transfers, [](const auto &Flow) { return Flow.MassKg > 0.F; });
                        Require(bRecorded == bAbove, "The published overflow must record only the actual crossing.");
                    }
                    std::cout << "contracts: 5 mm layered sill, supply depth " << DepthM * 1000.F << " mm, receiving mass "
                              << Cpu.State.Regions[1].Mass * 1.e6F << " mg\n";
                }
            }

            nlohmann::json CheckBasinOverflow(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const FHeightfield    Bed{3, 2, 0.03F, 0.02F, {0.F, 0.006F, 0.F, 0.F, 0.003F, 0.F}};
                const FLiquidMaterial Water{1000.F, 0.001F, ELocalMaterialRule::None};
                auto                  Geometry = MakeGeometry(Bed, Partition(3, 1), Partition(2, 1), false, Water);
                Require(Geometry.Cells.size() == 1 && Geometry.Regions.size() == 2 && Geometry.Faces.size() == 1 &&
                            Geometry.SampleRegions[0] != Geometry.SampleRegions[2] &&
                            Geometry.SampleRegions[0] == Geometry.SampleRegions[3],
                        "Two basins and their shared spill were not inferred in a single cell.");
                if (Context)
                {
                    const int             Inlet = Geometry.SourceRegion();
                    const int             Target = static_cast<int>(Geometry.SampleRegions[2]);
                    constexpr float       ScheduledKg = 0.0001F;
                    constexpr float       ManualKg = 0.0002F;
                    FSurfaceSimulationGpu Gpu(*Context, Geometry);
                    const auto            Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, ScheduledKg, {}, {ManualKg, Target});
                    Gpu.RecordStep(Commands, {}, 0.F);
                    SubmitForReadback(*Context, Commands);
                    const auto Result = Gpu.Readback();
                    Require(Inlet != Target && std::abs(Result.State.Regions[Inlet].Mass - ScheduledKg) < 1.e-8F &&
                                std::abs(Result.State.Regions[Target].Mass - ManualKg) < 1.e-8F,
                            "A manual dose must reach its selected basin once without moving the scheduled inlet.");
                }
                {
                    // 두 홈 위에 물이 있어도 바닥의 40 mg 한도와 한 번의 건조를 공유한다.
                    auto Coupled = Geometry;
                    Coupled.Liquid.LocalRule = ELocalMaterialRule::Water;
                    Coupled.FilmLayers = {{1700.F, 1.F, 1000.F, 0.F, {}}, PrimaryFilmMaterial(Water)};
                    Coupled.InitialFilmKg = {0.0001F, 0.0001F, 0.F, 0.F};
                    Coupled.bTransport = false;
                    Coupled.TransportSubsteps = 4;
                    Coupled.Materials[0].CapacityKgM2 = 4.e-5F / static_cast<float>(CellArea(Coupled, 0));
                    Coupled.Materials[0].AbsorbPerSecond = 2400.F;
                    Coupled.Materials[0].DryPerSecond = -std::log(0.75F) / StepSeconds;

                    FLocalStepInput Input;
                    Input.Reset(2, 1);
                    Input.LiquidKg.assign(2, 0.0002F);
                    const auto  Cpu = StepReference(Coupled, InitialState(Coupled), {}, StepSeconds, 0.F, {}, &Input);
                    const auto &Cell = Cpu.State.Cells.front();
                    Require(std::abs(Cell.Absorbed - 3.e-5F) < 1.e-9F && std::abs(Cell.Evaporated - 1.e-5F) < 1.e-9F &&
                                std::abs(Cell.Mass - 3.6e-4F) < 1.e-9F,
                            "Both film regions must share one substrate capacity and dry once per fixed step.");
                    if (Context)
                    {
                        FSurfaceSimulationGpu Gpu(*Context, Coupled);
                        const auto            Commands = Context->GetCommands().BeginSingleTime();
                        Gpu.RecordStep(Commands, {}, 0.F, {}, {}, &Input);
                        SubmitForReadback(*Context, Commands);
                        Compare(Gpu.Readback(), Cpu);
                    }
                    std::cout << "contracts: two film regions shared 40 mg absorption capacity, then dried 10 mg once.\n";
                }
                nlohmann::json Measurements = nlohmann::json::array();
                for (const int Inlet : {0, 2})
                {
                    Geometry.SourceSample = Inlet;
                    const auto                  Source = Geometry.SourceRegion();
                    const auto                  Receiver = Geometry.SampleRegions[2 - Inlet];
                    FSurfaceReferenceStepResult Cpu{InitialState(Geometry), {}, {}};
                    auto          Gpu = Context ? std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry) : nullptr;
                    float         Injected = 0.F;
                    float         PreviousGPUReceived = 0.F;
                    std::uint32_t DisplayInterval = 0;
                    int           CompletedSteps = 0;
                    for (const float Dose : {0.0002F, 0.00039F, 0.00011F, 0.F})
                    {
                        Injected += Dose;
                        const auto Commands = Context ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE;
                        double     WorstMassError = 0.0;
                        if (Gpu)
                            Gpu->BeginDisplayInterval();
                        ++DisplayInterval;
                        // 범람이 시작되는 한 스텝의 기록과 후속 여러 스텝의 누적을 모두 확인한다.
                        const int   Steps = Dose == 0.00011F ? 1 : 240;
                        const float IntervalStart = static_cast<float>(CompletedSteps) * StepSeconds;
                        CompletedSteps += Steps;
                        const float IntervalEnd = static_cast<float>(CompletedSteps) * StepSeconds;
                        for (int Step = 0; Step < Steps; ++Step)
                        {
                            const float Input = Step == 0 ? Dose : 0.F;
                            Cpu = StepReference(Geometry, std::move(Cpu.State), {}, StepSeconds, Input);
                            const float Received = Cpu.State.Regions[Receiver].Mass;
                            if (Injected < 0.0006F)
                                Require(Received == 0.F && Cpu.FaceCandidateMassKg.front() == 0.F,
                                        "A dry in-cell sill leaked.");
                            const double Actual = static_cast<double>(Cpu.State.Regions[Source].Mass) + Received;
                            WorstMassError = std::max(WorstMassError, std::abs(Actual - Injected));
                            if (Gpu)
                                Gpu->RecordStep(Commands, {}, Input, {}, {}, nullptr, true);
                        }
                        const auto A = Cpu.State.Regions[Source], B = Cpu.State.Regions[Receiver];
                        Require((Injected < 0.0006F ? B.Mass == 0.F : B.Mass > 0.F) && WorstMassError < 1.e-8,
                                "In-cell basin overflow or conservation failed.");
                        double GpuError = 0.0;
                        float  DisplayedKg = 0.F;
                        if (Gpu)
                        {
                            SubmitForReadback(*Context, Commands);
                            const auto Result = Gpu->Readback();
                            GpuError = Compare(Result, Cpu);
                            Require(GpuError < 1.e-8 && (Injected < 0.0006F ? Result.State.Regions[Receiver].Mass == 0.F
                                                                            : Result.State.Regions[Receiver].Mass > 0.F),
                                    "GPU leaked across or failed to cross the in-cell sill.");
                            const auto GPUReceived = Result.State.Regions[Receiver].Mass;
                            for (const auto &Flow : Gpu->ReadOverflow())
                            {
                                if (Flow.MassKg == 0.F || Flow.DisplayInterval != DisplayInterval)
                                    continue;
                                Require(Flow.LastTransferSeconds > IntervalStart && Flow.LastTransferSeconds <= IntervalEnd,
                                        "Overflow must retain the actual time of its most recent transfer.");
                                Require(
                                    Flow.HeadM > 0.F && Flow.Receiver == Receiver &&
                                        std::ranges::find(Geometry.Contacts, static_cast<int>(Flow.Contact)) !=
                                            Geometry.Contacts.end(),
                                    "Overflow was recorded away from the transferring contact or in the wrong direction.");
                                DisplayedKg += Flow.MassKg;
                            }
                            Require(std::abs(DisplayedKg - (GPUReceived - PreviousGPUReceived)) < 1.e-9F &&
                                        (GPUReceived > PreviousGPUReceived ? DisplayedKg > 0.F : DisplayedKg == 0.F),
                                    "The display interval did not record the transferred mass.");
                            PreviousGPUReceived = GPUReceived;
                        }
                        Measurements.push_back({{"inlet", Inlet},
                                                {"dose_mL", 1000.F * Injected},
                                                {"inlet_level_mm", 1000.F * A.Level},
                                                {"receiver_level_mm", 1000.F * B.Level},
                                                {"receiver_mL", 1000.F * B.Mass},
                                                {"display_interval_overflow_mL", 1000.F * DisplayedKg},
                                                {"cpu_gpu_max_kg", GpuError},
                                                {"ledger_max_kg", WorstMassError}});
                    }
                }
                // 턱의 월드 높이만 바꾸는 자세 입력이 질량 인덱스를 그대로 유지하는지 판정한다.
                auto TiltGrid =
                    MakeGeometry({3, 1, 0.03F, 0.01F, {0.F, 0.006F, 0.F}}, Partition(3, 1), Partition(1, 1), false, Water);
                TiltGrid.InitialLiquidKg = 0.00059F;
                FSurfaceReferenceStepResult Cpu{InitialState(TiltGrid), {}, {}};
                auto       TiltGpu = Context ? std::make_unique<FSurfaceSimulationGpu>(*Context, TiltGrid) : nullptr;
                const int  Source = TiltGrid.SourceRegion();
                const auto Receiver = TiltGrid.SampleRegions.back();
                for (const float Angle : {0.F, TiltRadians})
                {
                    const auto Pose = MakePose(Angle);
                    const auto Commands = Context ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE;
                    for (int Step = 0; Step < 240; ++Step)
                    {
                        Cpu = StepReference(TiltGrid, std::move(Cpu.State), Pose, StepSeconds, 0.F);
                        if (TiltGpu)
                            TiltGpu->RecordStep(Commands, Pose, 0.F);
                    }
                    const auto Received = Cpu.State.Regions[Receiver].Mass;
                    Require(Angle == 0.F ? Received == 0.F : Received > 0.F,
                            "Projected in-cell sill did not respond to tilt.");
                    double GpuError = 0.0;
                    if (TiltGpu)
                    {
                        SubmitForReadback(*Context, Commands);
                        const auto Result = TiltGpu->Readback();
                        GpuError = Compare(Result, Cpu);
                        Require(GpuError < 1.e-8 && (Angle == 0.F ? Result.State.Regions[Receiver].Mass == 0.F
                                                                  : Result.State.Regions[Receiver].Mass > 0.F),
                                "GPU projected in-cell sill did not respond to tilt.");
                    }
                    Measurements.push_back({{"inlet", 0},
                                            {"dose_mL", 0.59},
                                            {"tilt_rad", Angle},
                                            {"sill_flat_mm", 6},
                                            {"inlet_level_mm", 1000.F * Cpu.State.Regions[Source].Level},
                                            {"receiver_mL", 1000.F * Received},
                                            {"cpu_gpu_max_kg", GpuError}});
                }
                // 공유 면의 두 높이가 같아도, 평평한 턱을 넘어 낮은 홈으로 실제 물이 이동한다.
                auto FlatCrest = MakeGeometry({4, 1, 0.04F, 0.01F, {0.F, 0.0027F, 0.0027F, 0.F}},
                                              Partition(4, 2),
                                              Partition(1, 1),
                                              false,
                                              Water);
                FlatCrest.SourceSample = 0;
                const int  CrestSource = FlatCrest.SourceRegion();
                const auto CrestReceiver = FlatCrest.SampleRegions.back();
                FlatCrest.InitialLiquidKg = static_cast<float>(1000.0 * RegionVolume(FlatCrest, CrestSource, {}, 0.0035));
                const auto CrestStep = StepReference(FlatCrest, InitialState(FlatCrest), {}, StepSeconds, 0.F);
                Require(FlatCrest.Contacts.size() == 1 && CrestStep.State.Regions[CrestReceiver].Mass > 0.F,
                        "The flat crest fixture must transfer out of the source on its first step.");
                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, FlatCrest);
                    const auto            Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.BeginDisplayInterval();
                    Gpu.RecordStep(Commands, {}, 0.F, {}, {}, nullptr, true);
                    SubmitForReadback(*Context, Commands);
                    const auto Result = Gpu.Readback();
                    const auto Flows = Gpu.ReadOverflow();
                    const auto Transfer = std::ranges::find_if(Flows, [](const auto &Flow) { return Flow.MassKg > 0.F; });
                    Require(Compare(Result, CrestStep) < 1.e-8 && Transfer != Flows.end() && Transfer->Contact == 2U &&
                                Transfer->Receiver == CrestReceiver &&
                                std::abs(Transfer->MassKg - Result.State.Regions[CrestReceiver].Mass) < 1.e-9F &&
                                Transfer->DisplayInterval == 1U && Transfer->LastTransferSeconds == StepSeconds,
                            "An equal-height contact on an exposed crest must preserve the actual transfer.");
                }
                std::cout << "contracts: basin overflow verified in both directions and at an equal-height contact.\n";
                return Measurements;
            }

            void CheckTiltedSampleVolume(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                constexpr float Pitch = 0.0025F;
                constexpr float Density = 1000.F;
                constexpr float Mass = 6.98411e-8F;
                constexpr float Angle = 0.43633231F;
                for (int Columns : {1, 2})
                {
                    auto Geometry = MakeGeometry({Columns, 1, Pitch * Columns, Pitch, std::vector<float>(Columns)},
                                                 Partition(Columns, 1),
                                                 Partition(1, 1),
                                                 false,
                                                 {Density, 1.F, ELocalMaterialRule::Water});
                    Geometry.InitialLiquidKg = Mass;
                    Require(Geometry.Regions.size() == 1, "The tilted flat bed must have one storage region.");
                    auto Gpu = Context ? std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry) : nullptr;
                    for (float Radians : {Angle, -Angle})
                    {
                        const auto   Pose = MakePose(Radians);
                        const double Drop = static_cast<double>(Geometry.Shape.Width) * std::abs(Pose.Sine);
                        const double ProjectedArea = static_cast<double>(Geometry.Shape.Width) * Pitch * Pose.Cosine;
                        const double EndDepth = std::sqrt(2.0 * Drop * Mass / (Density * ProjectedArea));
                        const auto   Verify = [&](const auto &Result)
                        {
                            const auto Water = Result.State.Regions.front();
                            Require(std::abs(Water.Level + Drop * 0.5 - EndDepth) < 1.e-8 &&
                                        std::abs(Water.Mass - Mass) < 1.e-12 &&
                                        std::abs(RegionVolume(Geometry, 0, Pose, Water.Level) * Density - Mass) < 1.e-10,
                                    "Tilted storage must match the analytic wedge and conserve its mass.");
                        };
                        const auto Cpu = StepReference(Geometry, InitialState(Geometry), Pose, StepSeconds, 0.F);
                        Verify(Cpu);
                        if (Gpu)
                        {
                            const auto Commands = Context->GetCommands().BeginSingleTime();
                            Gpu->RecordStep(Commands, Pose, 0.F);
                            SubmitForReadback(*Context, Commands);
                            Verify(Gpu->Readback());
                        }
                    }
                }
                std::cout << "contracts: analytic tilted wedge verified for one and two samples in both tilt directions.\n";
            }

            void CheckMaximumGrid(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                constexpr int   Extent = MaxCellsPerAxis;
                constexpr int   Steps = 4;
                constexpr float InputMass = 0.0002F;
                const auto      Cuts = Partition(Extent, Extent);
                auto            Geometry = MakeGeometry({Extent, Extent, 0.64F, 0.64F, std::vector<float>(MaxCells)},
                                                        Cuts,
                                                        Cuts,
                                                        true,
                                                        {1000.F, 1.F, ELocalMaterialRule::Water});
                Geometry.SourceSample = MaxCells - 1;
                Geometry.Materials.back().AbsorbPerSecond = 0.5F;
                Geometry.Materials.back().CapacityKgM2 = 1.F;
                const auto Last = Geometry.SourceRegion();
                const auto Adjacent = Geometry.SampleRegions[MaxCells - 2];
                Require(Geometry.Cells.size() == MaxCells && Geometry.Regions.size() == MaxCells &&
                            Geometry.Faces.size() == 2 * MaxCells - Extent && Last == MaxCells - 1,
                        "The maximum grid did not populate the last cell and its boundary.");

                auto       Gpu = Context ? std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry) : nullptr;
                const auto Commands = Context ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE;
                FSurfaceReferenceStepResult Cpu{InitialState(Geometry), {}, {}};
                for (int Step = 0; Step < Steps; ++Step)
                {
                    const float Input = Step == 0 ? InputMass : 0.F;
                    Cpu = StepReference(Geometry, std::move(Cpu.State), {}, StepSeconds, Input);
                    if (Gpu)
                        Gpu->RecordStep(Commands, {}, Input);
                }
                const auto Verify = [&](const auto &Result)
                {
                    const auto Metrics = Sum(Geometry, Result.State, {});
                    Require(std::abs(Metrics.Total() - InputMass) < 1.e-8 && Result.State.Regions[Adjacent].Mass > 0.F &&
                                Result.State.Cells.back().Absorbed > 0.F && Result.State.Cells.back().Outflow > 0.F,
                            "The last cell did not conserve mass while transporting, absorbing and draining.");
                };
                double GpuError = 0.0;
                if (Gpu)
                {
                    SubmitForReadback(*Context, Commands);
                    const auto Actual = Gpu->Readback();
                    Verify(Actual);
                    GpuError = Compare(Actual, Cpu);
                    Require(GpuError < 1.e-8, "Maximum grid CPU and GPU mass differ.");
                }
                Verify(Cpu);
                std::cout << "contracts: maximum grid " << Extent << 'x' << Extent << ", " << Geometry.Faces.size()
                          << " faces";
                if (Context)
                    std::cout << ", CPU/GPU maximum mass delta " << GpuError << " kg";
                std::cout << ".\n";
            }

            void CheckActiveWorkgroups(const GPU::FVulkanContext &Context)
            {
                using namespace Liquid;
                const FHeightfield Bed{32, 8, 0.16F, 0.04F, std::vector<float>(32 * 8, 0.F)};
                const auto         Geometry =
                    MakeGeometry(Bed, Partition(32, 32), Partition(8, 8), true, Asset::LoadLiquidMaterial({}));
                FSurfaceSimulationGpu Active(Context, Geometry);
                FSurfaceSimulationGpu Full(Context, Geometry, false);

                auto Commands = Context.GetCommands().BeginSingleTime();
                Active.RecordStep(Commands, {}, 0.F);
                Full.RecordStep(Commands, {}, 0.F);
                Active.RecordStep(Commands, {}, 0.F);
                Full.RecordStep(Commands, {}, 0.F);
                SubmitForReadback(Context, Commands);
                Require(Compare(Active.Readback(), Full.Readback()) == 0.0,
                        "Empty active workgroups changed the dry state.");

                const int Source = Geometry.SampleRegions.front();
                Commands = Context.GetCommands().BeginSingleTime();
                for (int Step = 0; Step < 24; ++Step)
                {
                    const FLiquidDose Dose{Step == 0 ? 3.e-4F : 0.F, Source};
                    Active.RecordStep(Commands, {}, 0.F, {}, Dose);
                    Full.RecordStep(Commands, {}, 0.F, {}, Dose);
                }
                SubmitForReadback(Context, Commands);
                const auto Sparse = Active.Readback();
                const auto Dense = Full.Readback();
                Require(Compare(Sparse, Dense) == 0.0 && Sparse.FaceCandidateMassKg == Dense.FaceCandidateMassKg &&
                            Sparse.RegionOutflowScale == Dense.RegionOutflowScale && Sparse.State.Regions[1].Mass > 0.F,
                        "Active workgroups changed transport, donor limits, or wetting across a dry boundary.");
                std::cout << "contracts: active workgroups match full dispatch on a dry grid and localized wetting.\n";
            }

            void CheckOperationControls(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const FHeightfield Bed{2, 1, 0.10F, 0.05F, {0.F, 0.F}};
                auto Geometry = MakeGeometry(Bed, Partition(2, 2), Partition(1, 1), true, Asset::LoadLiquidMaterial({}));
                Geometry.InitialLiquidKg = 0.008F;
                FLocalStepInput EditorInput;
                EditorInput.Reset(Geometry.Regions.size(), Geometry.Cells.size());
                EditorInput.LiquidKg[Geometry.SampleRegions[1]] = 0.004F;
                EditorInput.IceKg[1] = 1.e-4F;
                EditorInput.TemperatureK[0] = 310.F;
                constexpr float Inlet = 0.001F;

                for (const bool bExchange : {false, true})
                    for (const bool bTransport : {false, true})
                    {
                        Geometry.bExternalExchange = bExchange;
                        Geometry.bTransport = bTransport;
                        const auto Initial = InitialState(Geometry);
                        const auto Cpu = StepReference(Geometry, Initial, {}, StepSeconds, Inlet, {5.F, 1.F}, &EditorInput);
                        const auto Check = [&](const auto &Result)
                        {
                            for (std::size_t Index = 0; Index < Geometry.Faces.size(); ++Index)
                            {
                                const bool bOutlet = Geometry.Faces[Index].Right < 0;
                                const bool bExpected = bOutlet ? bExchange : bTransport;
                                Require((Result.FaceCandidateMassKg[Index] > 0.F) == bExpected,
                                        "The operation switch did not isolate interior and outlet faces.");
                            }
                            const auto   Metrics = Sum(Geometry, Result.State, {});
                            const double Expected = Initial.Cells[0].Mass + (bExchange ? Inlet + 0.004F + 1.e-4F : 0.F);
                            Require(std::abs(Metrics.Total() - Expected) < 1.e-7 && (Metrics.Outflow > 0.F) == bExchange &&
                                        (Metrics.Ice > 0.F) == bExchange &&
                                        (Result.State.Cells[0].TemperatureK == 310.F) == bExchange &&
                                        Result.State.Regions[Geometry.SampleRegions[0]].Level > 0.F,
                                    "External input isolation, accumulation ledger or level reconstruction failed.");
                        };
                        Check(Cpu);
                        if (Context)
                        {
                            FSurfaceSimulationGpu Gpu(*Context, Geometry);
                            const auto            Commands = Context->GetCommands().BeginSingleTime();
                            Gpu.RecordStep(Commands, {}, Inlet, {5.F, 1.F}, {}, &EditorInput);
                            SubmitForReadback(*Context, Commands);
                            const auto GpuResult = Gpu.Readback();
                            Check(GpuResult);
                            Require(Compare(GpuResult, Cpu) < 1.e-7, "CPU and GPU operation controls disagree.");
                        }
                    }
                FSurfaceSceneRuntime Track;
                Track.PourKg = Inlet;
                Track.PourSeconds = StepSeconds * 1.5F;
                Require(std::abs(ScheduledInput(Track, 0.F) + ScheduledInput(Track, StepSeconds) - Inlet) < 1.e-9F &&
                            ScheduledInput(Track, 2.F * StepSeconds) == 0.F,
                        "Scheduled inlet must honor fractional final steps.");
                Track.PourStartSeconds = StepSeconds * 1.5F;
                Require(ScheduledInput(Track, 0.F) == 0.F &&
                            std::abs(ScheduledInput(Track, StepSeconds) - Inlet / 3.F) < 1.e-9F &&
                            std::abs(ScheduledInput(Track, StepSeconds * 2.F) - Inlet * 2.F / 3.F) < 1.e-9F &&
                            ScheduledInput(Track, StepSeconds * 3.F) == 0.F,
                        "Scheduled inlet must honor fractional start and end times.");
                Track.Geometry.bExternalExchange = false;
                Require(ScheduledInput(Track, StepSeconds * 2.F) == 0.F,
                        "A disabled inlet cannot contribute to the experiment ledger.");
                std::cout << "contracts: interior transport and external exchange controls verified.\n";
            }

            void CheckEditorInputs(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const FHeightfield Bed{2, 1, 0.10F, 0.05F, {0.F, 0.F}};
                const auto         Geometry =
                    MakeGeometry(Bed, Partition(2, 2), Partition(1, 1), false, Asset::LoadLiquidMaterial({}));
                FLocalStepInput First;
                First.Reset(Geometry.Regions.size(), Geometry.Cells.size());
                First.LiquidKg[Geometry.SampleRegions[0]] = 1.e-4F;
                First.LiquidKg[Geometry.SampleRegions[1]] = 1.e-4F;
                First.IceKg[1] = 2.e-4F;
                First.TemperatureK[0] = 315.F;
                FLocalStepInput Second;
                Second.Reset(Geometry.Regions.size(), Geometry.Cells.size());
                Second.TemperatureK[1] = 265.F;

                const auto CpuFirst = StepReference(Geometry, InitialState(Geometry), {}, StepSeconds, 0.F, {}, &First);
                const auto CpuSecond = StepReference(Geometry, CpuFirst.State, {}, StepSeconds, 0.F, {}, &Second);
                const auto CpuThird = StepReference(Geometry, CpuSecond.State, {}, StepSeconds, 0.F);
                const auto Check = [&](const auto &Result, const FSurfaceReferenceStepResult &Reference)
                {
                    for (std::size_t Cell = 0; Cell < Result.State.Cells.size(); ++Cell)
                    {
                        const auto &State = Result.State.Cells[Cell];
                        Require(std::abs(State.Mass - 1.e-4F) < 1.e-9F &&
                                    std::abs(State.Ice - (Cell == 1 ? 2.e-4F : 0.F)) < 1.e-9F &&
                                    std::abs(State.TemperatureK - Reference.State.Cells[Cell].TemperatureK) < 1.e-4F,
                                "The editor dose did not preserve the selected liquid, ice, and temperature inputs.");
                    }
                };
                Require(CpuFirst.State.Cells[0].TemperatureK == 315.F && CpuFirst.State.Cells[1].TemperatureK != 315.F &&
                            CpuSecond.State.Cells[1].TemperatureK == 265.F && CpuThird.State.Cells[1].TemperatureK != 265.F,
                        "Editor temperature must be local and last only while supplied.");
                Check(CpuThird, CpuThird);

                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, Geometry);
                    auto                  Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F, {}, {}, &First);
                    Gpu.RecordStep(Commands, {}, 0.F, {}, {}, &Second);
                    SubmitForReadback(*Context, Commands);
                    Check(Gpu.Readback(), CpuSecond);
                    Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F);
                    SubmitForReadback(*Context, Commands);
                    Check(Gpu.Readback(), CpuThird);
                }
                std::cout << "contracts: local liquid and ice doses, and expiring cell temperature verified.\n";
            }

            void CheckTransportOnlyTemperature(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                auto Geometry = MakeGeometry({2, 1, 0.10F, 0.05F, {0.F, 0.F}},
                                             Partition(2, 2),
                                             Partition(1, 1),
                                             false,
                                             {1000.F, 0.001F, ELocalMaterialRule::None});
                Geometry.Materials[0].TemperatureK = 300.F;
                FLocalStepInput Input;
                Input.Reset(Geometry.Regions.size(), Geometry.Cells.size());
                Input.TemperatureK[0] = 315.F;
                const auto Initial = InitialState(Geometry);
                const auto Heated = StepReference(Geometry, Initial, {}, StepSeconds, 0.F, {}, &Input);
                const auto Released = StepReference(Geometry, Heated.State, {}, StepSeconds, 0.F);
                Require(Initial.Cells[0].TemperatureK == 300.F && Heated.State.Cells[0].TemperatureK == 315.F &&
                            Heated.State.Cells[1].TemperatureK == AmbientTemperatureK &&
                            Released.State.Cells[0].TemperatureK == 300.F,
                        "Substrate temperature must be initialized and respond to a local input without water reactions.");
                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, Geometry);
                    auto                  Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F, {}, {}, &Input);
                    SubmitForReadback(*Context, Commands);
                    Compare(Gpu.Readback(), Heated);
                    Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F);
                    SubmitForReadback(*Context, Commands);
                    Compare(Gpu.Readback(), Released);
                }
                std::cout << "contracts: substrate temperature without water reactions verified.\n";
            }

            void CheckScheduledInlet(const GPU::FVulkanContext *Context)
            {
                using namespace Liquid;
                const auto FlatCuts = Partition(4, 4);
                auto       Flat = MakeGeometry({4, 1, 0.04F, 0.01F, std::vector<float>(4)},
                                               FlatCuts,
                                               Partition(1, 1),
                                               false,
                                               {1000.F, 1.F, ELocalMaterialRule::Water});
                SetScheduledInlet(Flat, {{-0.005F, 0.F}, {0.02F, 0.01F}});
                constexpr std::array Expected{0.25F, 0.5F, 0.25F, 0.F};
                for (std::size_t Region = 0; Region < Expected.size(); ++Region)
                    Require(std::abs(Flat.ScheduledInletWeights[Region] - Expected[Region]) < 1.e-6F,
                            "A partially covered sample received the wrong inlet weight.");

                auto Settings = Defaults(EScene::Runoff);
                Settings.ShapeColumns = Settings.ShapeRows = 64;
                for (const int Extent : {16, 64})
                {
                    Settings.GridColumns = Settings.GridRows = Extent;
                    const auto  Scene = MakeScene(Settings);
                    const auto &Geometry = Scene.Geometry;
                    const auto &Weights = Geometry.ScheduledInletWeights;
                    const auto  InletRegions = std::ranges::count_if(Weights, [](float Weight) { return Weight > 1.e-5F; });
                    Require(InletRegions == (Extent == 16 ? 1 : 16),
                            "The fixed inlet did not cover the expected liquid storage regions.");

                    const auto &Shape = Geometry.Shape;
                    const auto &Inlet = Settings.ScheduledInlet.value();
                    const int   InletX = static_cast<int>((Inlet.CenterM[0] - Shape.EdgeX(0)) / Shape.PitchX());
                    const int   InletY = static_cast<int>((Inlet.CenterM[1] - Shape.EdgeY(0)) / Shape.PitchY());
                    const float InletBed = Shape.Height(InletX, InletY);
                    auto        Gpu = Context ? std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry) : nullptr;
                    const auto  Commands = Context ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE;
                    FSurfaceReferenceStepResult Cpu{InitialState(Geometry), {}, {}};
                    const auto                  Depth = [&](const FSurfaceSimulationState &State)
                    {
                        double Maximum = 0.0;
                        for (std::size_t Region = 0; Region < Weights.size(); ++Region)
                            if (Weights[Region] > 1.e-5F)
                                Maximum = std::max(Maximum, static_cast<double>(State.Regions[Region].Level - InletBed));
                        return Maximum * 1000.0;
                    };
                    double    PeakDepth = 0.0;
                    const int Steps = static_cast<int>(std::round(Scene.PourSeconds / StepSeconds));
                    for (int Step = 0; Step < Steps; ++Step)
                    {
                        const float Time = Step * StepSeconds;
                        const auto  Pose = MakePose(ScheduledAngle(Scene, Time));
                        const float Input = ScheduledInput(Scene, Time);
                        const auto  Thermal = ScheduledTemperature(Scene, Time);
                        Cpu = StepReference(Geometry, std::move(Cpu.State), Pose, StepSeconds, Input, Thermal);
                        PeakDepth = std::max(PeakDepth, Depth(Cpu.State));
                        if (Gpu)
                            Gpu->RecordStep(Commands, Pose, Input, Thermal);
                    }

                    const auto Total = Sum(Geometry, Cpu.State, {}).Total();
                    Require(std::abs(Total - Scene.PourKg) < 1.e-8,
                            "The fixed footprint did not preserve the total scheduled dose.");
                    std::cout << "contracts: scheduled inlet " << Extent << 'x' << Extent << ", " << InletRegions
                              << " regions, " << Total << " kg, peak depth " << PeakDepth << " mm";
                    if (Gpu)
                    {
                        SubmitForReadback(*Context, Commands);
                        const auto Actual = Gpu->Readback();
                        Require(std::abs(Sum(Geometry, Actual.State, {}).Total() - Scene.PourKg) < 1.e-8,
                                "GPU fixed inlet did not preserve the total scheduled dose.");
                        const auto Error = Compare(Actual, Cpu);
                        std::cout << ", CPU/GPU maximum mass delta " << Error << " kg";
                    }
                    std::cout << ".\n";
                }
            }

            nlohmann::json ProfileBasins(const GPU::FVulkanContext &Context)
            {
                using namespace Liquid;
                constexpr int              Extent = 32;
                constexpr int              BatchSize = 32;
                constexpr int              Batches = 110;
                const auto                 Cuts = Partition(Extent, 8);
                const FLiquidMaterial      Water{1000.F, 0.001F, ELocalMaterialRule::None};
                nlohmann::json             Results = nlohmann::json::array();
                VkPhysicalDeviceProperties Properties{};
                vkGetPhysicalDeviceProperties(Context.GetPhysicalDevice(), &Properties);

                for (const bool bManyDivisions : {false, true})
                {
                    FHeightfield Bed{Extent, Extent, 0.32F, 0.32F, std::vector<float>(Extent * Extent)};
                    for (int Y = 0; Y < Extent; ++Y)
                        for (int X = 0; X < Extent; ++X)
                            if ((bManyDivisions || (Y >= 16 && Y < 20 && X < 4)) && X % 4 == 1)
                                Bed.Heights[Y * Extent + X] = 0.006F;

                    const auto Measure = [&](EStorageRegionMode Storage)
                    {
                        auto                     Grid = MakeGeometry(Bed, Cuts, Cuts, false, Water, Storage);
                        FSurfaceSimulationGpu    Gpu(Context, Grid);
                        FSurfaceSimulationTiming Timer(Context);
                        nlohmann::json           Raw = nlohmann::json::array();
                        for (int Batch = 0; Batch < Batches; ++Batch)
                        {
                            const auto Command = Context.GetCommands().BeginSingleTime();
                            Timer.Begin(Command, 0, true);
                            for (int Step = 0; Step < BatchSize; ++Step)
                                Gpu.RecordStep(Command, {}, 1.e-6F);
                            Timer.Mark(Command, 0, ESurfaceSimulationTimestamp::SolverEnd);
                            Timer.Mark(Command, 0, ESurfaceSimulationTimestamp::SnapshotEnd);
                            Timer.Mark(Command, 0, ESurfaceSimulationTimestamp::RefractionPrepassEnd);
                            Timer.Mark(Command, 0, ESurfaceSimulationTimestamp::DrawEnd);
                            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
                            Timer.Collect(0);
                            if (const auto Stats = Timer.Stats().Solver; Stats.Samples > 0)
                                Raw.push_back(Stats.LatestMilliseconds * 1000.0 / BatchSize);
                        }
                        const auto Stats = Timer.Stats().Solver;
                        auto       Sorted = Raw.get<std::vector<double>>();
                        std::ranges::sort(Sorted);
                        return nlohmann::json{{"regions", Grid.Regions.size()},
                                              {"faces", Grid.Faces.size()},
                                              {"median_us", Sorted.empty() ? 0.0 : Sorted[Sorted.size() / 2]},
                                              {"mean_us", Stats.MeanMilliseconds * 1000.0 / BatchSize},
                                              {"p90_us", Stats.P90Milliseconds * 1000.0 / BatchSize},
                                              {"step_us", std::move(Raw)}};
                    };

                    // 순서를 교차해 클럭 워밍업과 연속 실행 순서에 대한 민감도를 드러낸다.
                    for (int Trial = 0; Trial < 4; ++Trial)
                        for (const bool bSplit : {Trial % 2 != 0, Trial % 2 == 0})
                        {
                            auto Result =
                                Measure(bSplit ? EStorageRegionMode::SubgridBasins : EStorageRegionMode::SinglePerCell);
                            Result["trial"] = Trial;
                            Result["storage"] = bSplit ? "subgrid_basins" : "single_per_cell";
                            Result["case"] = bManyDivisions ? "all_cells_split" : "one_cell_split";
                            Results.push_back(std::move(Result));
                        }
                }
                return {{"gpu", Properties.deviceName},
                        {"driver_version", Properties.driverVersion},
                        {"steps_per_batch", BatchSize},
                        {"batches", Batches},
                        {"samples", std::move(Results)}};
            }
        } // namespace

        void CheckReferenceContracts(const GPU::FVulkanContext   *Context,
                                     const std::filesystem::path &Output,
                                     bool                         bProfileBasins)
        {
            using namespace Liquid;
            const FHeightfield Bed{3, 3, 0.03F, 0.03F, std::vector<float>(9)};
            const auto         Partitioned = Partition(3, 3);
            auto               Geometry = MakeGeometry(Bed, Partitioned, Partitioned, false, Asset::LoadLiquidMaterial({}));
            auto               State = InitialState(Geometry);
            std::vector<float> Demands(Geometry.Faces.size());
            constexpr int      Center = 4;
            State.Regions[Center].Mass = 0.001F;
            for (const int Edge : Geometry.RegionFaceRange(Center).In(Geometry.RegionFaces))
                Demands[Edge] = Geometry.Faces[Edge].Left == Center ? 0.0008F : -0.0008F;
            const auto Factors = LimitOutflow(Geometry, State, Demands);
            Require(std::abs(Factors[Center] - 0.3125F) < 1.e-6F, "Four outgoing faces did not share the donor budget.");

            State = InitialState(Geometry);
            State.Cells[Center].Absorbed = 0.001F;
            Geometry.Materials[Center].DryPerSecond = std::log(2.F) * 0.5F;
            for (int Index = 0; Index < 480; ++Index)
                State = StepReference(Geometry, std::move(State), {}, StepSeconds, 0.F).State;
            Require(std::abs(State.Cells[Center].Absorbed - 0.0005F) < 1.e-8F &&
                        std::abs(State.Cells[Center].Evaporated - 0.0005F) < 1.e-8F,
                    "The excess-free drying fixture did not match its two-second half-life.");
            std::cout << "contracts: four-way donor scale " << Factors[Center] << ", two-second drying residual mass "
                      << State.Cells[Center].Absorbed << " kg.\n";

            const auto         Viscous = Asset::LoadLiquidMaterialById("ViscousLiquid");
            const FHeightfield FlatBed{2, 1, 0.1F, 0.05F, {0.F, 0.F}};
            const auto         Columns = Partition(2, 2);
            const auto         Rows = Partition(1, 1);
            auto               Grid = MakeGeometry(FlatBed, Columns, Rows, false, Viscous);
            Grid.InitialLiquidKg = 0.004F;
            const double Area = CellArea(Grid, Grid.SourceCell());
            const double Depth = Grid.InitialLiquidKg / (Area * Viscous.DensityKgM3);
            // With equal pitches and a dry receiver the shared face's span equals its center distance.
            const double Mass = Viscous.DensityKgM3 * Viscous.DensityKgM3 * Gravity * StepSeconds * std::pow(Depth, 4) /
                                (3.0 * Viscous.DynamicViscosityPaS);
            const auto   CheckViscous = [&](const auto &Result)
            {
                Require(std::abs(Result.FaceCandidateMassKg.front() - Mass) < Mass * 5.e-4 &&
                            std::abs(Result.State.Cells[1].Mass - Mass) < Mass * 5.e-4,
                        "Liquid face mass did not use the configured density and viscosity.");
                Require(
                    std::abs(Result.State.Cells[0].Level - (Grid.InitialLiquidKg - Mass) / (Area * Viscous.DensityKgM3)) <
                            1.e-7 &&
                        std::abs(Result.State.Cells[0].Mass + Result.State.Cells[1].Mass - Grid.InitialLiquidKg) < 1.e-8 &&
                        Result.State.Cells[0].Ice == 0.F && Result.State.Cells[0].PhaseLevel == 0.F,
                    "Transport-only liquid level, conservation or rule selection failed.");
            };
            CheckViscous(StepReference(Grid, InitialState(Grid), {}, StepSeconds, 0.F));
            if (Context)
            {
                FSurfaceSimulationGpu Gpu(*Context, Grid);
                const auto            Commands = Context->GetCommands().BeginSingleTime();
                Gpu.RecordStep(Commands, {}, 0.F);
                SubmitForReadback(*Context, Commands);
                CheckViscous(Gpu.Readback());
            }
            std::cout << "contracts: file-defined viscous liquid transport verified.\n";

            // 에디터와 같은 설정 경로에서 물성과 용융 계수 변경이 CPU와 GPU에 도달하는지 확인한다.
            auto       Edited = Defaults(EScene::Textile);
            const auto Baseline = MakeScene(Edited);
            Require(Edited.Profile.has_value(), "The textile demo must load a typed SurfaceProfile.");
            Edited.Profile->Liquid.DynamicViscosityPaS *= 2.F;
            Edited.Profile->Cell.MeltKgM2SecondK = 0.F;
            const auto EditedScene = MakeScene(Edited);
            Require(EditedScene.Geometry.Liquid.DynamicViscosityPaS == 2.F * Baseline.Geometry.Liquid.DynamicViscosityPaS,
                    "The profile's viscosity edit did not reach the transport model.");
            const auto CheckMelt = [&](const FSurfaceSceneRuntime &Scene, bool bExpectMelt)
            {
                const auto &Geometry = Scene.Geometry;
                const auto  Initial = InitialState(Geometry);
                const auto  Validate = [&](const auto &Result)
                {
                    bool bMelted = false;
                    for (std::size_t Cell = 0; Cell < Initial.Cells.size(); ++Cell)
                        bMelted |= Result.State.Cells[Cell].Ice < Initial.Cells[Cell].Ice - 1.e-9F;
                    Require(bMelted == bExpectMelt, "The applied profile did not control snow melting.");
                };
                Validate(StepReference(Geometry, Initial, {}, StepSeconds, 0.F));
                if (Context)
                {
                    FSurfaceSimulationGpu Gpu(*Context, Geometry);
                    const auto            Commands = Context->GetCommands().BeginSingleTime();
                    Gpu.RecordStep(Commands, {}, 0.F);
                    SubmitForReadback(*Context, Commands);
                    Validate(Gpu.Readback());
                }
            };
            CheckMelt(Baseline, true);
            CheckMelt(EditedScene, false);
            auto Disabled = Defaults(EScene::Textile);
            Disabled.bLocalUpdates = false;
            CheckMelt(MakeScene(Disabled), false);
            std::cout << "contracts: typed profile viscosity and zero-melt overrides verified.\n";
            CheckTiltedSampleVolume(Context);
            CheckMaximumGrid(Context);
            CheckScheduledInlet(Context);
            CheckEditorInputs(Context);
            CheckTransportOnlyTemperature(Context);
            CheckOperationControls(Context);
            if (Context)
                CheckActiveWorkgroups(*Context);
            CheckFilmTransport(Context);
            CheckWettedContactAndLayeredSill(Context);
            const auto Overflow = CheckBasinOverflow(Context);
            if (!Output.empty())
            {
                Save(Overflow, Output / "basin-overflow.json");
                if (Context && bProfileBasins)
                    Save(ProfileBasins(*Context), Output / "basin-gpu-timing.json");
            }
        }

    #pragma endregion

    // ============================================================
    // 02. Scenario Experiment
    // ============================================================
    #pragma region 02_Scenario_Experiment
        // 시간별 장부와 수위를 검사하고 GPU가 제공되면 동일한 스텝의 상태를 대조한다.

        namespace
        {
            FExperimentResult RunFilmExperiment(const FSurfaceSceneRuntime  &Scene,
                                                const GPU::FVulkanContext   *Context,
                                                const std::filesystem::path &Output)
            {
                using namespace Liquid;
                const auto &Geometry = Scene.Geometry;
                const auto  Count = Geometry.Regions.size();
                const auto  TotalMass = [](const FSurfaceSimulationState &State)
                {
                    double Total = 0.0;
                    for (const auto &Value : State.Film)
                        Total += Value.Mass;
                    return Total;
                };
                auto         State = InitialState(Geometry);
                const double Initial = TotalMass(State);
                const float  Duration = std::max(8.F, Scene.SecondPourStart + Scene.PourSeconds + 0.7F);
                const int    FramesToCheck = static_cast<int>(Duration / StepSeconds);
                std::unique_ptr<FSurfaceSimulationGpu> Gpu;
                if (Context)
                    Gpu = std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry, false);
                nlohmann::json Series = nlohmann::json::array();
                double         Added = 0, WorstLedger = 0, WorstMass = 0, WorstLevel = 0, WorstMixedWater = 0;
                double         DriedKg = 0, WaterOutKg = 0, RewetKg = 0, MovedAfterRewet = 0, WorstWaterLedger = 0;
                const bool     bTracksSediment =
                    std::ranges::any_of(Geometry.FilmReactions,
                                        [](const FFilmReaction &Rule) { return Rule.WaterPerSoil > 0.F; });
                const double InitialFreeWater =
                    std::accumulate(State.Film.end() - Count,
                                    State.Film.end(),
                                    0.0,
                                    [](double Total, const FFilmRegionState &Value) { return Total + Value.Mass; });
                std::array<double, 2> Branches{};
                std::array<float, 2>  FirstOutlet{-1.F, -1.F};
                std::array<double, 2> SecondBranches{};
                for (int Frame = 0; Frame < FramesToCheck;)
                {
                    const auto Buffer = Gpu ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE;
                    const int  End = std::min(Frame + 60, FramesToCheck);
                    for (; Frame < End; ++Frame)
                    {
                        const float Time = Frame * StepSeconds;
                        const float Input = ScheduledInput(Scene, Time);
                        const auto  Thermal = ScheduledTemperature(Scene, Time);
                        const auto  Pose = MakePose(ScheduledAngle(Scene, Time));
                        Added += Input;
                        auto Cpu = StepReference(Geometry, std::move(State), Pose, StepSeconds, Input, Thermal);
                        State = std::move(Cpu.State);
                        WaterOutKg += Cpu.FilmWaterOutKg;
                        RewetKg += Cpu.FilmRewetKg;
                        if (Scene.SecondPourStart > 0.F && Time >= Scene.SecondPourStart)
                            MovedAfterRewet += Cpu.FilmSolidMovedKg;
                        for (std::size_t I = 0; I < Geometry.Faces.size(); ++I)
                        {
                            const auto Face = Geometry.Faces[I];
                            if (Face.Right >= 0 || Cpu.PrimaryFaceTransferredMassKg[I] <= 0.F)
                                continue;
                            const auto   Contact = Geometry.Contacts[Face.FirstContact] / 2;
                            const int    Branch = Contact / Geometry.Shape.Columns < Geometry.Shape.Rows / 2 ? 0 : 1;
                            const double Mass = Cpu.PrimaryFaceTransferredMassKg[I];
                            Branches[Branch] += Mass;
                            if (FirstOutlet[Branch] < 0.F && Branches[Branch] > 0.0001)
                                FirstOutlet[Branch] = Time + StepSeconds;
                            if (Scene.SecondPourStart > 0.F && Time >= Scene.SecondPourStart)
                                SecondBranches[Branch] += Mass;
                        }
                        if (Gpu)
                            Gpu->RecordStep(Buffer, Pose, Input, Thermal);
                    }
                    if (Gpu)
                    {
                        SubmitForReadback(*Context, Buffer);
                        const auto GpuState = Gpu->Readback().State;
                        Require(GpuState.Film.size() == State.Film.size(), "Layered Vulkan state has the wrong size.");
                        for (std::size_t I = 0; I < State.Film.size(); ++I)
                        {
                            Require(std::isfinite(GpuState.Film[I].Mass) && std::isfinite(GpuState.Film[I].Level),
                                    "Layered Vulkan state contains a nonfinite value.");
                            WorstMass = std::max(WorstMass, double(std::abs(State.Film[I].Mass - GpuState.Film[I].Mass)));
                            WorstMixedWater =
                                std::max(WorstMixedWater,
                                         double(std::abs(State.Film[I].MixedWater - GpuState.Film[I].MixedWater)));
                            WorstLevel =
                                std::max(WorstLevel, double(std::abs(State.Film[I].Level - GpuState.Film[I].Level)));
                        }
                    }
                    double Outlet = 0;
                    DriedKg = 0;
                    for (const auto &Cell : State.Cells)
                    {
                        Outlet += Cell.Outflow;
                        DriedKg += Cell.Evaporated;
                    }
                    WorstLedger = std::max(WorstLedger, std::abs(TotalMass(State) + Outlet + DriedKg - Initial - Added));
                    std::vector<double> LayerMass(Geometry.FilmLayers.size()), MixedWater(Geometry.FilmLayers.size());
                    for (std::size_t I = 0; I < State.Film.size(); ++I)
                    {
                        LayerMass[I / Count] += State.Film[I].Mass;
                        MixedWater[I / Count] += State.Film[I].MixedWater;
                    }
                    auto LayerVolumeMilliliters = LayerMass;
                    for (std::size_t Layer = 0; Layer < LayerMass.size(); ++Layer)
                        LayerVolumeMilliliters[Layer] = 1.e6 * LayerMass[Layer] / Geometry.FilmLayers[Layer].DensityKgM3;
                    if (bTracksSediment)
                    {
                        const double Water = LayerMass.back() + std::accumulate(MixedWater.begin(), MixedWater.end(), 0.0);
                        WorstWaterLedger =
                            std::max(WorstWaterLedger, std::abs(Water + WaterOutKg + DriedKg - InitialFreeWater - Added));
                    }
                    Series.push_back({{"seconds", Frame * StepSeconds},
                                      {"layer_mixed_water_kg", MixedWater},
                                      {"dried_kg", DriedKg},
                                      {"rewet_kg", RewetKg},
                                      {"mud_solids_moved_after_second_pour_kg", MovedAfterRewet},
                                      {"layer_kg", LayerMass},
                                      {"layer_volume_mL", LayerVolumeMilliliters},
                                      {"outlets_water_kg", Branches},
                                      {"second_outlets_water_kg", SecondBranches}});
                }
                const auto Summary = nlohmann::json{{"bed_coupling", Geometry.bFilmBedCoupling},
                                                    {"omitted_support", Geometry.FilmOmittedSupport},
                                                    {"first_outlets_0_1g_s", FirstOutlet},
                                                    {"gpu_checked", Gpu != nullptr},
                                                    {"initial_kg", Initial},
                                                    {"injected_kg", Added},
                                                    {"ledger_error_kg", WorstLedger},
                                                    {"gpu_mass_error_kg", WorstMass},
                                                    {"gpu_level_error_m", WorstLevel},
                                                    {"gpu_mixed_water_error_kg", WorstMixedWater},
                                                    {"water_ledger_error_kg", WorstWaterLedger},
                                                    {"dried_kg", DriedKg},
                                                    {"rewet_kg", RewetKg},
                                                    {"mud_solids_moved_after_second_pour_kg", MovedAfterRewet},
                                                    {"outlets_water_kg", Branches},
                                                    {"second_outlets_water_kg", SecondBranches}};
                std::cout << SceneKeys[static_cast<int>(Scene.Kind)] << ' ' << Summary.dump() << '\n';

                Require(WorstLedger < 1.e-5, "Layered mass ledger exceeded 0.01 g.");
                Require(WorstWaterLedger < 1.e-5, "Layered water ledger exceeded 0.01 g.");
                Require(WorstMass < 3.e-6 && WorstMixedWater < 3.e-6 && WorstLevel < 0.00012,
                        "Layered CPU/GPU state diverged.");
                FExperimentResult Result;
                Result.Report = Summary.dump(2);
                Result.Columns = Geometry.Shape.Columns;
                Result.Rows = Geometry.Shape.Rows;
                for (std::size_t Sample = 0; Sample < Geometry.Shape.Heights.size(); ++Sample)
                {
                    const auto   Region = Geometry.SampleRegions[Sample];
                    double       Bed = Geometry.Shape.Heights[Sample];
                    const double Area =
                        Geometry.Regions[Region].SampleCount * Geometry.Shape.PitchX() * Geometry.Shape.PitchY();
                    for (std::size_t Layer = 0; Layer + 1 < Geometry.FilmLayers.size(); ++Layer)
                        Bed += State.Film[Layer * Count + Region].Mass / (Area * Geometry.FilmLayers[Layer].DensityKgM3);
                    Result.SurfaceMilliliters.push_back(
                        State.Regions[Region].Mass * 1.e6F /
                        (Geometry.FilmLayers.back().DensityKgM3 * Geometry.Regions[Region].SampleCount));
                    Result.AbsorbedGrams.push_back(0.F);
                    Result.DepthMillimeters.push_back(
                        static_cast<float>(1000 * std::max(0.0, State.Regions[Region].Level - Bed)));
                }
                if (!Output.empty())
                {
                    auto Raster = Describe(Scene, State, MakePose(ScheduledAngle(Scene, FramesToCheck * StepSeconds)));
                    for (std::size_t Layer = 0; Layer + 1 < Geometry.FilmLayers.size(); ++Layer)
                    {
                        auto       Heights = nlohmann::json::array();
                        const auto SampleArea = Geometry.Shape.PitchX() * Geometry.Shape.PitchY();
                        for (const auto Region : Geometry.SampleRegions)
                            Heights.push_back(1000.F * State.Film[Layer * Count + Region].Mass /
                                              (SampleArea * Geometry.Regions[Region].SampleCount *
                                               Geometry.FilmLayers[Layer].DensityKgM3));
                        Raster[Layer == 0 ? "lower_layer_mm" : "middle_layer_mm"] = Heights;
                    }
                    Save({{"scene", SceneNames[static_cast<int>(Scene.Kind)]},
                          {"summary", Summary},
                          {"series", Series},
                          {"geometry", Raster}},
                         Output);
                }
                return Result;
            }
        } // namespace

        FExperimentResult RunExperiment(const FSurfaceSceneSettings &Settings,
                                        const GPU::FVulkanContext   *Context,
                                        const std::filesystem::path &Output,
                                        bool                         bAssertScenario)
        {
            using namespace Liquid;
            const auto Setup = MakeScene(Settings);
            if (Setup.Geometry.FilmLayers.size() > 1 || !Setup.Geometry.FilmReactions.empty() ||
                !Settings.FilmPatches.empty())
                return RunFilmExperiment(Setup, Context, Output);
            const auto                            &Geometry = Setup.Geometry;
            FSurfaceReferenceStepResult            Cpu{InitialState(Geometry), {}, {}};
            const auto                             Initial = Sum(Geometry, Cpu.State, {});
            std::unique_ptr<FSurfaceSimulationGpu> Gpu;
            if (Context)
                Gpu = std::make_unique<FSurfaceSimulationGpu>(*Context, Geometry);
            const auto Cmd = [&]() { return Context ? Context->GetCommands().BeginSingleTime() : VK_NULL_HANDLE; };

            FExperimentResult Result;
            const bool        bMeasureCavity =
                Settings.Scene == EScene::Cavity && Settings.GridColumns == 2 && Settings.GridRows == 1;
            const bool bPhase = Settings.Scene == EScene::FreezeThaw;
            const bool bCharring = Settings.Scene == EScene::Charring;
            const bool bMeadow = Settings.Scene == EScene::Meadow;
            const bool bQuench = Geometry.SubstrateQuench.ArealHeatCapacityJm2K > 0.F;
            const bool bEnergyPulse = Settings.TemperatureRamp && Settings.TemperatureRamp->bFromEnergyPulse;
            const auto PhaseVolume = [&Geometry](const FMetrics &State)
            { return State.Surface / Geometry.Liquid.DensityKgM3 + State.Ice / IceDensity; };
            double                PeakLiquid = Initial.Surface, PeakIce = Initial.Ice;
            double                PeakTemperatureDeviationK = Initial.TemperatureDeviationK;
            double                MaxPhaseVolume = 0.0, MaxPhaseVolumeError = 0.0;
            const double          InitialLevel = Cpu.State.Cells[Geometry.SourceCell()].PhaseLevel;
            double                HighestLevel = InitialLevel;
            std::optional<double> SpillTime;
            std::optional<double> FirstFlameTime;
            std::optional<double> FirstSteamTime;
            std::optional<double> FarBankFlameTime;
            float                 PeakFlameAfterIgnition = 0.F;
            FMetrics              Preheat;
            nlohmann::json        Series = nlohmann::json::array();
            double                Injected = 0.0, MaxLedgerError = 0.0, MaxVolumeError = 0.0, MaxGpuError = 0.0;
            float                 MaxTemperatureErrorK = 0.F;
            float                 MaxSteamErrorKgS = 0.F;
            float                 MaxCharError = 0.F, MaxFlameError = 0.F;
            double                PeakAbsorbed = 0.0, PeakFlame = 0.0, PeakSteam = 0.0;
            double                MeadowVaporKg = 0.0;
            double                FirstContactAreaM2 = 0.0, FirstAbsorbedKg = 0.0;
            std::optional<float>  FirstOutletSeconds;
            std::uint64_t         HeadLimitedFaces = 0, DonorLimitedRegions = 0;
            FMetrics              Metrics = Initial;
            FSurfaceTilt          Pose{};
            for (int Start = 0; Start < Frames; Start += SampleInterval)
            {
                const auto Buffer = Cmd();
                float      Time = 0.F;
                for (int Frame = Start; Frame < Start + SampleInterval; ++Frame)
                {
                    Time = static_cast<float>(Frame) * StepSeconds;
                    Pose = MakePose(ScheduledAngle(Setup, Time));
                    const float Input = ScheduledInput(Setup, Time);
                    Injected += Input;
                    const auto Thermal = ScheduledTemperature(Setup, Time);
                    Cpu = StepReference(Geometry, std::move(Cpu.State), Pose, StepSeconds, Input, Thermal);
                    HeadLimitedFaces += Cpu.HeadLimitedFaces;
                    DonorLimitedRegions += Cpu.DonorLimitedRegions;
                    if (Frame == 0)
                    {
                        FirstContactAreaM2 = Cpu.ContactAreaM2;
                        for (const auto &Cell : Cpu.State.Cells)
                            FirstAbsorbedKg += Cell.Absorbed;
                    }
                    if (!FirstOutletSeconds)
                    {
                        double OutletKg = 0.0;
                        for (const auto &Cell : Cpu.State.Cells)
                            OutletKg += Cell.Outflow;
                        if (OutletKg >= 1.e-5)
                            FirstOutletSeconds = Time + StepSeconds;
                    }
                    if (bMeasureCavity && !SpillTime &&
                        Injected - Cpu.State.Cells[Geometry.SourceCell()].Mass >= VisibleSpillKg)
                        SpillTime = static_cast<double>(Frame + 1) * StepSeconds;
                    if (bCharring && !FirstFlameTime &&
                        std::ranges::any_of(Cpu.State.Cells,
                                            [](const FSurfaceCellState &Cell) { return Cell.Flame > 0.F; }))
                        FirstFlameTime = static_cast<double>(Frame + 1) * StepSeconds;
                    if (bQuench && !FirstSteamTime &&
                        std::ranges::any_of(Cpu.State.Cells,
                                            [](const FSurfaceCellState &Cell) { return Cell.SteamRate > 0.F; }))
                        FirstSteamTime = static_cast<double>(Frame + 1) * StepSeconds;
                    if (bMeadow)
                    {
                        for (std::size_t Cell = 0; Cell < Geometry.Cells.size(); ++Cell)
                        {
                            const auto &Bounds = Geometry.Cells[Cell];
                            const float V = (Bounds.FirstRow + Bounds.EndRow) / (2.F * Geometry.Shape.Rows);
                            const auto &State = Cpu.State.Cells[Cell];
                            const float Flame = State.Flame;
                            MeadowVaporKg += static_cast<double>(State.SteamRate) * StepSeconds;
                            if (!FirstSteamTime && State.SteamRate > 0.F)
                                FirstSteamTime = static_cast<double>(Frame + 1) * StepSeconds;
                            if (Time > 4.5F && V > 0.32F)
                                PeakFlameAfterIgnition = std::max(PeakFlameAfterIgnition, Flame);
                            if (V > 0.72F && Flame > 0.1F && !FarBankFlameTime)
                                FarBankFlameTime = Time + StepSeconds;
                        }
                    }
                    if (Gpu)
                        Gpu->RecordStep(Buffer, Pose, Input, Thermal);
                }
                if (Gpu)
                {
                    SubmitForReadback(*Context, Buffer);
                    const auto Actual = Gpu->Readback();
                    // 결합 전파에는 탄화 온도 구간(200 K)의 1%를 허용한다.
                    // 최대 활성도 기울기는 1.3 / 200 K이며 탄화율은 활성도의 적분이다.
                    auto Reaction = Geometry.bEvaporateAbsorbedWater ? FReactionTolerance{2.F, 0.003F, 0.015F}
                                    : bQuench ? FReactionTolerance{.TemperatureK = 0.3F, .SteamKgS = 1.e-5F}
                                              : FReactionTolerance{};
                    if (bEnergyPulse)
                    {
                        // 가열 보간의 결합 연산에 따른 반올림 차이를 최고 목표 온도의 두 ULP 이내로 제한한다.
                        const float Target = Settings.TemperatureRamp->TargetK[0];
                        const float Interval = std::nextafter(Target, std::numeric_limits<float>::infinity()) - Target;
                        Reaction.TemperatureK = std::max(Reaction.TemperatureK, 2.F * Interval);
                    }
                    MaxGpuError = std::max(MaxGpuError, Compare(Actual, Cpu, Reaction));
                    if (bMeadow || bQuench)
                        for (std::size_t Cell = 0; Cell < Geometry.Cells.size(); ++Cell)
                        {
                            const auto &GpuCell = Actual.State.Cells[Cell];
                            const auto &CpuCell = Cpu.State.Cells[Cell];
                            MaxTemperatureErrorK =
                                std::max(MaxTemperatureErrorK, std::abs(GpuCell.TemperatureK - CpuCell.TemperatureK));
                            MaxSteamErrorKgS = std::max(MaxSteamErrorKgS, std::abs(GpuCell.SteamRate - CpuCell.SteamRate));
                            MaxCharError = std::max(MaxCharError, std::abs(GpuCell.Char - CpuCell.Char));
                            MaxFlameError = std::max(MaxFlameError, std::abs(GpuCell.Flame - CpuCell.Flame));
                        }
                    const auto GpuMetrics = Sum(Geometry, Actual.State, Pose);
                    Require(std::abs(GpuMetrics.Total() - (Initial.Total() + Injected)) < 5.e-7,
                            "GPU ledger did not balance.");
                }
                Metrics = Sum(Geometry, Cpu.State, Pose);
                MaxLedgerError = std::max(MaxLedgerError, std::abs(Metrics.Total() - Initial.Total() - Injected));
                MaxVolumeError = std::max(MaxVolumeError, Metrics.Reconstruction);
                PeakLiquid = std::max(PeakLiquid, Metrics.Surface);
                PeakIce = std::max(PeakIce, Metrics.Ice);
                PeakTemperatureDeviationK = std::max(PeakTemperatureDeviationK, Metrics.TemperatureDeviationK);
                PeakAbsorbed = std::max(PeakAbsorbed, Metrics.Absorbed);
                PeakFlame = std::max(PeakFlame, Metrics.Flame);
                PeakSteam = std::max(PeakSteam, Metrics.Steam);
                Result.SurfaceMilliliters.push_back(static_cast<float>(Metrics.Surface * 1000));
                Result.AbsorbedGrams.push_back(static_cast<float>(Metrics.Absorbed * 1000));
                Series.push_back({{"seconds", Time + StepSeconds},
                                  {"surface_g", Metrics.Surface * 1000},
                                  {"ice_g", Metrics.Ice * 1000},
                                  {"absorbed_g", Metrics.Absorbed * 1000},
                                  {"outlet_g", Metrics.Outflow * 1000},
                                  {"evaporated_g", Metrics.Evaporated * 1000},
                                  {"flame", Metrics.Flame},
                                  {"char", Metrics.Char},
                                  {"steam_g_s", Metrics.Steam * 1000}});
                if (bQuench || bEnergyPulse)
                    Series.back()["hottest_temperature_K"] = Metrics.HottestK;
                if (bCharring)
                {
                    Series.back()["hottest_temperature_K"] = Metrics.HottestK;
                    if (std::abs(Time + StepSeconds - 1.F) < StepSeconds * 0.5F)
                        Preheat = Metrics;
                }
                if (bMeasureCavity)
                {
                    Series.back()["pocket_mL"] = Cpu.State.Cells[Geometry.SourceCell()].Mass * 1000;
                    Series.back()["received_mL"] = (Injected - Cpu.State.Cells[Geometry.SourceCell()].Mass) * 1000;
                }
                if (bPhase)
                {
                    const double Volume = PhaseVolume(Metrics);
                    MaxPhaseVolume = std::max(MaxPhaseVolume, Volume);
                    double Reconstructed = 0.0;
                    for (int Cell = 0; Cell < static_cast<int>(Cpu.State.Cells.size()); ++Cell)
                        Reconstructed += VolumeAtLevel(Geometry, Cell, Pose, Cpu.State.Cells[Cell].PhaseLevel);
                    const auto &Source = Cpu.State.Cells[Geometry.SourceCell()];
                    HighestLevel = std::max(HighestLevel, static_cast<double>(Source.PhaseLevel));
                    MaxPhaseVolumeError = std::max(MaxPhaseVolumeError, std::abs(Reconstructed - Volume));
                    Series.back()["phase_volume_mL"] = Volume * 1.e6;
                    Series.back()["phase_level_mm"] = Source.PhaseLevel * 1000;
                    Series.back()["phase_temperature_K"] = Source.TemperatureK;
                    Series.back()["water_total_g"] = (Metrics.Surface + Metrics.Ice) * 1000;
                }
            }
            Require(MaxLedgerError < 5.e-7, "CPU mass ledger did not balance.");
            Require(MaxVolumeError < 5.e-7, "Reconstructed mass did not match stored surface mass.");
            VerifyActiveStateDeclarations(Settings,
                                          PeakLiquid,
                                          PeakIce,
                                          PeakAbsorbed,
                                          Metrics.Char,
                                          PeakTemperatureDeviationK);
            if (bEnergyPulse)
            {
                const float Target = Settings.TemperatureRamp->TargetK[0];
                Require(PeakLiquid == 0.0 && PeakSteam == 0.0 && std::abs(Metrics.HottestK - Target) < 1.e-3,
                        "The dry heat-capacity case did not reach its energy-based target without liquid.");
            }
            else if (bAssertScenario)
                VerifyScenario(Settings.Scene, Setup, Cpu.State, Metrics, Initial.Ice, PeakAbsorbed, PeakFlame, PeakSteam);
            const auto Index = static_cast<int>(Settings.Scene);
            const auto BedRange = std::ranges::minmax_element(Geometry.Shape.Heights);
            double     DownstreamMass = Metrics.Outflow;
            for (std::size_t Cell = 0; Cell < Cpu.State.Cells.size(); ++Cell)
                if (Geometry.Cells[Cell].FirstColumn >= Geometry.Shape.Columns / 2)
                    DownstreamMass += Cpu.State.Cells[Cell].Mass;
            nlohmann::json Summary{
                {"bed_relief_mm", 1000.F * (*BedRange.max - *BedRange.min)},
                {"downstream_mass_kg", DownstreamMass},
                {"bed_ice_ior", Setup.Appearance.bIceSubstrate ? Geometry.Liquid.IceRefractiveIndex : 0.F},
                {"metal_substrate", Setup.Appearance.bMetalSubstrate},
                {"mass_ledger_error_kg", MaxLedgerError},
                {"reconstructed_mass_error_kg", MaxVolumeError},
                {"peak_absorbed_kg", PeakAbsorbed},
                {"peak_steam_kg_s", PeakSteam},
                {"peak_flame", PeakFlame},
                {"final_char", Metrics.Char},
                {"source_mass_kg", Injected},
                {"initial_ice_kg", Initial.Ice},
                {"receiver_kg", Cpu.State.Cells.size() > 1 ? Cpu.State.Cells[1].Mass : 0.F},
                {"final_surface_kg", Metrics.Surface},
                {"final_absorbed_kg", Metrics.Absorbed},
                {"final_ice_kg", Metrics.Ice},
                {"final_sink_kg", Metrics.Evaporated},
                {"final_outlet_kg", Metrics.Outflow},
                {"gpu_checked", Context != nullptr},
                {"cpu_gpu_mass_error_kg", MaxGpuError},
                {"scenario_checks", bAssertScenario},
                {"transport_substeps", Geometry.TransportSubsteps},
                {"head_limited_faces", HeadLimitedFaces},
                {"donor_limited_regions", DonorLimitedRegions},
                {"first_contact_area_mm2", FirstContactAreaM2 * 1.e6},
                {"first_absorbed_kg", FirstAbsorbedKg}};
            if (FirstOutletSeconds)
                Summary["first_outlet_10mg_s"] = *FirstOutletSeconds;
            if (bQuench)
            {
                double CoolingJ = 0.0;
                float  MaxTemperatureDropK = 0.F;
                for (int Cell = 0; Cell < static_cast<int>(Geometry.Cells.size()); ++Cell)
                {
                    const float InitialK = Geometry.Materials[Cell].TemperatureK;
                    const float FinalK = Cpu.State.Cells[Cell].TemperatureK;
                    const float DropK = InitialK - FinalK;
                    Require(DropK >= -1.e-3F && FinalK >= std::min(373.15F, InitialK) - 1.e-3F,
                            "Contact cooling must remain between the initial temperature and the boiling point.");
                    CoolingJ += DropK * Geometry.SubstrateQuench.ArealHeatCapacityJm2K * CellArea(Geometry, Cell);
                    MaxTemperatureDropK = std::max(MaxTemperatureDropK, DropK);
                }
                const double WaterEnergyJ = Metrics.Evaporated * WaterVaporizationJPerKg;
                const double EnergyResidualJ = std::abs(CoolingJ - WaterEnergyJ);
                Require(EnergyResidualJ < std::max(0.1, 1.e-4 * WaterEnergyJ),
                        "Substrate heat loss must match the energy assigned to vapor generation.");
                Summary["quench"] = {{"areal_heat_capacity_J_m2_K", Geometry.SubstrateQuench.ArealHeatCapacityJm2K},
                                     {"initial_hottest_temperature_K", Initial.HottestK},
                                     {"max_temperature_drop_K", MaxTemperatureDropK},
                                     {"substrate_heat_loss_J", CoolingJ},
                                     {"vaporization_energy_J", WaterEnergyJ},
                                     {"energy_residual_J", EnergyResidualJ}};
                if (FirstSteamTime)
                    Summary["quench"]["first_steam_seconds"] = *FirstSteamTime;
                if (Context)
                {
                    Summary["cpu_gpu_temperature_error_K"] = MaxTemperatureErrorK;
                    Summary["cpu_gpu_steam_error_kg_s"] = MaxSteamErrorKgS;
                }
            }
            if (bMeadow)
            {
                if (Context)
                {
                    Summary["cpu_gpu_temperature_error_K"] = MaxTemperatureErrorK;
                    Summary["cpu_gpu_char_error"] = MaxCharError;
                    Summary["cpu_gpu_flame_error"] = MaxFlameError;
                }
                Summary["peak_flame_outside_ignition"] = PeakFlameAfterIgnition;
                Summary["heat_driven_vapor_kg"] = MeadowVaporKg;
                if (FirstSteamTime)
                    Summary["first_steam_seconds"] = *FirstSteamTime;
                Summary["far_bank_flame_seconds"] =
                    FarBankFlameTime ? nlohmann::json(*FarBankFlameTime) : nlohmann::json(nullptr);
                if (bAssertScenario)
                    Require(PeakFlameAfterIgnition > 0.1F, "Meadow flame went out when the igniter was removed.");
            }
            if (bCharring)
            {
                if (bAssertScenario)
                {
                    Require(std::abs(Initial.HottestK - AmbientTemperatureK) < 1.e-4 &&
                                Preheat.HottestK > Initial.HottestK + 40.0 && Preheat.Flame == 0.0 && Preheat.Char == 0.0 &&
                                FirstFlameTime && *FirstFlameTime > 1.0,
                            "Wood did not warm up before the first flame and char.");
                }
                Summary["initial_hottest_temperature_K"] = Initial.HottestK;
                Summary["hottest_temperature_at_1s_K"] = Preheat.HottestK;
                if (FirstFlameTime)
                    Summary["first_flame_seconds"] = *FirstFlameTime;
                Summary["flame_at_1s"] = Preheat.Flame;
                Summary["char_at_1s"] = Preheat.Char;
            }
            if (bMeasureCavity)
            {
                const auto Cavity = MeasureCavity(Geometry);
                if (bAssertScenario)
                {
                    Require(Cavity.DepthMillimeters > 3.0 && Cavity.CapacityMilliliters > 1.0 && SpillTime.has_value(),
                            "The input did not reconstruct a pocket that fills and spills.");
                    const double StaticFillTime = Cavity.CapacityMilliliters / (1000 * Setup.PourKg / Setup.PourSeconds);
                    Require(*SpillTime + StepSeconds >= StaticFillTime,
                            "Water crossed the cavity sill before the pocket filled.");
                }
                Summary["cavity"] = {{"sill_depth_mm", Cavity.DepthMillimeters},
                                     {"capacity_at_sill_mL", Cavity.CapacityMilliliters},
                                     {"spill_time_s", SpillTime ? nlohmann::json(*SpillTime) : nlohmann::json(nullptr)},
                                     {"spill_threshold_mL", VisibleSpillKg * 1000}};
            }
            if (bEnergyPulse)
                Summary["energy_pulse_temperature_K"] = {{"initial", Initial.HottestK}, {"final", Metrics.HottestK}};
            if (bPhase)
            {
                const double ExpectedSolid = Initial.Surface / IceDensity;
                if (bAssertScenario)
                {
                    Require(PeakIce > Initial.Surface * 0.999 && Initial.Ice == 0.0,
                            "The cold interval did not fully freeze the sample.");
                    Require(std::abs(MaxPhaseVolume - ExpectedSolid) < 5.e-10 &&
                                std::abs(PhaseVolume(Metrics) - PhaseVolume(Initial)) < 5.e-10 &&
                                MaxPhaseVolumeError < 5.e-10 && HighestLevel > InitialLevel,
                            "The phase volume did not match the density and tray geometry.");
                }
                Summary["phase_cycle"] = {{"initial_mass_g", Initial.Surface * 1000},
                                          {"peak_ice_g", PeakIce * 1000},
                                          {"initial_volume_mL", PhaseVolume(Initial) * 1.e6},
                                          {"peak_volume_mL", MaxPhaseVolume * 1.e6},
                                          {"final_volume_mL", PhaseVolume(Metrics) * 1.e6},
                                          {"initial_level_mm", InitialLevel * 1000},
                                          {"peak_level_mm", HighestLevel * 1000},
                                          {"final_level_mm", Cpu.State.Cells[Geometry.SourceCell()].PhaseLevel * 1000},
                                          {"phase_geometry_error_mL", MaxPhaseVolumeError * 1.e6}};
            }
            const auto Raster = Describe(Setup, Cpu.State, Pose);
            Result.Columns = Geometry.Shape.Columns;
            Result.Rows = Geometry.Shape.Rows;
            Result.DepthMillimeters = Raster["depth_mm"].get<std::vector<float>>();
            if (Settings.Scene == EScene::Runoff)
            {
                // 벽돌의 홈으로 급수하면 가장 높은 외곽 테두리는 이 데모의 급수량에서 마른 상태로 남아야 한다.
                int WetRimSamples = 0;
                for (std::size_t Sample = 0; Sample < Geometry.Shape.Heights.size(); ++Sample)
                    if (Geometry.Shape.Heights[Sample] >= *BedRange.max - 1.e-6F && Result.DepthMillimeters[Sample] > 0.F)
                        ++WetRimSamples;
                Summary["wet_rim_samples"] = WetRimSamples;
                if (bAssertScenario)
                    Require(WetRimSamples == 0, "The runoff demo wet the rim at the prescribed dose.");
            }
            if (!Settings.ComparisonProfiles.empty())
            {
                struct FSpecimenWater
                {
                    double Initial = 0.0;
                    double Absorbed = 0.0;
                    double Surface = 0.0;
                    double Capacity = 0.0;
                };
                std::vector<FSpecimenWater> Samples(Settings.SpecimenCount());
                const int                   Width = Geometry.Shape.Columns / Settings.SpecimenCount();
                const auto                  SpecimenAt = [&](int Cell) { return Geometry.Cells[Cell].FirstColumn / Width; };
                const auto                  Starting = InitialState(Geometry);
                for (int Cell = 0; Cell < static_cast<int>(Geometry.Cells.size()); ++Cell)
                {
                    auto &Sample = Samples[SpecimenAt(Cell)];
                    Sample.Initial += Starting.Cells[Cell].Mass;
                    Sample.Absorbed += Cpu.State.Cells[Cell].Absorbed;
                    Sample.Surface += Cpu.State.Cells[Cell].Mass;
                    Sample.Capacity += Geometry.Materials[Cell].CapacityKgM2 * CellArea(Geometry, Cell);
                }
                if (bAssertScenario)
                {
                    for (int Y = 0; Y < Geometry.Shape.Rows; ++Y)
                        for (int X = Width; X < Geometry.Shape.Columns; ++X)
                            Require(Geometry.Shape.Height(X % Width, Y) == Geometry.Shape.Height(X, Y),
                                    "Specimens must use matching geometry at every corresponding sample.");
                    for (const auto &Face : Geometry.Faces)
                        Require(Face.Right >= 0 && SpecimenAt(Geometry.Regions[Face.Left].Cell) ==
                                                       SpecimenAt(Geometry.Regions[Face.Right].Cell),
                                "Water can cross between specimens or leave their closed domain.");
                    constexpr double ToleranceKg = 1.e-6;
                    for (const auto &Sample : Samples)
                    {
                        Require(std::abs(Samples.front().Initial - Sample.Initial) < ToleranceKg &&
                                    std::abs(Sample.Initial - Sample.Absorbed - Sample.Surface) < ToleranceKg,
                                "The specimens did not receive and retain the same mass.");
                        Require(Sample.Absorbed <= Sample.Capacity + ToleranceKg,
                                "A specimen exceeded the absorption capacity.");
                        if (Sample.Capacity < Sample.Initial)
                            Require(std::abs(Sample.Absorbed - Sample.Capacity) < ToleranceKg,
                                    "A low-capacity specimen did not saturate.");
                        if (Sample.Capacity >= Sample.Initial && Sample.Initial > ToleranceKg)
                            Require(Sample.Absorbed > 0.0, "A high-capacity specimen did not absorb water.");
                    }
                    auto Ordered = Samples;
                    std::ranges::sort(Ordered, {}, &FSpecimenWater::Capacity);
                    Require(std::ranges::is_sorted(Ordered, {}, &FSpecimenWater::Absorbed),
                            "Larger capacities should retain at least as much absorbed water.");
                }
                for (std::size_t Index = 0; Index < Samples.size(); ++Index)
                {
                    const auto &Sample = Samples[Index];
                    const auto &Profile = Index == 0 ? *Settings.Profile : Settings.ComparisonProfiles[Index - 1];
                    Summary["substrate_comparison"][Profile.Substrate.Id] = {{"initial_g", Sample.Initial * 1000},
                                                                             {"capacity_g", Sample.Capacity * 1000},
                                                                             {"absorbed_g", Sample.Absorbed * 1000},
                                                                             {"surface_g", Sample.Surface * 1000}};
                }
            }
            std::ostringstream Caption;
            Caption << std::setprecision(3) << "Mass residual: " << MaxLedgerError * 1e6 << " mg\n"
                    << "Volume reconstruction: " << MaxVolumeError * 1e6 << " mg equivalent\n";
            if (Context)
                Caption << "CPU/GPU mass delta: " << MaxGpuError * 1e6 << " mg\n";
            if (bMeasureCavity)
            {
                Caption << "Storage at sill: " << Summary["cavity"]["capacity_at_sill_mL"].get<double>() << " mL\n";
                if (SpillTime)
                    Caption << "Spill (0.01 mL received): " << *SpillTime << " s\n";
            }
            if (bPhase)
                Caption << "Volume: " << PhaseVolume(Initial) * 1.e6 << " -> " << MaxPhaseVolume * 1.e6 << " -> "
                        << PhaseVolume(Metrics) * 1.e6 << " mL\n";
            Caption << "Peak absorbed: " << PeakAbsorbed * 1000 << " g\n"
                    << "Final charring: " << Metrics.Char * 100 << "%";
            Result.Report = Caption.str();
            std::cout << SceneKeys[Index] << ' ' << Summary.dump() << '\n';
            if (!Output.empty())
                Save({{"scene", SceneNames[Index]},
                      {"purpose", ScenePurpose[Index]},
                      {"summary", Summary},
                      {"series", Series},
                      {"geometry", Raster},
                      {"simulation_cells", Geometry.Cells.size()},
                      {"liquid_regions", Geometry.Regions.size()},
                      {"simulation_grid", {Settings.GridColumns, Settings.GridRows}},
                      {"geometry_source", static_cast<int>(Settings.Source)}},
                     Output);
            return Result;
        }

    #pragma endregion

    // ============================================================
    // 03. State Export
    // ============================================================
    #pragma region 03_State_Export
        // 현재 계산 셀 상태를 형상 샘플의 배열로 내보낸다.

        FExperimentResult ExportState(const FSurfaceSceneSettings           &Settings,
                                      const FSurfaceSceneRuntime            &Scene,
                                      const Liquid::FSurfaceSimulationState &State,
                                      Liquid::FSurfaceTilt                   Pose,
                                      const std::filesystem::path           &Output)
        {
            const auto Raster = Describe(Scene, State, Pose);
            Save({{"scene", SceneNames[static_cast<int>(Settings.Scene)]}, {"geometry", Raster}}, Output);
            FExperimentResult Preview;
            Preview.Columns = Scene.Geometry.Shape.Columns;
            Preview.Rows = Scene.Geometry.Shape.Rows;
            Preview.DepthMillimeters = Raster["depth_mm"].get<std::vector<float>>();
            return Preview;
        }

    #pragma endregion

    // ============================================================
    // 04. Experiment Image Export
    // ============================================================
    #pragma region 04_Experiment_Image_Export

        void GenerateExperimentPlot(const std::filesystem::path &Report, bool bKeepData)
        {
            const std::filesystem::path Script = MDSS_SURFACE_PLOT_SCRIPT;
            bool                        bSucceeded = false;
                                                                                                                                                                                            #if defined( \
                                                                                                                                                                                                _WIN32)
            std::wstring Command = L"python.exe \"" + Script.wstring() + L"\" \"" + Report.wstring() + L"\"";
            if (!bKeepData)
                Command += L" --png-only";
            STARTUPINFOW Startup{};
            Startup.cb = sizeof(Startup);
            PROCESS_INFORMATION Child{};
            if (CreateProcessW(nullptr,
                               Command.data(),
                               nullptr,
                               nullptr,
                               FALSE,
                               CREATE_NO_WINDOW,
                               nullptr,
                               nullptr,
                               &Startup,
                               &Child))
            {
                CloseHandle(Child.hThread);
                DWORD Result = 1;
                bSucceeded = WaitForSingleObject(Child.hProcess, INFINITE) == WAIT_OBJECT_0 &&
                             GetExitCodeProcess(Child.hProcess, &Result) && Result == 0;
                CloseHandle(Child.hProcess);
            }
                                                                                                                                                                                            #else
            char        Python[] = "python3";
            char        PngOnly[] = "--png-only";
            std::string ScriptName = Script.string(), ReportName = Report.string();
            char *Arguments[] = {Python, ScriptName.data(), ReportName.data(), bKeepData ? nullptr : PngOnly, nullptr};
            pid_t Child = 0;
            int   Status = 0;
            bSucceeded = posix_spawnp(&Child, Python, nullptr, nullptr, Arguments, environ) == 0 &&
                         waitpid(Child, &Status, 0) == Child && WIFEXITED(Status) && WEXITSTATUS(Status) == 0;
                                                                                                                                                                                            #endif
            if (!bSucceeded)
                throw std::runtime_error("Could not generate the PNG. Raw data saved at " + Report.string());
        }

    #pragma endregion

} // namespace MDSS::SurfaceSimulation
