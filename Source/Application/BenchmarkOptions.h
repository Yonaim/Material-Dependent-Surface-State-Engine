/**
 * @file BenchmarkOptions.h
 * @brief benchmark 명령행 인수를 해석하고 실행 옵션의 유효성을 검사한다.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace MDSS
{
    struct FBenchmarkOptions
    {
        bool                  bEnabled = false;
        std::filesystem::path ScenePath;
        std::filesystem::path OutputPath;
        std::uint32_t         Resolution = 256;
        std::uint32_t         WarmupFrames = 0;
        std::uint32_t         MeasurementFrames = 0;

        // Benchmark frame limits
        [[nodiscard]] std::size_t GetTotalFrameLimit() const noexcept;
    };

    struct FApplicationLaunchOptions
    {
        std::size_t           FrameLimit = 0;
        std::uint32_t         RenderTileSize = 16;
        bool                  bPerWorkgroupChannelMask = true;
        std::filesystem::path ScenePath;
        FBenchmarkOptions     Benchmark;
    };

    // Command-line parsing and usage text
    [[nodiscard]] FApplicationLaunchOptions ParseApplicationLaunchOptions(int Argc, char* Argv[]);
    [[nodiscard]] const char*               GetApplicationUsage() noexcept;
} // namespace MDSS
