/**
 * @file ApplicationLaunchOptions.cpp
 * @brief benchmark 명령행 인수를 해석하고 실행 옵션의 유효성을 검사한다.
 */
#include "Application/ApplicationLaunchOptions.h"
#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include "Rendering/Surface/Simulation/SurfaceSimulationDisplay.h"
#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>

namespace MDSS
{
    namespace
    {
        int ParseSurfaceSimulationView(std::string_view Value)
        {
            if (Value == "depth")
                return static_cast<int>(SurfaceSimulation::ESurfaceSimulationView::LayerThickness);
            const auto &Keys = SurfaceSimulation::SurfaceSimulationViewKeys;
            const auto  Found = std::ranges::find_if(Keys, [Value](const char *Key) { return Value == Key; });
            if (Found == Keys.end())
                throw std::invalid_argument("Unknown Surface Simulation view: " + std::string(Value));
            return static_cast<int>(Found - Keys.begin());
        }

        std::uint32_t ParsePositiveInteger(std::string_view Value, std::string_view Option)
        {
            std::uint32_t Result = 0;
            const auto [End, Error] = std::from_chars(Value.data(), Value.data() + Value.size(), Result);

            if (Error != std::errc{} || End != Value.data() + Value.size() || Result == 0)
                throw std::invalid_argument(std::string(Option) + " requires a positive integer.");
            return Result;
        }

    } // namespace

// ============================================================
// 00. std Implementation
// ============================================================
#pragma region 00_std_Implementation
    // benchmark 명령행 인수를 해석하고 실행 옵션의 유효성을 검사한다.

