#include "Logger/Logger.h"

#include <cstddef>
#include <iostream>
#include <mutex>
#include <utility>

namespace MDSS
{
    namespace
    {
        struct LoggerStorage
        {
            std::mutex            Mutex;
            std::vector<LogEntry> Entries;
            std::uint64_t         NextSequence = 1;
            std::uint64_t         Revision = 0;
        };

        LoggerStorage& GetStorage()
        {
            static LoggerStorage Storage;
            return Storage;
        }

        constexpr std::size_t MaxRetainedEntries = 10000;
    } // namespace

    void Logger::Verbose(std::string_view Module, std::string_view Message)
    {
        Write(LogLevel::Verbose, Module, Message);
    }

    void Logger::Debug(std::string_view Module, std::string_view Message)
    {
        Write(LogLevel::Debug, Module, Message);
    }

    void Logger::Info(std::string_view Module, std::string_view Message)
    {
        Write(LogLevel::Info, Module, Message);
    }

    void Logger::Warning(std::string_view Module, std::string_view Message)
    {
        Write(LogLevel::Warning, Module, Message);
    }

    void Logger::Error(std::string_view Module, std::string_view Message)
    {
        Write(LogLevel::Error, Module, Message);
    }

    void Logger::Write(LogLevel Level, std::string_view Module, std::string_view Message)
    {
        const std::string ModuleText = Module.empty() ? "General" : std::string(Module);
        const std::string MessageText(Message);
        const std::string Formatted =
            "[" + std::string(GetLevelName(Level)) + "][" + ModuleText + "] " + MessageText;

        LoggerStorage& Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);

        // Keep terminal output and DebugUI backed by the exact same formatted entry.
        std::ostream& Stream = (Level == LogLevel::Warning || Level == LogLevel::Error) ? std::cerr : std::cout;
        Stream << Formatted << '\n';
        Stream.flush();

        if (Storage.Entries.size() >= MaxRetainedEntries)
        {
            Storage.Entries.erase(Storage.Entries.begin(),
                                  Storage.Entries.begin() +
                                      static_cast<std::ptrdiff_t>(MaxRetainedEntries / 10));
        }

        Storage.Entries.push_back({Storage.NextSequence++, Level, ModuleText, MessageText, Formatted});
        ++Storage.Revision;
    }

    std::vector<LogEntry> Logger::GetEntries()
    {
        LoggerStorage& Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Entries;
    }

    std::uint64_t Logger::GetRevision()
    {
        LoggerStorage& Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Revision;
    }

    void Logger::Clear()
    {
        LoggerStorage& Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        Storage.Entries.clear();
        ++Storage.Revision;
    }

    const char* Logger::GetLevelName(LogLevel Level) noexcept
    {
        switch (Level)
        {
        case LogLevel::Verbose:
            return "VERBOSE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warning:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        case LogLevel::Count:
            break;
        }
        return "UNKNOWN";
    }
} // namespace MDSS
