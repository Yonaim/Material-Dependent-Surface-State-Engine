#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace MDSS
{
    enum class LogLevel : std::uint8_t
    {
        Verbose = 0,
        Debug,
        Info,
        Warning,
        Error,
        Count
    };

    struct LogEntry
    {
        std::uint64_t Sequence = 0;
        LogLevel      Level = LogLevel::Info;
        std::string   Module;
        std::string   Message;
        std::string   Formatted;
    };

    // Process-wide development logger.
    // Every entry is written to the terminal immediately and retained so DebugUI
    // can present the same log stream without coupling engine modules to ImGui.
    class Logger final
    {
    public:
        static void Verbose(std::string_view Module, std::string_view Message);
        static void Debug(std::string_view Module, std::string_view Message);
        static void Info(std::string_view Module, std::string_view Message);
        static void Warning(std::string_view Module, std::string_view Message);
        static void Error(std::string_view Module, std::string_view Message);

        static void Write(LogLevel Level, std::string_view Module, std::string_view Message);

        // Returns a thread-safe snapshot for debug/editor presentation.
        static std::vector<LogEntry> GetEntries();
        static std::uint64_t         GetRevision();
        static void                  Clear();

        static const char* GetLevelName(LogLevel Level) noexcept;

    private:
        Logger() = delete;
    };
} // namespace MDSS
