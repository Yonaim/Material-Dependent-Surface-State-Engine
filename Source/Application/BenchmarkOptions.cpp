/**
 * @file BenchmarkOptions.cpp
 * @brief Validates benchmark and frame-limit command-line arguments.
 */
#include "Application/BenchmarkOptions.h"

#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace MDSS
{
    namespace
    {
        std::uint32_t ParsePositiveInteger(std::string_view Value, std::string_view Option)
        {
            std::uint32_t Result = 0;
            const auto [End, Error] = std::from_chars(Value.data(), Value.data() + Value.size(), Result);
            if (Error != std::errc{} || End != Value.data() + Value.size() || Result == 0)
                throw std::invalid_argument(std::string(Option) + " requires a positive integer.");
            return Result;
        }

        std::uint32_t ParseNonNegativeInteger(std::string_view Value, std::string_view Option)
        {
            std::uint32_t Result = 0;
            const auto [End, Error] = std::from_chars(Value.data(), Value.data() + Value.size(), Result);
            if (Error != std::errc{} || End != Value.data() + Value.size())
                throw std::invalid_argument(std::string(Option) + " requires a nonnegative integer.");
            return Result;
        }

        std::uint32_t ParseResolution(std::string_view Value)
        {
            const std::uint32_t Resolution = ParsePositiveInteger(Value, "--benchmark-resolution");
            if (Resolution != 128 && Resolution != 256 && Resolution != 512)
                throw std::invalid_argument("--benchmark-resolution must be 128, 256, or 512.");
            return Resolution;
        }
    } // namespace

#pragma region std_Implementation

    std::size_t TBenchmarkOptions::GetTotalFrameLimit() const noexcept
    {
        // Two extra frames drain the timestamp queries for the final measured frames.
        return static_cast<std::size_t>(WarmupFrames) + MeasurementFrames + 2U;
    }

    TApplicationLaunchOptions ParseApplicationLaunchOptions(int Argc, char* Argv[])
    {
        TApplicationLaunchOptions Options;
        bool                     HasBenchmarkScene = false;
        bool                     HasBenchmarkResolution = false;
        bool                     HasWarmupFrames = false;
        bool                     HasMeasurementFrames = false;
        bool                     HasBenchmarkOutput = false;
        bool                     HasFrameLimit = false;

        for (int Index = 1; Index < Argc; ++Index)
        {
            const std::string_view Argument(Argv[Index]);
            const auto ReadValue = [&]() -> std::string_view
            {
                if (Index + 1 >= Argc)
                    throw std::invalid_argument(std::string(Argument) + " requires a value.");
                return Argv[++Index];
            };

            if (Argument == "--frames")
            {
                Options.FrameLimit = ParsePositiveInteger(ReadValue(), Argument);
                HasFrameLimit = true;
            }
            else if (Argument == "--render-tile-size")
            {
                Options.RenderTileSize = ParsePositiveInteger(ReadValue(), Argument);
                if (Options.RenderTileSize != 8U && Options.RenderTileSize != 16U &&
                    Options.RenderTileSize != 32U)
                    throw std::invalid_argument("--render-tile-size must be 8, 16, or 32.");
            }
            else if (Argument == "--per-wg-channel-mask")
            {
                Options.bPerWorkgroupChannelMask = true;
            }
            else if (Argument == "--benchmark-scene")
            {
                Options.Benchmark.ScenePath = std::filesystem::path(std::string(ReadValue()));
                HasBenchmarkScene = true;
            }
            else if (Argument == "--benchmark-resolution")
            {
                Options.Benchmark.Resolution = ParseResolution(ReadValue());
                HasBenchmarkResolution = true;
            }
            else if (Argument == "--benchmark-warmup-frames")
            {
                Options.Benchmark.WarmupFrames = ParseNonNegativeInteger(ReadValue(), Argument);
                HasWarmupFrames = true;
            }
            else if (Argument == "--benchmark-measure-frames")
            {
                Options.Benchmark.MeasurementFrames = ParsePositiveInteger(ReadValue(), Argument);
                HasMeasurementFrames = true;
            }
            else if (Argument == "--benchmark-output")
            {
                Options.Benchmark.OutputPath = std::filesystem::path(std::string(ReadValue()));
                HasBenchmarkOutput = true;
            }
            else
            {
                throw std::invalid_argument("Unknown argument: " + std::string(Argument));
            }
        }

        const bool AnyBenchmarkArgument = HasBenchmarkScene || HasBenchmarkResolution || HasWarmupFrames ||
                                          HasMeasurementFrames || HasBenchmarkOutput;
        if (AnyBenchmarkArgument)
        {
            if (HasFrameLimit)
                throw std::invalid_argument("Do not combine --frames with benchmark options.");
            if (!HasBenchmarkScene || !HasBenchmarkResolution || !HasWarmupFrames || !HasMeasurementFrames ||
                !HasBenchmarkOutput)
                throw std::invalid_argument("All --benchmark-* options are required for a benchmark run.");
            if (Options.Benchmark.ScenePath.empty() || Options.Benchmark.OutputPath.empty())
                throw std::invalid_argument("Benchmark Scene and output paths must not be empty.");
            const std::uint64_t TotalFrames = static_cast<std::uint64_t>(Options.Benchmark.WarmupFrames) +
                                              Options.Benchmark.MeasurementFrames + 2U;
            if (TotalFrames > std::numeric_limits<std::size_t>::max())
                throw std::invalid_argument("Benchmark frame count is too large.");
            Options.Benchmark.Enabled = true;
            Options.FrameLimit = Options.Benchmark.GetTotalFrameLimit();
        }
        else if (HasFrameLimit)
        {
            Options.Benchmark.Enabled = false;
        }

        return Options;
    }

    const char* GetApplicationUsage() noexcept
    {
        return "Usage: MDSS [--frames COUNT] [--render-tile-size {8|16|32}] [--per-wg-channel-mask] | "
               "MDSS --benchmark-scene PATH --benchmark-resolution {128|256|512} "
               "--benchmark-warmup-frames COUNT --benchmark-measure-frames COUNT --benchmark-output PATH "
               "[--render-tile-size {8|16|32}] [--per-wg-channel-mask]";
    }
#pragma endregion
} // namespace MDSS
