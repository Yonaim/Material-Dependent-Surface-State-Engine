/**
 * @file BenchmarkOptions.h
 * @brief Command-line settings for a repeatable fixed-step benchmark run.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace MDSS
{
    struct TBenchmarkOptions
    {
        bool                 Enabled = false;
        std::filesystem::path ScenePath;
        std::filesystem::path OutputPath;
        std::uint32_t        Resolution = 256;
        std::uint32_t        WarmupFrames = 0;
        std::uint32_t        MeasurementFrames = 0;

        // Benchmark frame limits
        [[nodiscard]] std::size_t GetTotalFrameLimit() const noexcept;
    };

    struct TApplicationLaunchOptions
    {
        std::size_t       FrameLimit = 0;
        TBenchmarkOptions Benchmark;
    };

    // Command-line parsing and usage text
    [[nodiscard]] TApplicationLaunchOptions ParseApplicationLaunchOptions(int Argc, char* Argv[]);
    [[nodiscard]] const char* GetApplicationUsage() noexcept;
} // namespace MDSS
