/**
 * @file Logger.h
 * @brief 모듈별 로그 기록과 로그 항목 조회.
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MDSS
{
    enum class ELogLevel : std::uint8_t
    {
        Verbose = 0,
        Debug,
        Info,
        Warning,
        Error,
        Count
    };

    struct FLogEntry
    {
        std::uint64_t Sequence = 0;
        ELogLevel     Level = ELogLevel::Info;
        std::string   Module;
        std::string   Message;
        std::string   Formatted;
    };

    // Process-wide development logger.
    // Every entry is written to the terminal immediately and retained so FDebugUI
    // can present the same log stream without coupling engine modules to ImGui.
    class FLogger final
    {
    public:
        // Log output
        static void Verbose(std::string_view Module, std::string_view Message);
        static void Debug(std::string_view Module, std::string_view Message);
        static void Info(std::string_view Module, std::string_view Message);
        static void Warning(std::string_view Module, std::string_view Message);
        static void Error(std::string_view Module, std::string_view Message);

        /** @brief 로그 항목을 콘솔에 기록하고 thread-safe history에 보관한다. */
        static void Write(ELogLevel Level, std::string_view Module, std::string_view Message);

        // Retained log history
        /** @brief 현재 로그 history의 thread-safe 복사본을 반환한다. */
        static std::vector<FLogEntry> GetEntries();
        /** @brief 로그가 추가되거나 지워질 때 증가하는 revision을 반환한다. */
        static std::uint64_t GetRevision();
        /** @brief 콘솔에는 영향을 주지 않고 보관 중인 로그 history를 비운다. */
        static void Clear();

        // Level display names
        static const char* GetLevelName(ELogLevel Level) noexcept;

    private:
        FLogger() = delete;
    };
} // namespace MDSS
