/**
 * @file ApplicationLaunchOptions.h
 * @brief 명령행 인수를 해석하고 실행 옵션의 유효성을 검사한다.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace MDSS
{
    enum class ESurfaceSimulationLaunch
    {
        None,
        Reference,
        GpuCheck,
        Demo
    };

    struct FApplicationLaunchOptions
    {
        std::size_t                FrameLimit = 0;
        std::filesystem::path      ScenePath;
        ESurfaceSimulationLaunch   Lab = ESurfaceSimulationLaunch::None;
        int                        LabScene = -1;
        bool                       bLabProfileBasins = false;
        int                        LabViews = 1;
        std::array<int, 4>         LabViewModes{};
        std::array<std::string, 4> LabViewSources{};
        int                        LabViewModeCount = 0;
        int                        LabShading = -1;
        int                        LabWaterEdges = -1;
        int                        LabWaterSlosh = -1;
        bool                       bLabTextures = true;
        bool                       bLabStudioStrip = true;
        bool                       bLabEnvironmentReflections = true;
        bool                       bLabMaterialNormalMaps = true;
        bool                       bLabAnimateCamera = false;
        std::filesystem::path      LabCapture;
        std::filesystem::path      LabLiquidMaterial;
        std::filesystem::path      LabOutput;
        std::filesystem::path      LabGeometryFixtures;
    };

    // Command-line parsing and usage text
    [[nodiscard]] FApplicationLaunchOptions ParseApplicationLaunchOptions(int Argc, char *Argv[]);
    [[nodiscard]] const char               *GetApplicationUsage() noexcept;
} // namespace MDSS
