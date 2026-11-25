/**
 * @file SurfaceSimulation.cpp
 * @brief Surface Simulation의 실행 모드와 고정 스텝 입력, 프레임 표시를 조율한다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSimulation.h"
#include "Application/ApplicationLaunchOptions.h"
#include "Application/Editor/SurfaceSimulation/Experiments.h"
#include "Application/Editor/SurfaceSimulation/GeometryExperiments.h"
#include "Application/Editor/SurfaceSimulation/SceneAsset.h"
#include "Application/Editor/SurfaceSimulation/SurfaceSimulationUI.h"
#include "Application/EngineConfig.h"
#include "Application/Window.h"
#include "AssetManager/Assets/TextureAsset.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "DebugUI/Panels/EditorLayoutStyle.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "SurfaceState/Simulation/SurfaceLocalUpdates.h"
#include "SurfaceState/Simulation/SurfaceSimulationGpu.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <deque>
#include <future>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <iostream>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <utility>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Application and Runtime Controls
    // ============================================================
    #pragma region 00_Application_and_Runtime_Controls
        // 인터랙티브 실행의 리소스 수명과 고정 스텝 실행 및 검사를 관리한다.

        namespace
        {
            constexpr int MaxSubsteps = 8;

            bool IsHeatmapView(ESurfaceSimulationView View)
            {
                using enum ESurfaceSimulationView;
                return UsesStateSource(View) || View == Bed || View == Temperature || View == Activity;
            }

            bool HasTemperatureState(std::span<const Rendering::FStateVisualization> States)
            {
                return std::ranges::any_of(States,
                                           [](const auto &State)
                                           { return State.Channel == Rendering::ESurfaceStateChannel::Temperature; });
            }

            bool IsStateGroupView(ESurfaceSimulationView View)
            {
                return View == ESurfaceSimulationView::LayerStates || View == ESurfaceSimulationView::SubstrateStates;
            }

            std::uint32_t ResolveStateSource(ESurfaceSimulationView                          View,
                                             std::span<const Rendering::FStateVisualization> States,
                                             std::uint32_t                                   Preferred)
            {
                if (!UsesStateSource(View) || (Preferred < States.size() && SupportsStateSource(View, States[Preferred])))
                    return Preferred;
                for (std::uint32_t Slot = 0; Slot < States.size(); ++Slot)
                    if (SupportsStateSource(View, States[Slot]))
                        return Slot;
                return NoSurfaceStateSource;
            }

            const char *ViewName(ESurfaceSimulationView                          View,
                                 std::span<const Rendering::FStateVisualization> States,
                                 std::uint32_t                                   StateIndex)
            {
                if (View == ESurfaceSimulationView::StateHeatmap && StateIndex < States.size())
                    return States[StateIndex].Name.c_str();
                return SurfaceSimulationViewNames[static_cast<std::size_t>(View)];
            }

            void DrawViewLabel(ImDrawList                                     &Draw,
                               ImVec2                                          Min,
                               ESurfaceSimulationView                          View,
                               int                                             Palette,
                               std::span<const Rendering::FStateVisualization> States,
                               std::uint32_t                                   StateIndex,
                               const FSurfaceSceneRuntime                     &Scene,
                               bool                                            bShowEmptyGroup,
                               bool                                            bTemperature,
                               std::optional<glm::vec2>                        TemperatureRangeK)
            {
                using enum ESurfaceSimulationView;
                StateIndex = ResolveStateSource(View, States, StateIndex);
                const auto      Name = ViewName(View, States, StateIndex);
                const auto      Title = ImGui::CalcTextSize(Name);
                const bool      bGroup = IsStateGroupView(View);
                const bool      bSubstrate = View == ESurfaceSimulationView::SubstrateStates;
                constexpr float SwatchSpace = 15.F;
                if (bGroup && !bShowEmptyGroup &&
                    std::ranges::none_of(States,
                                         [bSubstrate](const auto &State) { return State.bSubstrate == bSubstrate; }))
                    return;
                float Width = Title.x;
                float Height = Title.y;
                if (bGroup)
                {
                    for (const auto &State : States)
                    {
                        if (State.bSubstrate != bSubstrate)
                            continue;
                        Width = std::max(Width, SwatchSpace + ImGui::CalcTextSize(State.Name.c_str()).x);
                        Height += Title.y + 2.F;
                    }
                }

                const bool  bPhysical = View == ContactArea || View == EffectiveBed || View == SolverDepth ||
                                        View == OverflowTransfers || View == AbsorptionFill;
                const bool  bMetric = View == LayerThickness || View == ArealMass || bPhysical || bTemperature;
                const bool  bSingle = UsesStateSource(View) && StateIndex < States.size();
                const bool  bRamp = bSingle && !bTemperature && Palette == AutomaticHeatmapPalette;
                std::string Range;
                if (bTemperature)
                {
                    char Actual[96]{};
                    if (TemperatureRangeK)
                        std::snprintf(Actual,
                                      sizeof(Actual),
                                      "Min / max  %.1f / %.1f \xC2\xB0"
                                      "C",
                                      TemperatureRangeK->x - 273.15F,
                                      TemperatureRangeK->y - 273.15F);
                    else
                        std::snprintf(Actual, sizeof(Actual), "Reading temperature...");
                    Range = Actual;
                }
                else if (bSingle)
                {
                    const auto &State = States[StateIndex];
                    const float Maximum =
                        View == LayerThickness
                            ? ThicknessDisplayMaxMm(State, Scene.Geometry.Liquid, Scene.Appearance.Snow.BulkDensityKgM3)
                            : State.DisplayMaximum;
                    const char *Unit = View == LayerThickness ? "mm"
                                       : View == ArealMass    ? "kg/m\xC2\xB2"
                                                              : State.DisplayUnit.c_str();
                    char        Limits[64]{};
                    const float Minimum =
                        State.Channel == Rendering::ESurfaceStateChannel::Temperature ? Liquid::AmbientTemperatureK : 0.F;
                    std::snprintf(Limits, sizeof(Limits), "%.4g-%.4g %s", Minimum, Maximum, Unit);
                    Range = bMetric ? State.Name + "   " + Limits : Limits;
                }
                else if (bPhysical)
                {
                    switch (View)
                    {
                        case ContactArea:
                        Range = "Dry / other wet / reaction contact";
                        break;
                        case EffectiveBed:
                        Range = "Chart height + supporting layers";
                        break;
                        case SolverDepth:
                        Range = "0-4 mm  •  unsmoothed";
                        break;
                        case OverflowTransfers:
                        Range = "0-1 mg  •  arrow points downstream";
                        break;
                        case AbsorptionFill:
                        Range = "0-100% of storage limit";
                        break;
                        default:
                        break;
                    }
                }
                else if (bMetric)
                    Range = "No compatible state";
                const float RangeWidth = (bRamp ? 36.F : 8.F) + ImGui::CalcTextSize(Range.c_str()).x;
                if (bMetric)
                {
                    Width = std::max(Width, RangeWidth);
                    Height += Title.y + 2.F;
                }
                else if (bSingle)
                    Width += RangeWidth;
                Draw.AddRectFilled({Min.x + 5.F, Min.y + 5.F},
                                   {Min.x + Width + 15.F, Min.y + Height + 11.F},
                                   IM_COL32(12, 17, 24, 217),
                                   3.F);
                const float X = Min.x + 10.F;
                float       Y = Min.y + 8.F;
                Draw.AddText({X, Y}, IM_COL32_WHITE, Name);
                const float RangeX = bMetric ? X : X + Title.x + 8.F;
                const float RangeY = bMetric ? Y + Title.y + 2.F : Y;
                const auto  Color = [](const auto &State)
                { return ImGui::ColorConvertFloat4ToU32({State.Color.r, State.Color.g, State.Color.b, 1.F}); };
                if (bRamp)
                {
                    const float     RampX = RangeX;
                    const float     RampY = RangeY + (Title.y - 7.F) * 0.5F;
                    const ImU32     High = Color(States[StateIndex]);
                    constexpr ImU32 Low = IM_COL32(46, 51, 60, 255);
                    Draw.AddRectFilledMultiColor({RampX, RampY}, {RampX + 22.F, RampY + 7.F}, Low, High, High, Low);
                }
                if (!Range.empty())
                    Draw.AddText({RangeX + (bRamp ? 28.F : 0.F), RangeY}, IM_COL32(180, 194, 208, 255), Range.c_str());
                if (!bGroup)
                    return;
                for (const auto &State : States)
                {
                    if (State.bSubstrate != bSubstrate)
                        continue;
                    Y += Title.y + 2.F;
                    Draw.AddRectFilled({X, Y + 3.F}, {X + 8.F, Y + 11.F}, Color(State), 1.F);
                    Draw.AddText({X + SwatchSpace, Y}, IM_COL32_WHITE, State.Name.c_str());
                }
            }

            void
            DrawOutletOverlay(ImDrawList &Draw, ImVec2 Min, ImVec2 Max, std::span<const std::array<glm::vec2, 2>> Edges)
            {
                constexpr ImU32 Accent = IM_COL32(255, 188, 73, 255);
                constexpr ImU32 Shadow = IM_COL32(12, 17, 24, 235);
                const auto      ToPixel = [Min, Max](glm::vec2 UV)
                { return ImVec2{Min.x + UV.x * (Max.x - Min.x), Min.y + UV.y * (Max.y - Min.y)}; };

                for (const auto &Edge : Edges)
                {
                    const auto A = ToPixel(Edge[0]), B = ToPixel(Edge[1]);
                    Draw.AddLine(A, B, Shadow, 5.F);
                    Draw.AddLine(A, B, Accent, 2.5F);
                }
            }

            void DrawWaterSourceOverlay(ImDrawList                 &Draw,
                                        ImVec2                      Min,
                                        ImVec2                      Max,
                                        const FSurfaceInletOverlay &Overlay,
                                        const char                 *Label,
                                        bool                        bPouring)
            {
                const ImU32     Accent = bPouring ? IM_COL32(242, 15, 25, 255) : IM_COL32(223, 25, 34, 235);
                const ImU32     Fill = IM_COL32(221, 8, 12, bPouring ? 108 : 78);
                constexpr ImU32 Shadow = IM_COL32(12, 17, 24, 232);
                const auto      ToPixel = [Min, Max](glm::vec2 UV)
                { return ImVec2{Min.x + UV.x * (Max.x - Min.x), Min.y + UV.y * (Max.y - Min.y)}; };
                // 인접 표본끼리 공유하는 변에서 반투명 채움이 겹치지 않게 한다.
                const auto SavedFlags = Draw.Flags;
                Draw.Flags &= ~ImDrawListFlags_AntiAliasedFill;
                for (const auto &Quad : Overlay.Quads)
                    Draw.AddQuadFilled(ToPixel(Quad[0]), ToPixel(Quad[1]), ToPixel(Quad[2]), ToPixel(Quad[3]), Fill);
                Draw.Flags = SavedFlags;
                for (const auto &Edge : Overlay.Edges)
                {
                    const auto A = ToPixel(Edge[0]), B = ToPixel(Edge[1]);
                    Draw.AddLine(A, B, Shadow, bPouring ? 6.F : 4.F);
                    Draw.AddLine(A, B, Accent, bPouring ? 3.5F : 2.F);
                }

                if (Label == nullptr)
                    return;
                const auto   TextSize = ImGui::CalcTextSize(Label);
                const ImVec2 Bottom{Max.x - 7.F, Max.y - 7.F};
                const ImVec2 Top{std::max(Min.x + 7.F, Bottom.x - TextSize.x - 10.F), Bottom.y - TextSize.y - 6.F};
                Draw.AddRectFilled(Top, Bottom, Shadow, 3.F);
                Draw.AddText({Top.x + 5.F, Top.y + 3.F}, Accent, Label);
            }

            void ResetSceneCamera(FSurfaceSimulationViews &Views, const FSurfaceSceneSettings &Settings)
            {
                const EScene Scene = Settings.Scene;
                if (Settings.SectionWindow.HeightMeters > 0.F)
                {
                    // 단면 눈금에 가까운 시점에서도 뒤쪽의 수면은 함께 보인다.
                    const bool bGlassVessel = Settings.SectionWindow.GlassThicknessMeters > 0.F;
                    Views.CameraAzimuth = bGlassVessel ? -65.F : -84.F;
                    Views.CameraElevation = 25.F;
                    Views.CameraDistanceScale = bGlassVessel ? 0.72F : 0.93F;
                    Views.CameraOffset = {};
                    return;
                }
                const bool bElevated = Scene == EScene::Relief || Scene == EScene::IceBed;
                const bool bCavity = Scene == EScene::Cavity;
                const bool bPhase = Scene == EScene::FreezeThaw;
                const bool bComparison = Scene == EScene::AbsorptionComparison;
                Views.CameraAzimuth = bComparison ? -82.F : bElevated ? -55.F : bCavity || bPhase ? -71.F : -62.62F;
                Views.CameraElevation = Scene == EScene::IceBed    ? 42.F
                                        : bElevated                ? 28.F
                                        : bCavity                  ? 44.F
                                        : bPhase                   ? 34.F
                                        : bComparison              ? 45.F
                                        : Scene == EScene::Textile ? 23.F
                                                                   : 40.31F;
                Views.CameraDistanceScale = Scene == EScene::Runoff ? 1.20F : Scene == EScene::IceBed ? 1.16F : 1.F;
                Views.CameraOffset = {};
            }

            void SetViewMode(FSurfaceSimulationViews &Views, int Index, int Mode)
            {
                Views.Modes[Index] = Mode;
                if (Mode == static_cast<int>(ESurfaceSimulationView::Pbr))
                    Views.SmoothWater[Index] = true;
            }

            void SetLayerAndSubstrateViews(FSurfaceSimulationViews &Views)
            {
                using enum ESurfaceSimulationView;
                Views.Count = 4;
                Views.Modes = {static_cast<int>(Pbr),
                               static_cast<int>(SubstrateStates),
                               static_cast<int>(LayerStates),
                               static_cast<int>(LayerThickness)};
                SetViewMode(Views, 0, static_cast<int>(Pbr));
            }

            void SetSceneView(FSurfaceSimulationViews &Views, const FSurfaceSceneSettings &Settings)
            {
                ResetSceneCamera(Views, Settings);
                const EScene Scene = Settings.Scene;
                const bool   bElevated = Scene == EScene::Relief || Scene == EScene::IceBed;
                const bool   bPhase = Scene == EScene::FreezeThaw;
                Views.SmoothWater.fill(!bPhase);
                SetViewMode(Views, 0, static_cast<int>(DefaultSurfaceSimulationView));
                Views.Modes[1] = static_cast<int>(bElevated ? ESurfaceSimulationView::Elevation
                                                  : bPhase  ? ESurfaceSimulationView::LayerStates
                                                  : Scene == EScene::Charring || Scene == EScene::Compound
                                                      ? ESurfaceSimulationView::Temperature
                                                      : ESurfaceSimulationView::LayerThickness);
            }

            [[nodiscard]] std::filesystem::path OutputName(const char *Key, const char *Suffix)
            {
                const auto Now = std::chrono::system_clock::now().time_since_epoch();
                const auto Millis = std::chrono::duration_cast<std::chrono::milliseconds>(Now).count();
                return GetEngineConfigDirectory().parent_path() / "Captures/Surface" /
                       (std::string(Key) + '-' + std::to_string(Millis) + Suffix);
            }

            class FRun
            {
                public:
                FRun(const GPU::FVulkanContext &GpuContext, FWindow *LabWindow, FSurfaceSceneSettings Settings)
                    : Context(GpuContext),
                      Window(LabWindow),
                      Active(std::move(Settings)),
                      Scene(MakeScene(Active)),
                      Simulation(std::make_unique<Liquid::FSurfaceSimulationGpu>(Context, Scene.Geometry)),
                      Display(MakeDisplay(Scene, Active))
                {
                }

                void Reset(const FSurfaceSceneSettings &Settings)
                {
                    AdoptReset(FRun(Context, Window, Settings));
                }

                void AdoptReset(FRun &&Replacement)
                {
                    Active = std::move(Replacement.Active);
                    Scene = std::move(Replacement.Scene);
                    Simulation = std::move(Replacement.Simulation);
                    Display = std::move(Replacement.Display);
                    Steps = 0;
                    Pending = 0.0;
                    ScheduledInletKg = 0.0;
                    PreparedInletKg = 0.0;
                    Doses.clear();
                    ThermalPulses.clear();
                    LastPulseEnd = 0;
                    Pose = {};
                }

                void QueueInjection(FSurfaceSimulationUI                                  &Ui,
                                    std::vector<FSurfaceInjectionFootprint::FRegionWeight> Weights)
                {
                    const EInjectionTarget Target = Ui.InjectionTarget;
                    if (Target == EInjectionTarget::PrimaryLiquid &&
                        std::ranges::none_of(Active.States,
                                             [](const auto &State)
                                             { return State.Channel == Rendering::ESurfaceStateChannel::LiquidMass; }))
                    {
                        Ui.InputMessage = "This scene has no surface liquid state.";
                        return;
                    }
                    if (Target == EInjectionTarget::Ice && !Liquid::UsesWaterMaterial(Scene.Geometry))
                    {
                        Ui.InputMessage = "This scene has no local material input for the selected target.";
                        return;
                    }
                    if (Target == EInjectionTarget::Ice &&
                        std::ranges::find(Active.States,
                                          Rendering::ESurfaceStateChannel::IceMass,
                                          &Rendering::FStateVisualization::Channel) == Active.States.end())
                    {
                        Ui.InputMessage = "This scene has no snow or ice layer.";
                        return;
                    }

                    const std::size_t Regions = Weights.size();
                    const auto        Duration = static_cast<std::size_t>(
                        std::ceil(static_cast<double>(Ui.TemperatureDuration) / Liquid::StepSeconds));
                    const float Amount = Target == EInjectionTarget::Temperature ? Ui.TargetTemperatureC + 273.15F
                                         : Target == EInjectionTarget::Ice
                                             ? Ui.IceDoseGrams * 0.001F
                                             : Ui.DoseMilliliters * 1.e-6F * Scene.Geometry.Liquid.DensityKgM3;
                    Doses.push_back({Target, Amount, Duration, std::move(Weights)});
                    char Message[144]{};
                    if (Target == EInjectionTarget::Temperature)
                        std::snprintf(Message,
                                      sizeof(Message),
                                      "Temperature pulse %.1f C in %zu regions.",
                                      Ui.TargetTemperatureC,
                                      Regions);
                    else if (Target == EInjectionTarget::Ice)
                        std::snprintf(Message,
                                      sizeof(Message),
                                      "Injected %.3f g of solid water into %zu regions.",
                                      Ui.IceDoseGrams,
                                      Regions);
                    else
                        std::snprintf(Message,
                                      sizeof(Message),
                                      "Injected %.3f mL of %s into %zu regions.",
                                      Ui.DoseMilliliters,
                                      Scene.Geometry.Liquid.bIncandescent ? "lava" : "water",
                                      Regions);
                    Ui.InputMessage = Message;
                    FLogger::Info("SurfaceSimulation", Ui.InputMessage);
                    if (!Ui.bPlaying)
                        Ui.bSingleStep = true;
                }

                std::size_t
                PrepareInputs(FSurfaceSimulationUI &Ui, double Elapsed, std::span<Liquid::FSurfaceStepInput> Inputs)
                {
                    using namespace Liquid;
                    if (Ui.bPlaying)
                        Pending = std::min(0.25, Pending + Elapsed * Ui.PlaybackRate);
                    if (Ui.bSingleStep)
                        Pending = StepSeconds;
                    std::size_t Count = 0;
                    PreparedInletKg = 0.0;
                    while ((Ui.bPlaying || Ui.bSingleStep) && Count < Inputs.size() &&
                           Pending >= static_cast<double>(Count + 1) * StepSeconds)
                    {
                        const float Time = static_cast<float>(Steps + Count) * StepSeconds;
                        const float Angle = Active.Scene == EScene::FreezeThaw ? 0.F
                                            : Ui.bAutoTilt                     ? ScheduledAngle(Scene, Time)
                                                                               : Ui.ManualTiltDegrees * 0.01745329252F;
                        const float ScheduledMass = Ui.bScheduledInlet ? ScheduledInput(Scene, Time) : 0.F;
                        const auto  Thermal = Ui.bScheduledHeat
                                                  ? ScheduledTemperature(Scene, Time)
                                                  : FThermalInput{0.F, Scene.Geometry.InitialTemperatureWeight};
                        PreparedInletKg += ScheduledMass;
                        Inputs[Count++] = {MakePose(Angle), ScheduledMass, Thermal};
                    }
                    RecordSurfaceInputs(Inputs.first(Count));
                    return Count;
                }

                FSurfaceSimulationViews FrameViews(const FSurfaceSimulationUI &Ui) const
                {
                    return FrameViews(Ui, Ui.Views);
                }

                FSurfaceSimulationViews FrameViews(const FSurfaceSimulationUI &Ui, FSurfaceSimulationViews Views) const
                {
                    for (int Index = 0; Index < Views.Count; ++Index)
                        Views.StateIndices[Index] =
                            ResolveStateSource(static_cast<ESurfaceSimulationView>(Views.Modes[Index]),
                                               Active.States,
                                               Views.StateIndices[Index]);
                    if (Ui.bAnimateCamera)
                    {
                        const auto Camera = ScheduledCamera(Active.Scene, static_cast<float>(Steps) * Liquid::StepSeconds);
                        Views.CameraAzimuth += Camera.Azimuth;
                        Views.CameraElevation = std::clamp(Views.CameraElevation + Camera.Elevation, -89.F, 89.F);
                        Views.CameraDistanceScale *= Camera.DistanceScale;
                    }
                    return Views;
                }

                void CommitFrame(std::span<const Liquid::FSurfaceStepInput> Inputs)
                {
                    if (Inputs.empty())
                        return;
                    Pending -= static_cast<double>(Inputs.size()) * Liquid::StepSeconds;
                    Steps += Inputs.size();
                    ScheduledInletKg += PreparedInletKg;
                    Pose = Inputs.back().Pose;
                    for (std::size_t Index = 0; Index < Inputs.size() && !Doses.empty(); ++Index)
                        Doses.pop_front();
                }

                [[nodiscard]] bool HasOpenOutlet() const
                {
                    return Active.ComparisonProfiles.empty() && Active.bExternalExchange &&
                           Active.OpenOutletOverride.value_or(Active.bOpenOutlet);
                }

                bool IsScheduledInletActive(const FSurfaceSimulationUI &Ui) const
                {
                    return Active.bExternalExchange && Ui.bPlaying && Ui.bScheduledInlet &&
                           ScheduledInput(Scene, static_cast<float>(Steps) * Liquid::StepSeconds) > 0.F;
                }

                std::string WaterSourceStatus(const FSurfaceSimulationUI &Ui) const
                {
                    std::array<char, 96> Text{};
                    if (Scene.PourKg > 0.F)
                    {
                        const float Time = static_cast<float>(Steps) * Liquid::StepSeconds;
                        const float End = std::max(Scene.PourStartSeconds, Scene.SecondPourStart) + Scene.PourSeconds;
                        const float TargetKg = Scene.SecondPourStart > 0.F ? 2.F * Scene.PourKg : Scene.PourKg;
                        const char *Status = "Auto inlet running";
                        if (!Active.bExternalExchange)
                            Status = "Auto inlet blocked by solver";
                        else if (Time >= End)
                            Status =
                                ScheduledInletKg >= TargetKg * (1.0 - 1.e-5) ? "Auto inlet complete" : "Auto inlet ended";
                        else if (!Ui.bScheduledInlet)
                            Status = "Auto inlet disabled";
                        else if (!Ui.bPlaying)
                            Status = "Auto inlet paused";
                        else if (ScheduledInput(Scene, Time) <= 0.F)
                            Status = "Auto inlet waiting";
                        std::snprintf(Text.data(),
                                      Text.size(),
                                      "%s  %.3g / %.3g g",
                                      Status,
                                      ScheduledInletKg * 1000.0,
                                      TargetKg * 1000.0);
                        return Text.data();
                    }
                    if (Scene.Geometry.InitialLiquidKg > 0.F)
                    {
                        std::snprintf(Text.data(),
                                      Text.size(),
                                      !Active.ComparisonProfiles.empty() ? "Initial water  %.3g g per specimen"
                                                                         : "Initial water  %.3g g",
                                      1000.0 * Scene.Geometry.InitialLiquidKg / Active.SpecimenCount());
                        return Text.data();
                    }
                    if (Active.bLocalUpdates && Active.Scene == EScene::Textile)
                        return "Water source  Snowmelt";
                    if (Active.bLocalUpdates && Active.Scene == EScene::IceBed)
                        return "Water source  Ice melt";
                    return {};
                }

                void Action(FSurfaceSimulationUI &Ui, ESurfaceSimulationAction Action, std::filesystem::path &Capture)
                {
                    switch (Action)
                    {
                        case ESurfaceSimulationAction::None:
                        case ESurfaceSimulationAction::Snapshot:
                        case ESurfaceSimulationAction::SnapshotWithData:
                        case ESurfaceSimulationAction::CpuCheck:
                        case ESurfaceSimulationAction::GpuCheck:
                        return;
                        case ESurfaceSimulationAction::Restart:
                        case ESurfaceSimulationAction::Apply:
                        {
                            const auto &Settings = Action == ESurfaceSimulationAction::Restart ? Active : Ui.Draft;
                            const bool  bSceneChanged = Active.Scene != Settings.Scene;
                            auto        Selections = Ui.Views.StateIndices;
                            for (auto &Index : Selections)
                            {
                                if (Index >= Active.States.size())
                                {
                                    Index = 0;
                                    continue;
                                }
                                const auto &Id = Active.States[Index].Id;
                                const auto  Entry = std::ranges::find(Settings.States, Id, &Rendering::FStateVisualization::Id);
                                Index = Entry == Settings.States.end()
                                        ? 0U
                                        : static_cast<std::uint32_t>(Entry - Settings.States.begin());
                            }
                            Reset(Settings);
                            if (Action == ESurfaceSimulationAction::Restart)
                            Ui.bPlaying = !Active.bStartPaused;
                            Ui.bSingleStep = false;
                            Ui.Views.StateIndices = Selections;
                            if (bSceneChanged && Window)
                            SetSceneView(Ui.Views, Active);
                        }
                        Ui.InputMessage.clear();
                        Ui.Message = "Scene restarted.";
                        return;
                        case ESurfaceSimulationAction::ResetCamera:
                        ResetSceneCamera(Ui.Views, Active);
                        Ui.bAnimateCamera = false;
                        return;
                        case ESurfaceSimulationAction::Capture:
                        Capture = OutputName(SceneKeys[static_cast<int>(Active.Scene)], ".png");
                        Ui.Message = "Saving " + Capture.string();
                        return;
                    }
                }

                const GPU::FVulkanContext                     &Context;
                FWindow                                       *Window;
                FSurfaceSceneSettings                          Active;
                FSurfaceSceneRuntime                           Scene;
                std::unique_ptr<Liquid::FSurfaceSimulationGpu> Simulation;
                std::unique_ptr<FSurfaceSimulationDisplay>     Display;
                std::size_t                                    Steps = 0;
                double                                         Pending = 0.0;
                Liquid::FSurfaceTilt                           Pose{};

                private:
                struct FQueuedDose
                {
                    EInjectionTarget                                       Target;
                    float                                                  Amount;
                    std::size_t                                            DurationSteps;
                    std::vector<FSurfaceInjectionFootprint::FRegionWeight> Regions;
                };
                struct FThermalPulse
                {
                    float       TemperatureK = 0.F;
                    std::size_t EndStep = 0;
                };
                // 제출된 자동 급수만 집계한다. 수동 급수는 고정 스텝 입력에 나중에 더한다.
                double                               PreparedInletKg = 0.0;
                double                               ScheduledInletKg = 0.0;
                std::deque<FQueuedDose>              Doses;
                std::vector<FThermalPulse>           ThermalPulses;
                std::vector<Liquid::FLocalStepInput> StepInputs;
                std::size_t                          LastPulseEnd = 0;

                void RecordSurfaceInputs(std::span<Liquid::FSurfaceStepInput> Inputs)
                {
                    if (Inputs.empty() || (Doses.empty() && Steps >= LastPulseEnd))
                        return;

                    StepInputs.resize(Inputs.size());
                    for (std::size_t Index = 0; Index < Inputs.size(); ++Index)
                    {
                        const auto Step = Steps + Index;
                        if (Index >= Doses.size() && Step >= LastPulseEnd)
                            break;
                        auto &Values = StepInputs[Index];
                        Values.Reset(Scene.Geometry.Regions.size(), Scene.Geometry.Cells.size());
                        if (Index < Doses.size())
                            Deposit(Doses[Index], Step, Values);
                        if (Step < LastPulseEnd)
                            for (std::size_t Cell = 0; Cell < ThermalPulses.size(); ++Cell)
                                if (ThermalPulses[Cell].EndStep > Step)
                                    Values.TemperatureK[Cell] = ThermalPulses[Cell].TemperatureK;
                        Inputs[Index].LocalInput = &Values;
                    }
                }

                void Deposit(const FQueuedDose &Dose, std::size_t Step, Liquid::FLocalStepInput &Values)
                {
                    const auto &Geometry = Scene.Geometry;
                    if (Dose.Target == EInjectionTarget::Temperature)
                    {
                        ThermalPulses.resize(Geometry.Cells.size());
                        const auto End = Step + Dose.DurationSteps;
                        LastPulseEnd = std::max(LastPulseEnd, End);
                        for (const auto &[Region, Fraction] : Dose.Regions)
                            ThermalPulses[Geometry.Regions[Region].Cell] = {Dose.Amount, End};
                    }
                    else if (Dose.Target == EInjectionTarget::Ice)
                    {
                        for (const auto &[Region, Fraction] : Dose.Regions)
                            Values.IceKg[Geometry.Regions[Region].Cell] += Dose.Amount * Fraction;
                    }
                    else
                    {
                        for (const auto &[Region, Fraction] : Dose.Regions)
                            Values.LiquidKg[Region] += Dose.Amount * Fraction;
                    }
                }

                std::unique_ptr<FSurfaceSimulationDisplay> MakeDisplay(const FSurfaceSceneRuntime  &Source,
                                                                       const FSurfaceSceneSettings &Settings)
                {
                    if (Window)
                        return std::make_unique<FSurfaceSimulationDisplay>(Context,
                                                                           *Window,
                                                                           Source.Geometry,
                                                                           Source.Appearance,
                                                                           Settings.States,
                                                                           Settings.bSmoothDisplay);
                    return std::make_unique<FSurfaceSimulationDisplay>(Context,
                                                                       Source.Geometry,
                                                                       Source.Appearance,
                                                                       Settings.States,
                                                                       Settings.bSmoothDisplay);
                }
            };

            glm::vec2 ComparisonFitSize(std::span<const FSurfaceSceneSettings> Cases)
            {
                glm::vec2 Size{};
                for (const auto &Case : Cases)
                    Size = glm::max(Size, glm::vec2(Case.SizeM[0], Case.SizeM[1]));
                return Size;
            }

            std::filesystem::path ComparisonCapturePath(const std::filesystem::path &Path, int Side)
            {
                const char Suffix[] = {'-', static_cast<char>('A' + Side), '\0'};
                return Path.parent_path() / (Path.stem().string() + Suffix + Path.extension().string());
            }

            void RunCapture(const GPU::FVulkanContext       &Context,
                            FWindow                         &Window,
                            const FApplicationLaunchOptions &Options,
                            FSurfaceSceneSettings            Settings,
                            glm::vec2                        CameraFitSize = {})
            {
                using namespace Liquid;
                Settings.LiquidMaterialPath = Options.LabLiquidMaterial;
                FRun                 Run(Context, &Window, Settings);
                FSurfaceSimulationUI Ui;
                Ui.bAnimateCamera = Options.bLabAnimateCamera;
                Ui.SetDraft(Run.Active);
                Ui.Views.Count = Options.LabViews;
                Ui.Views.CameraFitSize = CameraFitSize;
                SetSceneView(Ui.Views, Run.Active);
                if (Options.LabShading >= 0)
                    SetViewMode(Ui.Views, 0, Options.LabShading);
                if (Options.LabViewModeCount > 0)
                {
                    Ui.Views.Count = Options.LabViewModeCount;
                    for (int Slot = 0; Slot < Ui.Views.Count; ++Slot)
                    {
                        SetViewMode(Ui.Views, Slot, Options.LabViewModes[Slot]);
                        const auto &Id = Options.LabViewSources[Slot];
                        if (Id.empty())
                            continue;
                        const auto &States = Run.Active.States;
                        const auto  State = std::ranges::find(States, Id, &Rendering::FStateVisualization::Id);
                        const auto  View = static_cast<ESurfaceSimulationView>(Ui.Views.Modes[Slot]);
                        if (State == States.end() || !SupportsStateSource(View, *State))
                            throw std::invalid_argument("State source is unavailable for this view: " + Id);
                        Ui.Views.StateIndices[Slot] = static_cast<std::uint32_t>(State - States.begin());
                    }
                }
                if (Options.LabWaterEdges >= 0)
                    Ui.Views.SmoothWater.fill(Options.LabWaterEdges != 0);
                if (Options.LabWaterSlosh >= 0)
                {
                    Ui.Views.SloshOptics.fill((Options.LabWaterSlosh & 1) != 0);
                    Ui.Views.SloshHeight.fill((Options.LabWaterSlosh & 2) != 0);
                }
                Ui.Views.bTextures = Options.bLabTextures;
                Ui.Views.bStudioStrip = Options.bLabStudioStrip;
                Ui.Views.bEnvironmentReflections = Options.bLabEnvironmentReflections;
                Ui.Views.MaterialNormalMaps.fill(Options.bLabMaterialNormalMaps);
                std::size_t Frames = 0;
                while (!Window.ShouldClose() && Frames < Options.FrameLimit)
                {
                    Window.PollEvents();
                    std::array<Liquid::FSurfaceStepInput, MaxSubsteps> Input{};
                    const auto Count = Run.PrepareInputs(Ui, static_cast<double>(StepSeconds) * 4, Input);
                    const auto ActiveInputs = std::span(Input).first(Count);
                    const auto Capture = Frames + 1 == Options.FrameLimit ? Options.LabCapture : std::filesystem::path{};
                    if (!Capture.empty() && !Run.Display->CanCapture())
                        throw std::runtime_error("Swapchain transfer source unavailable.");

                    if (Run.Display->DrawFrame(*Run.Simulation, ActiveInputs, Run.FrameViews(Ui), Capture))
                    {
                        Run.CommitFrame(ActiveInputs);
                        if (Count > 0)
                            Ui.bSingleStep = false;
                        ++Frames;
                    }
                }
                std::cout << "Surface simulation submitted " << Frames << " frames, " << Run.Steps << " fixed steps.\n";
                const auto  Timings = Run.Display->Stats();
                const auto &Solver = Timings.Solver;
                if (Solver.Samples > 0)
                    std::cout << "GPU solver timing: mean " << Solver.MeanMilliseconds << " ms, p90 "
                              << Solver.P90Milliseconds << " ms, samples " << Solver.Samples << "\n";
                const auto &Timing = Timings.Draw;
                if (Timing.Samples > 0)
                    std::cout << "GPU display timing: mean " << Timing.MeanMilliseconds << " ms, p90 "
                              << Timing.P90Milliseconds << " ms, samples " << Timing.Samples << "\n";
            }

            void RunChecks(const FApplicationLaunchOptions &Options, const GPU::FVulkanContext *Context)
            {
                CheckReferenceContracts(Context, Options.LabOutput, Options.bLabProfileBasins);
                if (!Options.LabGeometryFixtures.empty())
                    CheckGeometryImports(Options.LabGeometryFixtures, Options.LabOutput);
                const auto RunAsset = [&](const FSurfaceSceneSettings &Settings)
                {
                    const auto Output = Options.LabOutput.empty()
                                            ? std::filesystem::path{}
                                            : Options.LabOutput / (Settings.AssetPath.stem().string() + ".json");
                    // 원본 장면의 연출 기준은 그 입력에 적용하고 모든 에셋의 질량 수지와 GPU 일치를 검사한다.
                    const bool bBaseline = Settings.AssetPath.filename() == BundledScenePath(Settings.Scene).filename();
                    (void)RunExperiment(Settings, Context, Output, bBaseline);
                };
                if (!Options.ScenePath.empty())
                {
                    if (const auto Comparison = ReadSceneComparison(Options.ScenePath))
                    {
                        for (const auto &Case : LoadComparisonCases(*Comparison))
                            RunAsset(Case);
                    }
                    else
                        RunAsset(LoadSceneAsset(Options.ScenePath));
                    return;
                }
                for (int Index = 0; Index < static_cast<int>(SceneNames.size()); ++Index)
                {
                    if (Options.LabScene >= 0 && Options.LabScene != Index)
                        continue;
                    auto Settings = Defaults(static_cast<EScene>(Index));
                    auto RunCase = [&](std::string Name)
                    {
                        const auto Output = Options.LabOutput.empty() ? std::filesystem::path{} : Options.LabOutput / Name;
                        (void)RunExperiment(Settings, Context, Output, true);
                    };
                    if (Settings.Scene == EScene::Cavity)
                    {
                        const auto DemoGrid = Settings;
                        Settings.GridColumns = 2;
                        Settings.GridRows = 1;
                        RunCase("cavity-height.json");
                        Settings = DemoGrid;
                        RunCase("cavity-demo-height.json");
                    }
                    else
                        RunCase(std::string(SceneKeys[Index]) + ".json");
                    if (Options.LabScene < 0)
                        for (const auto &Path : BundledSceneVariants(Settings.Scene))
                        {
                            const auto Variant = LoadSceneAsset(Path);
                            if (Variant.Scene != Settings.Scene)
                                throw std::runtime_error(Path.string() + ": Variant geometry must match its scene family.");
                            RunAsset(Variant);
                        }
                }
                if (Options.LabScene >= 0 || Options.LabGeometryFixtures.empty())
                    return;

                auto RunVariant = [&](const FSurfaceSceneSettings &Settings, const char *Name)
                {
                    const auto Output = Options.LabOutput.empty() ? std::filesystem::path{} : Options.LabOutput / Name;
                    std::cout << "variant " << Name << '\n';
                    (void)RunExperiment(Settings, Context, Output, false);
                };
                auto Settings = Defaults(EScene::Terrace);
                Settings.ShapeColumns = 64;
                Settings.ShapeRows = 48;
                RunVariant(Settings, "terrace-geometry-64x48.json");
                Settings = Defaults(EScene::Terrace);
                Settings.GridColumns = 16;
                Settings.GridRows = 10;
                RunVariant(Settings, "terrace-solver-16x10.json");
                Settings.Source = SurfaceState::EHeightfieldSource::Mesh;
                Settings.Path = Options.LabGeometryFixtures / "patch.obj";
                Settings.ShapeColumns = 52;
                Settings.ShapeRows = 36;
                Settings.GridColumns = 10;
                Settings.GridRows = 7;
                Settings.Scale = 1.F;
                Settings.FineFilter = 2;
                Settings.BaseFilter = 95;
                RunVariant(Settings, "mesh-base-and-meso.json");
                Settings.bBaseOnly = true;
                RunVariant(Settings, "mesh-base-only.json");
            }
        } // namespace

    #pragma endregion

    // ============================================================
    // 01. Surface Simulation Entry Point
    // ============================================================
    #pragma region 01_Surface_Lab_Entry_Point
        // CPU 기준 검사에는 창을 만들지 않고 Vulkan 검사나 표시가 필요할 때만 초기화한다.

        void Run(const FApplicationLaunchOptions &Options)
        {
            if (Options.Lab == ESurfaceSimulationLaunch::Reference)
            {
                RunChecks(Options, nullptr);
                return;
            }
            FWindow                   Window(1200, 780, "MDSS Engine Capture", false);
            const GPU::FVulkanContext Context(Window);
            if (Options.Lab == ESurfaceSimulationLaunch::GpuCheck)
                RunChecks(Options, &Context);
            else if (const auto Comparison = ReadSceneComparison(Options.ScenePath))
            {
                const auto Cases = LoadComparisonCases(*Comparison);
                const auto Fit = ComparisonFitSize(Cases);
                for (int Side = 0; Side < static_cast<int>(Cases.size()); ++Side)
                {
                    auto CaseOptions = Options;
                    if (!Options.LabCapture.empty())
                        CaseOptions.LabCapture = ComparisonCapturePath(Options.LabCapture, Side);
                    RunCapture(Context, Window, CaseOptions, Cases[Side], Fit);
                }
            }
            else
                RunCapture(Context,
                           Window,
                           Options,
                           Options.ScenePath.empty() ? Defaults(static_cast<EScene>(std::max(Options.LabScene, 0)))
                                                     : LoadSceneAsset(Options.ScenePath));
        }

    #pragma endregion

    // ============================================================
    // 02. Embedded Editor Session
    // ============================================================
    #pragma region 02_Embedded_Editor_Session
        // 에디터의 Vulkan 장치와 프레임을 빌려 Surface Simulation의 뷰와 실험 입력을 관리한다.

        class FEditorSession::FImpl
        {
            public:
            explicit FImpl(const GPU::FVulkanContext &Context)
                : Run(Context, nullptr, Defaults(EScene::Compound))
            {
                Ui.SetDraft(Run.Active);
                SetSceneView(Ui.Views, Run.Active);
                SetLayerAndSubstrateViews(Ui.Views);
                Ui.Views.Palette = AutomaticHeatmapPalette;
            }

            ~FImpl()
            {
                ReleasePlot();
            }

            struct FComparisonSession
            {
                static constexpr int ViewsPerCase = 3;

                FSceneComparison Asset;
                // 기본 Run은 첫 조건이다. 각 조건의 외관과 진단은 같은 실행 결과를 읽는다.
                std::vector<std::unique_ptr<FRun>>   AdditionalRuns;
                std::vector<FSurfaceSimulationViews> ViewSettings;
                std::vector<std::array<bool, 4>>     ViewLabels;
                std::vector<FGridResolution>         Original;
                std::vector<FGridResolution>         Draft;
                glm::vec2                            FitSize{};
            };

            void Open(const std::filesystem::path &Path)
            {
                if (auto Asset = ReadSceneComparison(Path))
                {
                    auto Cases = LoadComparisonCases(*Asset);
                    auto Next = std::make_unique<FComparisonSession>();
                    Next->Asset = std::move(*Asset);
                    Next->FitSize = ComparisonFitSize(Cases);
                    for (const auto &Case : Cases)
                        Next->Original.push_back(FGridResolution::From(Case));
                    Next->Draft = Next->Original;

                    // 다른 조건의 준비가 끝날 때까지 현재 장면을 유지한다.
                    for (std::size_t Index = 1; Index < Cases.size(); ++Index)
                        Next->AdditionalRuns.push_back(
                            std::make_unique<FRun>(Run.Context, nullptr, std::move(Cases[Index])));
                    Open(std::move(Cases.front()));
                    auto Case = FSurfaceSimulationViews{};
                    using enum ESurfaceSimulationView;
                    Case.Count = FComparisonSession::ViewsPerCase;
                    Case.bSplitLowerRow = true;
                    SetViewMode(Case, 0, static_cast<int>(Pbr));
                    SetViewMode(Case, 1, static_cast<int>(LayerStates));
                    SetViewMode(Case, 2, static_cast<int>(SubstrateStates));
                    Next->ViewSettings.resize(Cases.size(), Case);
                    Next->ViewLabels.resize(Cases.size(), {true, true, true, true});
                    Compare = std::move(Next);
                    Ui.bPlaying = !Run.Active.bStartPaused;
                    return;
                }
                Open(LoadSceneAsset(Path));
            }

            void ApplyDefaultSubstrateView()
            {
                const auto &Settings = Run.Active;
                if (Settings.DefaultSubstrateViewState.empty())
                    return;

                const auto State = std::ranges::find(Settings.States,
                                                     Settings.DefaultSubstrateViewState,
                                                     &Rendering::FStateVisualization::Id);
                if (State == Settings.States.end())
                    return;

                SubstrateModeBeforeDefault = Ui.Views.Modes[1];
                Ui.Views.Modes[1] = static_cast<int>(ESurfaceSimulationView::StateHeatmap);
                Ui.Views.StateIndices[1] = static_cast<std::uint32_t>(State - Settings.States.begin());
            }

            void Open(FSurfaceSceneSettings Settings)
            {
                constexpr int HeatmapMode = static_cast<int>(ESurfaceSimulationView::StateHeatmap);
                const auto   &OldStates = Run.Active.States;
                const auto    OldSlot = Ui.Views.StateIndices[1];
                // 씬 기본값으로 바꾼 뷰를 되돌리되, 그 뒤 사용자가 고른 모드는 유지한다.
                if (SubstrateModeBeforeDefault && Ui.Views.Modes[1] == HeatmapMode && OldSlot < OldStates.size() &&
                    OldStates[OldSlot].Id == Run.Active.DefaultSubstrateViewState)
                    Ui.Views.Modes[1] = *SubstrateModeBeforeDefault;
                SubstrateModeBeforeDefault.reset();
                const bool bApplyPlaybackRate = Settings.InitialPlaybackRate || Run.Active.InitialPlaybackRate;
                Run.Display->FinishEditorCapture();
                Run.Reset(Settings);
                if (bApplyPlaybackRate)
                    Ui.PlaybackRate = Settings.InitialPlaybackRate.value_or(1.F);
                if (Compare)
                {
                    Compare.reset();
                    SetLayerAndSubstrateViews(Ui.Views);
                }
                Ui.SetDraft(Settings, true);
                if (Settings.bStartPaused)
                    Ui.bPlaying = false;
                SelectedCase = 0;
                SelectedViewport = 0;
                Ui.bSingleStep = false;
                Ui.SelectedAsset.clear();
                ResetSceneCamera(Ui.Views, Ui.Draft);
                Ui.Views.StateIndices.fill(0);
                ApplyDefaultSubstrateView();
                if (!HasTemperatureState(Run.Active.States))
                    for (auto &Mode : Ui.Views.Modes)
                        if (Mode == static_cast<int>(ESurfaceSimulationView::Temperature))
                            Mode = static_cast<int>(ESurfaceSimulationView::LayerThickness);
                Ui.bAnimateCamera = false;
                ClearExperiment();
                Ui.Message.clear();
                Ui.ProfileError.clear();
                Ui.InputMessage.clear();
                Ui.InjectionTarget = EInjectionTarget::PrimaryLiquid;
                Ui.bScheduledInlet = true;
                Ui.bScheduledHeat = true;
                Ui.bAutoTilt = true;
                Ui.ManualTiltDegrees = 0.F;
                PendingAction = ESurfaceSimulationAction::None;
                Previous = std::chrono::steady_clock::now();
            }

            [[nodiscard]] int RunCount() const noexcept
            {
                return Compare ? static_cast<int>(Compare->Asset.Scenes.size()) : 1;
            }

            [[nodiscard]] const FRun &RunAt(int View) const
            {
                return Compare && View > 0 ? *Compare->AdditionalRuns[View - 1] : Run;
            }

            [[nodiscard]] FRun &RunAt(int View)
            {
                return Compare && View > 0 ? *Compare->AdditionalRuns[View - 1] : Run;
            }

            [[nodiscard]] std::vector<FGridResolution> ActiveComparisonGrids() const
            {
                std::vector<FGridResolution> Grids;
                Grids.reserve(RunCount());
                for (int View = 0; View < RunCount(); ++View)
                    Grids.push_back(FGridResolution::From(RunAt(View).Active));
                return Grids;
            }

            void ResetComparison(ESurfaceSimulationAction Action)
            {
                // 모든 새 리소스가 준비된 뒤에만 기존 실행을 교체한다.
                std::vector<std::unique_ptr<FRun>> Replacements;
                Replacements.reserve(RunCount());
                for (int View = 0; View < RunCount(); ++View)
                {
                    const auto &Current = RunAt(View);
                    auto        Settings = Current.Active;
                    if (Action == ESurfaceSimulationAction::Apply)
                        Compare->Draft[View].ApplyTo(Settings);
                    Replacements.push_back(std::make_unique<FRun>(Run.Context, Current.Window, std::move(Settings)));
                }
                for (int View = 0; View < RunCount(); ++View)
                    RunAt(View).AdoptReset(std::move(*Replacements[View]));
                if (Action == ESurfaceSimulationAction::Restart)
                    Ui.bPlaying = !Run.Active.bStartPaused;
                Ui.bSingleStep = false;
                Ui.InputMessage.clear();
                Ui.ProfileError.clear();
                Ui.Message = "Comparison restarted.";
            }

            void Apply(ESurfaceSimulationAction Action)
            {
                try
                {
                    if (Action == ESurfaceSimulationAction::CpuCheck || Action == ESurfaceSimulationAction::GpuCheck ||
                        Action == ESurfaceSimulationAction::Snapshot ||
                        Action == ESurfaceSimulationAction::SnapshotWithData)
                    {
                        StartExperiment(Action);
                        return;
                    }
                    if (Compare &&
                        (Action == ESurfaceSimulationAction::Apply || Action == ESurfaceSimulationAction::Restart))
                        ResetComparison(Action);
                    else
                        Run.Action(Ui, Action, Capture);
                    if (Compare && Action == ESurfaceSimulationAction::Capture)
                    {
                        const auto Stem = Compare->Asset.Path.stem().string();
                        Capture = OutputName(Stem.c_str(), ".png");
                        Ui.Message = CaptureCase >= 0 ? "Saving " + ComparisonCapturePath(Capture, CaptureCase).string()
                                                      : "Saving " + Capture.stem().string() + " (" +
                                                            std::to_string(RunCount()) + " conditions)";
                    }
                    if (Action == ESurfaceSimulationAction::Apply || Action == ESurfaceSimulationAction::Restart)
                        ClearExperiment();
                    if (Action != ESurfaceSimulationAction::None && Action != ESurfaceSimulationAction::Capture)
                        Previous = std::chrono::steady_clock::now();
                }
                catch (const std::exception &Error)
                {
                    Ui.Message = Error.what();
                    if (Action == ESurfaceSimulationAction::Apply || Action == ESurfaceSimulationAction::Restart)
                        Ui.ProfileError = Ui.Message;
                    FLogger::Error("TDebugUI", Ui.Message);
                    Previous = std::chrono::steady_clock::now();
                }
            }

            [[nodiscard]] int ViewCount() const noexcept
            {
                return Compare ? RunCount() * FComparisonSession::ViewsPerCase : Ui.Views.Count;
            }

            [[nodiscard]] FSurfaceSimulationViews &SelectedViewSettings()
            {
                return Compare ? Compare->ViewSettings[SelectedCase] : Ui.Views;
            }

            [[nodiscard]] std::array<bool, 4> &ViewLabelsAt(int Case)
            {
                return Compare ? Compare->ViewLabels[Case] : bViewLabels;
            }

            // 카메라와 공통 표시 옵션은 공유하고, 화면별 옵션은 해당 조건의 세 뷰에서 읽는다.
            [[nodiscard]] FSurfaceSimulationViews CaseViews(int Case) const
            {
                auto Result = Ui.Views;
                if (!Compare)
                    return Result;
                const auto &Settings = Compare->ViewSettings[Case];
                Result.Count = Settings.Count;
                Result.bSplitLowerRow = Settings.bSplitLowerRow;
                Result.CameraFitSize = Compare->FitSize;
                Result.Modes = Settings.Modes;
                Result.StateIndices = Settings.StateIndices;
                Result.SmoothWater = Settings.SmoothWater;
                Result.SloshOptics = Settings.SloshOptics;
                Result.SloshHeight = Settings.SloshHeight;
                Result.MaterialNormalMaps = Settings.MaterialNormalMaps;
                Result.CellOutlines = Settings.CellOutlines;
                Result.PaletteOverrides = Settings.PaletteOverrides;
                Result.TextureOverrides = Settings.TextureOverrides;
                Result.EffectOverrides = Settings.EffectOverrides;
                return Result;
            }

            void DrawComparisonResolution()
            {
                ImGui::SeparatorText("Shared resolution");
                ImGui::SetItemTooltip("Editing the grid applies the same resolution to all conditions.");
                auto Resolution = Compare->Draft.front();
                int  ColumnMultiple = 1;
                for (int View = 0; View < RunCount(); ++View)
                    ColumnMultiple = std::lcm(ColumnMultiple, RunAt(View).Active.SpecimenCount());
                ImGui::PushItemWidth(-125.F);
                const bool bEdited = DrawGridResolution(Resolution, ColumnMultiple);
                ImGui::PopItemWidth();
                const bool bDifferent =
                    std::ranges::any_of(Compare->Draft, [&](const auto &Grid) { return Grid != Resolution; });
                const bool bMatch = bDifferent && ImGui::Button("Match All Grids");
                if (bEdited || bMatch)
                    std::ranges::fill(Compare->Draft, Resolution);

                if (ImGui::Button("Use Original Resolutions"))
                    Compare->Draft = Compare->Original;
            }

            void DrawComparisonApply()
            {
                const auto Active = ActiveComparisonGrids();
                const bool bEdited = Compare->Draft != Active;
                if (!Ui.ProfileError.empty())
                {
                    ImGui::TextColored({1.F, 0.54F, 0.4F, 1.F}, "Settings error");
                    ImGui::SetItemTooltip("%s", Ui.ProfileError.c_str());
                }
                else
                    ImGui::TextDisabled("%s", bEdited ? "Pending resolution change" : "No pending changes");

                ImGui::BeginDisabled(!bEdited);
                if (ImGui::Button("Apply & Restart"))
                    PendingAction = ESurfaceSimulationAction::Apply;
                ImGui::SameLine();
                if (ImGui::Button("Discard Changes"))
                {
                    Compare->Draft = Active;
                    Ui.ProfileError.clear();
                }
                ImGui::EndDisabled();
            }

            void DrawCaseInfo(int View) const
            {
                const auto &Case = RunAt(View);
                ImGui::SeparatorText(Compare->Asset.Labels[View].c_str());
                ImGui::TextWrapped("%s", Case.Active.AssetPath.filename().string().c_str());
                ImGui::TextDisabled("Simulation  %d x %d", Case.Active.GridColumns, Case.Active.GridRows);
                ImGui::TextDisabled("Geometry  %d x %d", Case.Scene.Geometry.Shape.Columns, Case.Scene.Geometry.Shape.Rows);
                ImGui::TextDisabled("Automatic basin splitting  %s", Case.Active.bSplitSubgridBasins ? "On" : "Off");
                ImGui::TextDisabled("Storage regions  %zu", Case.Scene.Geometry.Regions.size());
                ImGui::TextDisabled("Outlet  %s", Case.HasOpenOutlet() ? "Open +X" : "Closed");
            }

            struct FSnapshot
            {
                FSurfaceSceneRuntime            Scene;
                Liquid::FSurfaceSimulationState State;
                Liquid::FSurfaceTilt            Pose;
            };

            struct FRenderedExperiment
            {
                std::filesystem::path Image;
                Asset::FTextureData   Pixels;
                std::string           Title;
                std::string           Summary;
            };

            void StartExperiment(ESurfaceSimulationAction Action)
            {
                if (ExperimentJob.valid())
                    return;

                const bool bSnapshot =
                    Action == ESurfaceSimulationAction::Snapshot || Action == ESurfaceSimulationAction::SnapshotWithData;
                const bool  bCompare = Action == ESurfaceSimulationAction::GpuCheck;
                const bool  bKeepData = Action == ESurfaceSimulationAction::SnapshotWithData;
                const auto  Output = OutputName(SceneKeys[static_cast<int>(Run.Active.Scene)],
                                                bSnapshot  ? "-state.json"
                                                : bCompare ? "-gpu.json"
                                                           : "-cpu.json");
                const char *Title = bSnapshot ? "State Snapshot" : bCompare ? "CPU / GPU Comparison" : "CPU Reference";
                std::optional<FSnapshot> Snapshot;
                try
                {
                    if (bSnapshot)
                    {
                        if (vkDeviceWaitIdle(Run.Context.GetDevice()) != VK_SUCCESS)
                            throw std::runtime_error("Failed to read Surface Simulation state.");
                        Snapshot.emplace(Run.Scene, Run.Simulation->Readback().State, Run.Pose);
                        Previous = std::chrono::steady_clock::now();
                    }
                    if (bCompare)
                    {
                        // The comparison owns a separate queue and command pool, so it can run during rendering.
                        ExperimentWindow = std::make_unique<FWindow>(64, 64, "MDSS Experiment", false);
                        ExperimentContext = std::make_unique<GPU::FVulkanContext>(*ExperimentWindow);
                    }
                    Ui.ExperimentStatus = bSnapshot ? "Capturing state plot..." : "Running experiment...";
                    ExperimentJob = std::async(
                        std::launch::async,
                        [Settings = Run.Active,
                         Snapshot = std::move(Snapshot),
                         Context = ExperimentContext.get(),
                         Output,
                         bKeepData,
                         Title]
                        {
                            auto Result =
                                Snapshot ? ExportState(Settings, Snapshot->Scene, Snapshot->State, Snapshot->Pose, Output)
                                         : RunExperiment(Settings, Context, Output, false);
                            GenerateExperimentPlot(Output, bKeepData);
                            auto Image = Output;
                            Image.replace_extension(".png");
                            auto Pixels = Asset::FTextureLoader::LoadRGBA8(Image);
                            if (!bKeepData)
                            {
                                std::error_code RemovalError;
                                std::filesystem::remove(Output, RemovalError);
                                if (RemovalError)
                                    FLogger::Warning("TDebugUI",
                                                     "Could not remove intermediate experiment data: " + Output.string());
                            }
                            return FRenderedExperiment{std::move(Image),
                                                       std::move(Pixels),
                                                       Title,
                                                       std::move(Result.Report)};
                        });
                    bDiscardExperiment = false;
                    Ui.bExperimentRunning = true;
                }
                catch (const std::exception &Error)
                {
                    ExperimentContext.reset();
                    ExperimentWindow.reset();
                    Ui.ExperimentStatus = Error.what();
                    FLogger::Error("TDebugUI", Error.what());
                }
            }

            void PollExperiment()
            {
                if (!ExperimentJob.valid() || ExperimentJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    return;
                try
                {
                    auto Result = ExperimentJob.get();
                    if (!bDiscardExperiment)
                    {
                        PreparePlot(Result);
                        Ui.ExperimentStatus.clear();
                    }
                    FLogger::Info("TDebugUI", "Saved " + Result.Image.string());
                }
                catch (const std::exception &Error)
                {
                    if (!bDiscardExperiment)
                        Ui.ExperimentStatus = Error.what();
                    FLogger::Error("TDebugUI", Error.what());
                }
                ExperimentContext.reset();
                ExperimentWindow.reset();
                Ui.bExperimentRunning = false;
                if (bDiscardExperiment)
                    Ui.ExperimentStatus.clear();
            }

            void ClearExperiment()
            {
                bDiscardExperiment = true;
                ReleasePlot();
                Ui.ExperimentPlot.clear();
                Ui.ExperimentTitle.clear();
                Ui.ExperimentSummary.clear();
                Ui.ExperimentStatus = Ui.bExperimentRunning ? "Finishing the previous experiment..." : "";
                Ui.bExpandPlot = false;
            }

            void PreparePlot(FRenderedExperiment &Result)
            {
                const auto &Pixels = Result.Pixels;
                auto        Replacement = std::make_unique<Asset::FTextureAsset>(Asset::InvalidAssetHandle,
                                                                                 "Experiment plot",
                                                                                 Result.Image,
                                                                                 Run.Context,
                                                                                 Pixels.Width,
                                                                                 Pixels.Height,
                                                                                 Pixels.Pixels,
                                                                                 VK_FORMAT_R8G8B8A8_SRGB);
                const auto  Texture =
                    ImGui_ImplVulkan_AddTexture(Replacement->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                if (!Texture)
                    throw std::runtime_error("PNG saved, but its preview could not be loaded: " + Result.Image.string());
                ReleasePlot();
                Plot = std::move(Replacement);
                Ui.PlotAspect = static_cast<float>(Pixels.Width) / static_cast<float>(Pixels.Height);
                Ui.PlotTexture = Texture;
                Ui.ExperimentPlot = Result.Image;
                Ui.ExperimentTitle = std::move(Result.Title);
                Ui.ExperimentSummary = std::move(Result.Summary);
            }

            void ReleasePlot()
            {
                if (!Ui.PlotTexture)
                    return;
                vkDeviceWaitIdle(Run.Context.GetDevice());
                ImGui_ImplVulkan_RemoveTexture(Ui.PlotTexture);
                Ui.PlotTexture = VK_NULL_HANDLE;
                Plot.reset();
            }

            void ProcessCameraInput(bool bHovered)
            {
                auto       &Views = Ui.Views;
                const auto &IO = ImGui::GetIO();
                bOrbitDragging = ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
                                 (bOrbitDragging || (bHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)));
                if (bOrbitDragging)
                {
                    Ui.bAnimateCamera = false;
                    Views.CameraAzimuth = std::remainder(Views.CameraAzimuth - IO.MouseDelta.x * 0.45F, 360.F);
                    Views.CameraElevation = std::clamp(Views.CameraElevation + IO.MouseDelta.y * 0.45F, -89.F, 89.F);
                }

                float Zoom = bHovered ? IO.MouseWheel : 0.F;
                if ((bHovered || bOrbitDragging) && !IO.WantTextInput && !ImGui::IsAnyItemActive())
                {
                    const auto Axis = [](ImGuiKey Positive, ImGuiKey Negative)
                    {
                        return static_cast<float>(ImGui::IsKeyDown(Positive)) -
                               static_cast<float>(ImGui::IsKeyDown(Negative));
                    };
                    Zoom += Axis(ImGuiKey_Equal, ImGuiKey_Minus) * 10.F * IO.DeltaTime;
                    const float     Azimuth = glm::radians(Views.CameraAzimuth);
                    const glm::vec2 Forward{-std::cos(Azimuth), -std::sin(Azimuth)};
                    const glm::vec2 Right{Forward.y, -Forward.x};
                    const auto Direction = Forward * Axis(ImGuiKey_W, ImGuiKey_S) + Right * Axis(ImGuiKey_D, ImGuiKey_A);
                    if (glm::dot(Direction, Direction) > 0.F)
                    {
                        const auto &Shape = Run.Scene.Geometry.Shape;
                        const float Speed =
                            std::max(Shape.Width, Shape.Length) * Views.CameraDistanceScale * (IO.KeyShift ? 2.4F : 0.8F);
                        Views.CameraOffset += glm::normalize(Direction) * (Speed * IO.DeltaTime);
                        Ui.bAnimateCamera = false;
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Home))
                    {
                        ResetSceneCamera(Views, Run.Active);
                        Ui.bAnimateCamera = false;
                    }
                    constexpr std::array Keys{ImGuiKey_Keypad1,
                                              ImGuiKey_Keypad2,
                                              ImGuiKey_Keypad3,
                                              ImGuiKey_Keypad4,
                                              ImGuiKey_Keypad5,
                                              ImGuiKey_Keypad6,
                                              ImGuiKey_Keypad7,
                                              ImGuiKey_Keypad8,
                                              ImGuiKey_Keypad9};
                    for (int Index = 0; Index < static_cast<int>(Keys.size()); ++Index)
                        if (ImGui::IsKeyPressed(Keys[Index], false))
                        {
                            Views.CameraOffset = {};
                            Views.CameraAzimuth = std::remainder(90.F * static_cast<float>(Index % 3 + 1), 360.F);
                            Views.CameraElevation = 89.F * static_cast<float>(1 - Index / 3);
                            Ui.bAnimateCamera = false;
                        }
                }
                if (Zoom != 0.F)
                {
                    Views.CameraDistanceScale = std::clamp(Views.CameraDistanceScale * std::pow(0.88F, Zoom), 0.25F, 4.F);
                    Ui.bAnimateCamera = false;
                }
            }

            // 표시한 영역을 그대로 주입 큐로 넘겨, 비교 뷰에서도 대상이 한 번만 결정되게 한다.
            void ProcessInjectionPreview(ImDrawList &Draw, ImVec2 Origin, ImVec2 Size, ImVec2 Tile)
            {
                if (!Ui.bInjectionEnabled || !Run.Active.bExternalExchange || Run.Active.Scene == EScene::FreezeThaw ||
                    bOrbitDragging || ImGui::GetIO().WantTextInput || ImGui::IsAnyItemActive())
                    return;

                const auto Mouse = ImGui::GetMousePos();
                auto       Footprint =
                    Run.Display->PickInjectionFootprint({(Mouse.x - Origin.x) / Size.x, (Mouse.y - Origin.y) / Size.y},
                                                        Ui.Brush);
                ImGui::SetMouseCursor(ImGuiMouseCursor_None);
                constexpr ImU32 Accent = IM_COL32(43, 247, 216, 255);
                constexpr ImU32 Shadow = IM_COL32(12, 17, 24, 255);
                if (Footprint)
                {
                    const ImVec2 Min{Origin.x + static_cast<float>(Footprint->View % 2) * Tile.x,
                                     Origin.y + static_cast<float>(Footprint->View / 2) * Tile.y};
                    Draw.PushClipRect(Min, {Min.x + Tile.x, Min.y + Tile.y}, true);
                    for (const auto &Sample : Footprint->Samples)
                    {
                        std::array<ImVec2, 4> Points;
                        for (std::size_t I = 0; I < Points.size(); ++I)
                            Points[I] = {Origin.x + Sample.Corners[I].x * Size.x, Origin.y + Sample.Corners[I].y * Size.y};
                        // 두 삼각형은 표면 메시와 같은 대각선을 사용해 굽은 형상에도 맞춘다.
                        const ImU32 Fill = IM_COL32(43, 247, 216, static_cast<int>(20.F + 45.F * Sample.Coverage));
                        Draw.AddTriangleFilled(Points[0], Points[2], Points[1], Fill);
                        Draw.AddTriangleFilled(Points[0], Points[3], Points[2], Fill);
                        for (std::size_t I = 0; I < Points.size(); ++I)
                            if ((Sample.BoundaryEdges & (1U << I)) != 0)
                                Draw.AddLine(Points[I], Points[(I + 1) % Points.size()], Accent, 1.8F);
                    }
                    Draw.PopClipRect();
                }

                const ImU32  Cursor = Footprint ? Accent : IM_COL32(188, 198, 210, 255);
                const ImVec2 Left{Mouse.x - 6.F, Mouse.y}, Right{Mouse.x + 6.F, Mouse.y};
                const ImVec2 Top{Mouse.x, Mouse.y - 6.F}, Bottom{Mouse.x, Mouse.y + 6.F};
                Draw.AddLine(Left, Right, Shadow, 3.F);
                Draw.AddLine(Top, Bottom, Shadow, 3.F);
                Draw.AddLine(Left, Right, Cursor, 1.4F);
                Draw.AddLine(Top, Bottom, Cursor, 1.4F);

                if (!ImGui::IsKeyPressed(ImGuiKey_Space, false))
                    return;
                if (Footprint)
                    Run.QueueInjection(Ui, std::move(Footprint->Regions));
                else
                    Ui.InputMessage = "Aim at the top of the surface and press Space.";
            }

            FRun Run;
            struct FDisplayedWorkload
            {
                float       Time = 0.F;
                double      Backlog = 0.0;
                int         GridColumns = 0;
                int         GridRows = 0;
                int         SampleColumns = 0;
                int         SampleRows = 0;
                std::size_t Regions = 0;
                std::size_t Faces = 0;
                std::size_t Steps = 0;
                int         Views = 0;
                bool        bPlaying = false;
            } DisplayedWorkload;
            FSurfaceSimulationDisplayStats        DisplayedTimings;
            std::size_t                           LastFrameStepCount = 0;
            std::unique_ptr<FComparisonSession>   Compare;
            std::optional<int>                    SubstrateModeBeforeDefault;
            FSurfaceSimulationUI                  Ui;
            ESurfaceSimulationAction              PendingAction = ESurfaceSimulationAction::None;
            int                                   SelectedCase = 0;
            int                                   SelectedViewport = 0;
            int                                   CaptureCase = -1;
            int                                   CaptureView = -1;
            std::array<bool, 4>                   bViewLabels{true, true, true, true};
            bool                                  bRecord = false;
            bool                                  bPresentation = false;
            bool                                  bOrbitDragging = false;
            std::filesystem::path                 Capture;
            std::chrono::steady_clock::time_point Previous = std::chrono::steady_clock::now();
            std::unique_ptr<Asset::FTextureAsset> Plot;
            bool                                  bDiscardExperiment = false;
            std::unique_ptr<FWindow>              ExperimentWindow;
            std::unique_ptr<GPU::FVulkanContext>  ExperimentContext;
            std::future<FRenderedExperiment>      ExperimentJob;
        };

        FEditorSession::FEditorSession(const GPU::FVulkanContext &Context)
            : Impl(std::make_unique<FImpl>(Context))
        {
        }

        FEditorSession::~FEditorSession() = default;

        void FEditorSession::OpenScene(EScene Scene)
        {
            Impl->Open(Defaults(Scene));
        }

        void FEditorSession::OpenScene(const std::filesystem::path &Path)
        {
            Impl->Open(Path);
        }

        EScene FEditorSession::GetScene() const noexcept
        {
            return Impl->Run.Active.Scene;
        }
        const std::filesystem::path &FEditorSession::GetScenePath() const noexcept
        {
            return Impl->Compare ? Impl->Compare->Asset.Path : Impl->Run.Active.AssetPath;
        }
        std::string_view FEditorSession::GetComparisonTitle() const noexcept
        {
            return Impl->Compare ? std::string_view(Impl->Compare->Asset.Title) : std::string_view{};
        }

        FSceneOverviewValues FEditorSession::GetSceneOverviewValues() const
        {
            FSceneOverviewValues Values;
            const auto           AddUnique = [](auto &List, std::string_view Value)
            {
                if (!Value.empty() && std::ranges::find(List, Value) == List.end())
                    List.push_back(Value);
            };
            const auto Include = [&](const FSurfaceSceneSettings &Active)
            {
                AddUnique(Values.Materials, Active.Profile ? Active.Profile->Substrate.Id : Active.ProceduralSubstrate.Id);
                for (const auto &Profile : Active.ComparisonProfiles)
                    AddUnique(Values.Materials, Profile.Substrate.Id);
                for (const auto &State : Active.States)
                    AddUnique(State.bSubstrate ? Values.SubstrateStates : Values.LayerStates, State.Name);
            };
            Include(Impl->Run.Active);
            if (Impl->Compare)
                for (const auto &Run : Impl->Compare->AdditionalRuns)
                    Include(Run->Active);
            return Values;
        }
        int FEditorSession::GetViewCount() const noexcept
        {
            return Impl->ViewCount();
        }

        void FEditorSession::SetViewCount(int Count)
        {
            if (Impl->Compare)
                return;
            Impl->Ui.Views.Count = std::clamp(Count, 1, 4);
            Impl->SelectedViewport = std::min(Impl->SelectedViewport, Impl->Ui.Views.Count - 1);
        }

        void FEditorSession::QueueCapture(EEditorCapture Capture)
        {
            if (Impl->Compare && Capture == EEditorCapture::StateAndPlot)
            {
                Impl->Ui.Message = "Open an original scene to capture its state and plot.";
                FLogger::Info("SurfaceSimulation", Impl->Ui.Message);
                return;
            }
            const bool bActiveView = Capture == EEditorCapture::ActiveView;
            Impl->CaptureCase = bActiveView && Impl->Compare ? Impl->SelectedCase : -1;
            Impl->CaptureView = bActiveView ? Impl->SelectedViewport : -1;
            Impl->PendingAction = Capture == EEditorCapture::StateAndPlot ? ESurfaceSimulationAction::SnapshotWithData
                                                                          : ESurfaceSimulationAction::Capture;
        }

        void FEditorSession::DrawQuickViewControls()
        {
            using enum ESurfaceSimulationView;
            auto      &Views = Impl->SelectedViewSettings();
            const auto Index = Impl->SelectedViewport;
            auto      &Mode = Views.Modes[Index];
            const auto Select = [&](ESurfaceSimulationView Value)
            {
                const int Item = static_cast<int>(Value);
                if (ImGui::Selectable(SurfaceSimulationViewNames[Item], Mode == Item))
                    SetViewMode(Views, Index, Item);
            };
            const auto &States = Impl->RunAt(Impl->SelectedCase).Active.States;
            auto       &StateIndex = Views.StateIndices[Index];
            const auto  CurrentMode = static_cast<ESurfaceSimulationView>(Mode);
            if (ImGui::BeginCombo("##ViewMode",
                                  ViewName(CurrentMode, States, ResolveStateSource(CurrentMode, States, StateIndex)),
                                  ImGuiComboFlags_HeightLarge))
            {
                ImGui::SeparatorText("Appearance");
                for (const auto View : {Pbr, Refraction, Fresnel, Flat})
                    Select(View);
                if (ImGui::BeginMenu("Diagnostics"))
                {
                    if (ImGui::BeginMenu("Geometry"))
                    {
                        for (const auto View : {Elevation, Bed})
                            Select(View);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Grids"))
                    {
                        for (const auto View : {SimulationGrid, GeometryGrid})
                            Select(View);
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("States"))
                    {
                        for (const auto View : {LayerStates, SubstrateStates})
                            Select(View);
                        ImGui::SeparatorText("Individual states");
                        if (States.empty())
                            ImGui::TextDisabled("No state channels");
                        for (std::size_t Slot = 0; Slot < States.size(); ++Slot)
                        {
                            ImGui::PushID(static_cast<int>(Slot));
                            if (ImGui::Selectable(States[Slot].Name.c_str(),
                                                  CurrentMode == StateHeatmap && Slot == StateIndex))
                            {
                                Mode = static_cast<int>(StateHeatmap);
                                StateIndex = static_cast<std::uint32_t>(Slot);
                            }
                            ImGui::PopID();
                        }
                        ImGui::EndMenu();
                    }
                    if (ImGui::BeginMenu("Fields & Measures"))
                    {
                        for (const auto View : {LayerThickness, ArealMass, Temperature, Activity})
                            if (View != Temperature || HasTemperatureState(States))
                                Select(View);
                        ImGui::EndMenu();
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("Show"))
                ImGui::OpenPopup("Viewport overlays");
            if (ImGui::BeginPopup("Viewport overlays"))
            {
                ImGui::SeparatorText("Viewport Guides");
                ImGui::Checkbox("World Grid", &Impl->Ui.Views.bWorldGrid);
                ImGui::Checkbox("World Axes", &Impl->Ui.Views.bWorldAxes);
                ImGui::Checkbox("View Label", &Impl->ViewLabelsAt(Impl->SelectedCase)[Index]);
                ImGui::SeparatorText("Simulation Overlays");
                ImGui::Checkbox("Cell Outlines", &Views.CellOutlines[Index]);
                ImGui::Checkbox("Outlet Edge", &Impl->Ui.bShowOutletEdge);
                ImGui::Checkbox("State Injection", &Impl->Ui.bShowWaterSource);
                ImGui::SetItemTooltip("Show the scene's scheduled injection area in the editor and presentation view.");
                ImGui::EndPopup();
            }
        }

        void FEditorSession::DrawCompareMenu()
        {
            using enum ESurfaceSimulationView;
            auto &Views = Impl->Ui.Views;
            if (Impl->Compare)
            {
                if (ImGui::MenuItem("Exit Compare"))
                {
                    Impl->Open(FSurfaceSceneSettings(Impl->Run.Active));
                    return;
                }
                const auto SetTopViews = [&](const char *Label, ESurfaceSimulationView Mode)
                {
                    if (ImGui::MenuItem(Label))
                        for (auto &Case : Impl->Compare->ViewSettings)
                            SetViewMode(Case, 0, static_cast<int>(Mode));
                };
                ImGui::SeparatorText("Top view for all conditions");
                SetTopViews("Appearance", Pbr);
                SetTopViews("Layer States", LayerStates);
                SetTopViews("Substrate States", SubstrateStates);
                SetTopViews("Elevation", Elevation);
                SetTopViews("Simulation Cells", SimulationGrid);
                SetTopViews("Geometry Samples", GeometryGrid);
                return;
            }
            if (ImGui::MenuItem("Exit Compare", nullptr, false, Views.Count > 1))
                SetViewCount(1);
            if (ImGui::MenuItem("Layer & Substrate"))
            {
                SetLayerAndSubstrateViews(Views);
                Impl->ApplyDefaultSubstrateView();
                Impl->bViewLabels.fill(true);
                Impl->SelectedViewport = 0;
            }
            if (ImGui::MenuItem("Elevation & Grids"))
            {
                SetViewCount(4);
                Views.Modes = {static_cast<int>(Pbr),
                               static_cast<int>(Elevation),
                               static_cast<int>(SimulationGrid),
                               static_cast<int>(GeometryGrid)};
                SetViewMode(Views, 0, static_cast<int>(Pbr));
                Impl->SelectedViewport = 0;
            }
        }

        void FEditorSession::DrawRenderOptions()
        {
            using enum ESurfaceSimulationView;
            auto      &Views = Impl->SelectedViewSettings();
            auto      &Shared = Impl->Ui.Views;
            const auto Index = Impl->SelectedViewport;
            const auto Case = Impl->SelectedCase;
            const auto Heading = Impl->Compare ? Impl->Compare->Asset.Labels[Case] + " · View " + std::to_string(Index + 1)
                                               : "Active view " + std::to_string(Index + 1);
            EditorLayout::SectionTitle(Heading.c_str());
            const auto Current = static_cast<ESurfaceSimulationView>(Views.Modes[Index]);
            const bool bAppearance = Current == Flat || Current == Fresnel || Current == Refraction || Current == Pbr;
            const bool bHeatmap = IsHeatmapView(Current);

            const auto &States = Impl->RunAt(Case).Active.States;
            auto       &StateIndex = Views.StateIndices[Index];
            const auto  Source = ResolveStateSource(Current, States, StateIndex);
            if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginDisabled(!bAppearance);
                bool bTextures = Views.TextureOverrides[Index] < 0 ? Shared.bTextures : Views.TextureOverrides[Index] != 0;
                bool bEffects = Views.EffectOverrides[Index] < 0 ? Shared.bEffects : Views.EffectOverrides[Index] != 0;
                if (ImGui::Checkbox("Material textures", &bTextures))
                    Views.TextureOverrides[Index] = bTextures;
                if (ImGui::Checkbox("Steam and flame", &bEffects))
                    Views.EffectOverrides[Index] = bEffects;
                ImGui::BeginDisabled(Current != Pbr || !bTextures ||
                                     Impl->RunAt(Case).Scene.Appearance.Surface.NormalTexturePath.empty());
                ImGui::Checkbox("Material normal map", &Views.MaterialNormalMaps[Index]);
                ImGui::EndDisabled();
                ImGui::BeginDisabled(Current != Pbr);
                ImGui::Checkbox("Studio strip light", &Shared.bStudioStrip);
                ImGui::SetItemTooltip("Show the shared strip light reflection in every PBR viewport.");
                ImGui::EndDisabled();
                const auto &EnvironmentPath = Impl->RunAt(Case).Scene.Appearance.Environment.TexturePath;
                ImGui::BeginDisabled(Current == Flat || EnvironmentPath.empty());
                ImGui::Checkbox("Environment reflections", &Shared.bEnvironmentReflections);
                if (!EnvironmentPath.empty())
                    ImGui::SetItemTooltip("%s (all appearance viewports)", EnvironmentPath.filename().string().c_str());
                ImGui::EndDisabled();
                ImGui::EndDisabled();
            }

            if (ImGui::CollapsingHeader("Surface Display"))
            {
                int Bed = Shared.SmoothBedOverride + 1;
                if (ImGui::Combo("Bed mesh", &Bed, "Scene default\0Sampled\0Smooth\0"))
                    Shared.SmoothBedOverride = Bed - 1;
                ImGui::SetItemTooltip("Display only. Affects all viewports.");
                ImGui::Checkbox("Overflow display", &Shared.bShowOverflow);
                ImGui::SetItemTooltip("Show transfer visuals between connected wet regions.");
                const bool bSmoothWater =
                    bAppearance || (Current == LayerThickness && Source < States.size() &&
                                    States[Source].Channel == Rendering::ESurfaceStateChannel::LiquidMass);
                ImGui::BeginDisabled(!bSmoothWater);
                ImGui::Checkbox("Smooth liquid boundary", &Views.SmoothWater[Index]);
                ImGui::EndDisabled();

                ImGui::BeginDisabled(!bAppearance || Current == Flat);
                ImGui::Checkbox("Slosh reflections and refraction", &Views.SloshOptics[Index]);
                ImGui::SetItemTooltip("Animate water highlights after a change in tilt.");
                ImGui::Checkbox("Slosh water height", &Views.SloshHeight[Index]);
                ImGui::SetItemTooltip(
                    "Requires the Sampled bed mesh and fully wet cells. Preserves volume and wet boundaries.");
                ImGui::EndDisabled();
            }

            if (ImGui::CollapsingHeader("Diagnostics"))
            {
                ImGui::Checkbox("Cell outlines", &Views.CellOutlines[Index]);
                if (Current == LayerThickness || Current == ArealMass)
                {
                    const bool bHasSource = Source < States.size();
                    ImGui::BeginDisabled(!bHasSource);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Source");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(-1.F);
                    if (ImGui::BeginCombo("##DiagnosticSource",
                                          bHasSource ? States[Source].Name.c_str() : "No compatible state"))
                    {
                        for (std::uint32_t Slot = 0; Slot < States.size(); ++Slot)
                        {
                            if (!SupportsStateSource(Current, States[Slot]))
                                continue;
                            ImGui::PushID(static_cast<int>(Slot));
                            if (ImGui::Selectable(States[Slot].Name.c_str(), Slot == Source))
                                StateIndex = Slot;
                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::EndDisabled();
                }
                ImGui::BeginDisabled(!bHeatmap);
                auto &Override = Views.PaletteOverrides[Index];
                int   Palette = Override < 0 ? Shared.Palette : Override;
                if (ImGui::Combo("Heatmap palette", &Palette, HeatmapNames.data(), static_cast<int>(HeatmapNames.size())))
                    Override = Palette;
                ImGui::EndDisabled();
            }
        }

        void FEditorSession::BeginFrame()
        {
            Impl->bRecord = false;
            auto &Run = Impl->Run;
            auto &Ui = Impl->Ui;
            try
            {
                Run.Display->FinishEditorCapture();
                for (int View = 1; View < Impl->RunCount(); ++View)
                    Impl->RunAt(View).Display->FinishEditorCapture();
                if (Ui.Message.starts_with("Saving ") && Impl->Capture.empty())
                {
                    Ui.Message.replace(0, 7, "Saved ");
                    FLogger::Info("TDebugUI", Ui.Message);
                }
            }
            catch (const std::exception &Error)
            {
                Ui.Message = Error.what();
                FLogger::Error("TDebugUI", Ui.Message);
            }
            Impl->PollExperiment();
            Impl->Apply(std::exchange(Impl->PendingAction, ESurfaceSimulationAction::None));
        }

        void FEditorSession::RefreshProfilingReadings()
        {
            const auto &Run = Impl->Run;
            const float Time = static_cast<float>(Run.Steps) * Liquid::StepSeconds;
            const auto &Geometry = Run.Scene.Geometry;
            Impl->DisplayedTimings = Run.Display->Stats();
            Impl->DisplayedWorkload = {Time,
                                       Run.Pending,
                                       Run.Active.GridColumns,
                                       Run.Active.GridRows,
                                       Geometry.Shape.Columns,
                                       Geometry.Shape.Rows,
                                       Geometry.Regions.size(),
                                       Geometry.Faces.size(),
                                       Impl->LastFrameStepCount,
                                       Impl->ViewCount(),
                                       Impl->Ui.bPlaying};
        }

        void FEditorSession::DrawPanel(EEditorPanel Panel, FSceneAssetText SceneText)
        {
            if (Impl->Compare)
            {
                using enum EEditorPanel;
                switch (Panel)
                {
                    case Simulation:
                    Impl->DrawComparisonResolution();
                    for (int View = 0; View < Impl->RunCount(); ++View)
                        Impl->DrawCaseInfo(View);
                    return;
                    case Object:
                    case Assets:
                    case Conditions:
                    ImGui::TextWrapped("Active comparison settings");
                    for (int View = 0; View < Impl->RunCount(); ++View)
                        Impl->DrawCaseInfo(View);
                    return;
                    case Tools:
                    ImGui::TextWrapped("Comparison runs use the authored inputs.");
                    return;
                    case ApplySettings:
                    Impl->DrawComparisonApply();
                    return;
                    case Experiments:
                    ImGui::TextWrapped("Open an original scene to run the CPU/GPU experiment.");
                    return;
                    default:
                    break;
                }
            }
            auto       &Run = Impl->Run;
            const float Time = static_cast<float>(Run.Steps) * Liquid::StepSeconds;
            const float TiltDegrees = std::atan2(Run.Pose.Sine, Run.Pose.Cosine) * 57.2957795F;
            const auto  Action = Impl->Ui.DrawPanel(Panel,
                                                    Run.Active,
                                                    Run.Scene.Geometry.Liquid,
                                                    Time,
                                                    TiltDegrees,
                                                    Impl->DisplayedTimings,
                                                    SceneText);
            if (Action != ESurfaceSimulationAction::None)
                Impl->PendingAction = Action;
        }

        void FEditorSession::DrawExperimentPlot()
        {
            Impl->Ui.DrawExpandedExperiment();
        }

        std::string FEditorSession::GetWaterSourceStatus() const
        {
            return !Impl->Compare && Impl->Ui.bShowWaterSource ? Impl->Run.WaterSourceStatus(Impl->Ui) : std::string{};
        }

        const char *FEditorSession::GetOutletStatus() const
        {
            if (Impl->Compare)
            {
                const bool First = Impl->Run.HasOpenOutlet();
                const int  Count = Impl->RunCount();
                for (int View = 1; View < Count; ++View)
                    if (Impl->RunAt(View).HasOpenOutlet() != First)
                        return Count > 2 ? "Outlets vary by condition"
                               : First   ? "Outlets: Open +X / Closed"
                                         : "Outlets: Closed / Open +X";
                if (Count == 2)
                    return First ? "Both outlets: Open +X" : "Both outlets: Closed";
                return First ? "All outlets: Open +X" : "All outlets: Closed";
            }
            return Impl->Run.HasOpenOutlet() ? "Outlet: Open +X" : "Outlet: Closed";
        }

        void FEditorSession::DrawProfilingWorkload() const
        {
            EditorLayout::SectionTitle("Simulation Workload");
            const auto &Workload = Impl->DisplayedWorkload;
            const auto  Row = [](const char *Label, const char *Format, auto... Values)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(Label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(Format, Values...);
            };
            if (ImGui::BeginTable("ChartWorkload", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 180.F);
                ImGui::TableSetupColumn("Value");
                Row("Playback", "%s", Workload.bPlaying ? "Running" : "Paused");
                Row("Simulation time", "%.2f s", Workload.Time);
                Row("Pending simulation", "%.2f ms", Workload.Backlog * 1000.0);
                Row("Steps per frame", "%zu", Workload.Steps);
                Row("Solver grid", "%d x %d", Workload.GridColumns, Workload.GridRows);
                Row("Geometry samples", "%d x %d", Workload.SampleColumns, Workload.SampleRows);
                Row("Liquid regions / Faces", "%zu / %zu", Workload.Regions, Workload.Faces);
                Row("Visible views", "%d", Workload.Views);
                ImGui::EndTable();
            }
        }

        void FEditorSession::DrawViewport(const ImVec2 &Position, const ImVec2 &Size, bool bPresentation)
        {
            // 뷰 바깥쪽 선택 테두리가 도킹 창의 경계에 잘리지 않도록 여백을 확보한다.
            constexpr float SelectionWidth = 2.F;
            const float     Margin = bPresentation ? 0.F : SelectionWidth + 1.F;
            const ImVec2    ImageSize{Size.x - 2.F * Margin, Size.y - 2.F * Margin};
            if (ImageSize.x < 1.F || ImageSize.y < 1.F)
                return;
            ImGui::SetNextWindowPos(Position);
            ImGui::SetNextWindowSize(Size);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.F, 0.F});
            constexpr auto Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoMove;
            if (ImGui::Begin("Scene View##SurfaceSimulation", nullptr, Flags))
            {
                const bool   bPaired = bool(Impl->Compare);
                const int    Cases = Impl->RunCount();
                const int    Columns = Cases > 1 ? 2 : 1;
                const int    Rows = Cases > 2 ? 2 : 1;
                const ImVec2 Origin{Position.x + Margin, Position.y + Margin};
                const ImVec2 CaseSize{ImageSize.x / static_cast<float>(Columns), ImageSize.y / static_cast<float>(Rows)};
                const auto   Scale = ImGui::GetIO().DisplayFramebufferScale;
                const VkExtent2D Extent{static_cast<std::uint32_t>(std::max(1.F, CaseSize.x * Scale.x)),
                                        static_cast<std::uint32_t>(std::max(1.F, CaseSize.y * Scale.y))};
                const ImVec2     PixelSize{CaseSize.x / Extent.width, CaseSize.y / Extent.height};
                ImVec2           SelectionMin = Origin;
                ImVec2           SelectionMax = Origin;
                bool             bHovered = false;
                auto            &Draw = *ImGui::GetWindowDrawList();
                const auto      &Ui = Impl->Ui;
                const auto       Mouse = ImGui::GetMousePos();
                for (int Case = 0; Case < Cases; ++Case)
                {
                    const ImVec2 CaseOrigin{Origin.x + static_cast<float>(Case % Columns) * CaseSize.x,
                                            Origin.y + static_cast<float>(Case / Columns) * CaseSize.y};
                    const auto   Views = Impl->CaseViews(Case);
                    auto        &Run = Impl->RunAt(Case);
                    ImGui::SetCursorScreenPos(CaseOrigin);
                    const auto Target = Run.Display->PrepareEditorTarget(Extent, Views);
                    ImGui::Image(reinterpret_cast<ImTextureID>(Target), CaseSize);
                    const bool bImageHovered = ImGui::IsItemHovered();
                    const bool bClicked = !bPresentation && ImGui::IsItemClicked();
                    bHovered |= bImageHovered;
                    if (!bPaired && !bPresentation && bImageHovered)
                    {
                        const auto FirstView = SurfaceSimulationViewRect(Extent, Views, 0).extent;
                        Impl->ProcessInjectionPreview(Draw,
                                                      Origin,
                                                      ImageSize,
                                                      {FirstView.width * PixelSize.x, FirstView.height * PixelSize.y});
                    }

                    const bool bShowOutlet = !bPresentation && Ui.bShowOutletEdge && Run.HasOpenOutlet();
                    const bool bShowInlet = Ui.bShowWaterSource && Run.Scene.PourKg > 0.F;
                    const bool bPouring = bShowInlet && Run.IsScheduledInletActive(Ui);
                    const auto OverlayViews = bShowOutlet || bShowInlet ? Run.FrameViews(Ui, Views) : Views;
                    const auto SourceStatus =
                        bPresentation && Ui.bShowWaterSource ? Run.WaterSourceStatus(Ui) : std::string{};
                    for (int View = 0; View < Views.Count; ++View)
                    {
                        const auto   Rect = SurfaceSimulationViewRect(Extent, Views, View);
                        const ImVec2 Min{CaseOrigin.x + Rect.offset.x * PixelSize.x,
                                         CaseOrigin.y + Rect.offset.y * PixelSize.y};
                        const ImVec2 Max{Min.x + Rect.extent.width * PixelSize.x, Min.y + Rect.extent.height * PixelSize.y};
                        if (bClicked && Mouse.x >= Min.x && Mouse.x < Max.x && Mouse.y >= Min.y && Mouse.y < Max.y)
                        {
                            Impl->SelectedCase = Case;
                            Impl->SelectedViewport = View;
                        }
                        if (Case == Impl->SelectedCase && View == Impl->SelectedViewport)
                        {
                            SelectionMin = Min;
                            SelectionMax = Max;
                        }

                        Draw.PushClipRect(Min, Max, true);
                        if (bPaired)
                            Draw.AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_Border));
                        if (bShowOutlet)
                            DrawOutletOverlay(Draw, Min, Max, Run.Display->ProjectOutletEdge(OverlayViews, View));
                        const char *SourceLabel = View == 0 && !SourceStatus.empty() ? SourceStatus.c_str() : nullptr;
                        if (bShowInlet || SourceLabel)
                        {
                            const auto Overlay =
                                bShowInlet ? Run.Display->ProjectInletOverlay(OverlayViews, View) : FSurfaceInletOverlay{};
                            DrawWaterSourceOverlay(Draw, Min, Max, Overlay, SourceLabel, bPouring);
                        }
                        if (bPaired && View == 0)
                        {
                            const auto  &Label = Impl->Compare->Asset.Labels[Case];
                            const auto   LabelSize = ImGui::CalcTextSize(Label.c_str());
                            const ImVec2 Text{Min.x + 13.F, Max.y - LabelSize.y - 12.F};
                            Draw.AddRectFilled({Text.x - 6.F, Text.y - 4.F},
                                               {Text.x + LabelSize.x + 6.F, Text.y + LabelSize.y + 4.F},
                                               IM_COL32(20, 27, 36, 230),
                                               4.F);
                            Draw.AddText(Text, IM_COL32(240, 245, 247, 255), Label.c_str());
                        }
                        const auto  Mode = static_cast<ESurfaceSimulationView>(Views.Modes[View]);
                        const auto  State = Views.StateIndices[View];
                        const auto &States = Run.Active.States;
                        using enum ESurfaceSimulationView;
                        const bool bTemperature =
                            Mode == Temperature || (Mode == StateHeatmap && State < States.size() &&
                                                    States[State].Channel == Rendering::ESurfaceStateChannel::Temperature);
                        if (!bPresentation && Impl->ViewLabelsAt(Case)[View])
                        {
                            const int Palette =
                                Views.PaletteOverrides[View] < 0 ? Views.Palette : Views.PaletteOverrides[View];
                            DrawViewLabel(Draw,
                                          Min,
                                          Mode,
                                          Palette,
                                          States,
                                          State,
                                          Run.Scene,
                                          bPaired,
                                          bTemperature,
                                          bTemperature ? Run.Display->TemperatureRangeK() : std::nullopt);
                        }
                        if (!bPresentation && bTemperature && bImageHovered && !Ui.bInjectionEnabled &&
                            !Impl->bOrbitDragging && Mouse.x >= Min.x && Mouse.x < Max.x && Mouse.y >= Min.y &&
                            Mouse.y < Max.y)
                        {
                            const glm::vec2 Position{(Mouse.x - CaseOrigin.x) / CaseSize.x,
                                                     (Mouse.y - CaseOrigin.y) / CaseSize.y};
                            if (const auto TemperatureK = Run.Display->PickTemperatureK(Position))
                                ImGui::SetTooltip("Substrate  %.1f \xC2\xB0"
                                                  "C",
                                                  *TemperatureK - 273.15F);
                        }
                        Draw.PopClipRect();
                    }
                }
                Impl->bRecord = true;
                Impl->bPresentation = bPresentation;
                if (bPresentation)
                    Impl->bOrbitDragging = false;
                else
                    Impl->ProcessCameraInput(bHovered);

                if (!bPresentation)
                {
                    const ImVec2 Min = SelectionMin;
                    const ImVec2 Max = SelectionMax;
                    const ImU32  Color = ImGui::GetColorU32(ImGuiCol_CheckMark);
                    // 개별 뷰의 clip 범위를 벗어나 이웃 뷰를 그린 뒤 바깥쪽 네 변을 올린다.
                    Draw.PushClipRect(Position, {Position.x + Size.x, Position.y + Size.y}, false);
                    Draw.AddRectFilled({Min.x - SelectionWidth, Min.y - SelectionWidth},
                                       {Max.x + SelectionWidth, Min.y},
                                       Color);
                    Draw.AddRectFilled({Min.x - SelectionWidth, Max.y},
                                       {Max.x + SelectionWidth, Max.y + SelectionWidth},
                                       Color);
                    Draw.AddRectFilled({Min.x - SelectionWidth, Min.y}, {Min.x, Max.y}, Color);
                    Draw.AddRectFilled({Max.x, Min.y}, {Max.x + SelectionWidth, Max.y}, Color);
                    Draw.PopClipRect();
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        void FEditorSession::Restart()
        {
            Impl->PendingAction = ESurfaceSimulationAction::Restart;
        }

        void FEditorSession::Record(VkCommandBuffer Commands, std::uint32_t FrameSlot)
        {
            const auto Now = std::chrono::steady_clock::now();
            const auto Elapsed = std::clamp(std::chrono::duration<double>(Now - Impl->Previous).count(), 0.0, 0.1);
            Impl->Previous = Now;
            if (!Impl->bRecord)
                return;

            const bool  bPaired = bool(Impl->Compare);
            std::size_t StepCount = 0;
            for (int Side = 0; Side < Impl->RunCount(); ++Side)
            {
                auto                                                  &Run = Impl->RunAt(Side);
                std::array<Liquid::FSurfaceStepInput, MaxSubsteps * 4> Inputs{};
                const auto                                             Count = Run.PrepareInputs(Impl->Ui, Elapsed, Inputs);
                const auto                                             ActiveInputs = std::span(Inputs).first(Count);
                auto Views = Run.FrameViews(Impl->Ui, Impl->CaseViews(Side));
                if (Impl->bPresentation)
                {
                    Views.bWorldGrid = false;
                    Views.bWorldAxes = false;
                }
                auto Capture = Impl->Capture;
                if (bPaired && !Capture.empty())
                {
                    if (Impl->CaptureCase >= 0 && Impl->CaptureCase != Side)
                        Capture.clear();
                    else
                        Capture = ComparisonCapturePath(Capture, Side);
                }
                // Render Options는 뷰포트 UI 뒤에 그려지므로 최종 모드로 자원을 준비한다.
                Run.Display->PrepareBedDisplay(Run.Scene.Geometry, Run.Scene.Appearance, Views);
                Run.Display->RecordEditorFrame(Commands,
                                               FrameSlot,
                                               *Run.Simulation,
                                               ActiveInputs,
                                               Views,
                                               Capture,
                                               Impl->CaptureView);
                Run.CommitFrame(ActiveInputs);
                StepCount = std::max(StepCount, Count);
            }
            Impl->LastFrameStepCount = StepCount;
            if (StepCount > 0)
                Impl->Ui.bSingleStep = false;
            Impl->Capture.clear();
        }
    #pragma endregion

} // namespace MDSS::SurfaceSimulation