    FApplicationLaunchOptions ParseApplicationLaunchOptions(int Argc, char *Argv[])
    {
        FApplicationLaunchOptions Options;
        bool                      bHasFrameLimit = false;

        for (int Index = 1; Index < Argc; ++Index)
        {
            const std::string_view Argument(Argv[Index]);
            const auto             ReadValue = [&]() -> std::string_view
            {
                if (Index + 1 >= Argc)
                    throw std::invalid_argument(std::string(Argument) + " requires a value.");
                return Argv[++Index];
            };

            if (Argument == "--frames")
            {
                Options.FrameLimit = ParsePositiveInteger(ReadValue(), Argument);
                bHasFrameLimit = true;
            }
            else if (Argument == "--capture-surface" || Argument == "--reference-surface" ||
                     Argument == "--check-surface")
            {
                if (Options.Lab != ESurfaceSimulationLaunch::None)
                    throw std::invalid_argument("Select one Surface Simulation launch mode.");
                if (Argument == "--capture-surface")
                    Options.Lab = ESurfaceSimulationLaunch::Demo;
                else if (Argument == "--reference-surface")
                    Options.Lab = ESurfaceSimulationLaunch::Reference;
                else
                    Options.Lab = ESurfaceSimulationLaunch::GpuCheck;
            }
            else if (Argument == "--surface-scene")
            {
                const auto  Name = ReadValue();
                const auto &Keys = SurfaceSimulation::SceneKeys;
                const auto  Found = std::ranges::find_if(Keys, [Name](const char *Key) { return Name == Key; });
                if (Found == Keys.end())
                    throw std::invalid_argument(
                        "Use runoff, textile, charring, terrace, kiln, cascade, ice, cavity, freeze-thaw or "
                        "meadow, ice-gate, soil-path or absorption-comparison for --surface-scene.");
                Options.LabScene = static_cast<int>(Found - Keys.begin());
            }
            else if (Argument == "--surface-profile-basins")
                Options.bLabProfileBasins = true;
            else if (Argument == "--surface-views")
            {
                const auto Views = ParsePositiveInteger(ReadValue(), Argument);
                if (Views > 4)
                    throw std::invalid_argument("--surface-views expects a number from 1 to 4.");
                Options.LabViews = static_cast<int>(Views);
            }
            else if (Argument == "--surface-shading")
                Options.LabShading = ParseSurfaceSimulationView(ReadValue());
            else if (Argument == "--surface-view-modes")
            {
                auto Remaining = ReadValue();
                Options.LabViewModeCount = 0;
                for (;;)
                {
                    if (Options.LabViewModeCount >= static_cast<int>(Options.LabViewModes.size()))
                        throw std::invalid_argument("--surface-view-modes accepts up to four comma-separated views.");
                    const auto Separator = Remaining.find(',');
                    const auto Entry = Remaining.substr(0, Separator);
                    const auto SourceSeparator = Entry.find(':');
                    const int  Slot = Options.LabViewModeCount++;
                    const int  View = ParseSurfaceSimulationView(Entry.substr(0, SourceSeparator));
                    Options.LabViewModes[Slot] = View;
                    if (SourceSeparator != std::string_view::npos)
                    {
                        const auto Source = Entry.substr(SourceSeparator + 1);
                        if (Source.empty() || !SurfaceSimulation::UsesStateSource(
                                                  static_cast<SurfaceSimulation::ESurfaceSimulationView>(View)))
                            throw std::invalid_argument("A state source requires a state-based view and a state ID.");
                        Options.LabViewSources[Slot] = Source;
                    }
                    if (Separator == std::string_view::npos)
                        break;
                    Remaining.remove_prefix(Separator + 1);
                }
            }
            else if (Argument == "--surface-no-textures")
                Options.bLabTextures = false;
            else if (Argument == "--surface-no-studio-strip")
                Options.bLabStudioStrip = false;
            else if (Argument == "--surface-no-environment")
                Options.bLabEnvironmentReflections = false;
            else if (Argument == "--surface-no-normal-maps")
                Options.bLabMaterialNormalMaps = false;
            else if (Argument == "--surface-animate-camera")
                Options.bLabAnimateCamera = true;
            else if (Argument == "--surface-water-edges")
            {
                const auto Mode = ReadValue();
                if (Mode != "cell" && Mode != "smooth")
                    throw std::invalid_argument("--surface-water-edges expects cell or smooth.");
                Options.LabWaterEdges = Mode == "smooth" ? 1 : 0;
            }
            else if (Argument == "--surface-water-slosh")
            {
                constexpr std::array<std::string_view, 4> Modes{"none", "optics", "height", "both"};
                const auto                                Mode = std::ranges::find(Modes, ReadValue());
                if (Mode == Modes.end())
                    throw std::invalid_argument("--surface-water-slosh expects none, optics, height or both.");
                Options.LabWaterSlosh = static_cast<int>(Mode - Modes.begin());
            }
            else if (Argument == "--surface-liquid-material")
                Options.LabLiquidMaterial = std::filesystem::path(std::string(ReadValue()));
            else if (Argument == "--surface-capture")
                Options.LabCapture = std::filesystem::path(std::string(ReadValue()));
            else if (Argument == "--surface-output")
                Options.LabOutput = std::filesystem::path(std::string(ReadValue()));
            else if (Argument == "--surface-geometry-fixtures")
                Options.LabGeometryFixtures = std::filesystem::path(std::string(ReadValue()));
            else if (Argument == "--scene")
            {
                Options.ScenePath = std::filesystem::path(std::string(ReadValue()));

                if (Options.ScenePath.empty())
                    throw std::invalid_argument("--scene requires a non-empty path.");
            }
            else
            {
                throw std::invalid_argument("Unknown argument: " + std::string(Argument));
            }
        }

        if (Options.Lab != ESurfaceSimulationLaunch::None && bHasFrameLimit &&
            Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--frames requires an editor or capture run.");
        if (Options.LabScene >= 0 && !Options.ScenePath.empty())
            throw std::invalid_argument("Choose either --scene or --surface-scene.");
        if (Options.Lab == ESurfaceSimulationLaunch::Demo && !bHasFrameLimit)
            throw std::invalid_argument(
                "Offline capture requires --frames COUNT. Open a .Scene in the editor to interact.");
        if (!Options.bLabTextures && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-no-textures requires --capture-surface.");
        if (!Options.bLabStudioStrip && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-no-studio-strip requires --capture-surface.");
        if (!Options.bLabEnvironmentReflections && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-no-environment requires --capture-surface.");
        if (!Options.bLabMaterialNormalMaps && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-no-normal-maps requires --capture-surface.");
        if (Options.bLabAnimateCamera && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-animate-camera requires --capture-surface.");
        if (Options.LabWaterEdges >= 0 && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-water-edges requires --capture-surface.");
        if (Options.LabWaterSlosh >= 0 && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-water-slosh requires --capture-surface.");
        if (Options.LabViewModeCount > 0 &&
            (Options.Lab != ESurfaceSimulationLaunch::Demo || Options.LabShading >= 0 || Options.LabViews != 1))
            throw std::invalid_argument(
                "--surface-view-modes requires --capture-surface and sets its own views and modes.");
        if (!Options.LabLiquidMaterial.empty() && Options.Lab != ESurfaceSimulationLaunch::Demo)
            throw std::invalid_argument("--surface-liquid-material requires --capture-surface.");
        if (!Options.LabCapture.empty() && (Options.Lab != ESurfaceSimulationLaunch::Demo || !bHasFrameLimit))
            throw std::invalid_argument("--surface-capture requires --capture-surface and --frames COUNT.");
        if (!Options.LabGeometryFixtures.empty() && Options.Lab != ESurfaceSimulationLaunch::Reference &&
            Options.Lab != ESurfaceSimulationLaunch::GpuCheck)
            throw std::invalid_argument("--surface-geometry-fixtures requires a Surface Simulation check.");
        if (Options.bLabProfileBasins &&
            (Options.Lab != ESurfaceSimulationLaunch::GpuCheck || Options.LabOutput.empty()))
            throw std::invalid_argument("--surface-profile-basins requires --check-surface and --surface-output.");
        if (Options.Lab == ESurfaceSimulationLaunch::None &&
            (Options.LabScene >= 0 || Options.LabViews != 1 || Options.LabShading != -1 || !Options.LabOutput.empty()))
            throw std::invalid_argument("Use --capture-surface with --surface-* options.");
        return Options;
    }

    const char *GetApplicationUsage() noexcept
    {
        return "Usage: MDSS [--scene PATH] [--frames COUNT] | "
               "MDSS --capture-surface --frames COUNT [--surface-scene NAME] [--surface-views 1..4] "
               "[--surface-shading MODE | --surface-view-modes MODE[:STATE_ID][,MODE...]] [--surface-no-textures] "
               "[--surface-no-studio-strip] [--surface-no-environment] [--surface-no-normal-maps] "
               "[--surface-capture PNG] "
               "[--surface-water-edges cell|smooth] "
               "[--surface-water-slosh none|optics|height|both] [--surface-animate-camera] [--surface-liquid-material "
               "JSON] | "
               "--reference-surface | --check-surface [--surface-output DIRECTORY] [--surface-profile-basins]";
    }
#pragma endregion
} // namespace MDSS
