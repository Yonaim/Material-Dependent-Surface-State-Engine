/**
 * @file Logger.cpp
 * @brief 모듈별 로그 기록과 로그 항목 조회.
 */

#include "Logger/Logger.h"

#include <cstddef>
#include <iostream>
#include <mutex>
#include <utility>

namespace MDSS
{
    namespace
    {
        struct TLoggerStorage
        {
            std::mutex            Mutex;
            std::vector<TLogEntry> Entries;
            std::uint64_t         NextSequence = 1;
            std::uint64_t         Revision = 0;
        };

        TLoggerStorage& GetStorage()
        {
            static TLoggerStorage Storage;
            return Storage;
        }

        constexpr std::size_t MaxRetainedEntries = 10000;
    } // namespace

    void TLogger::Verbose(std::string_view Module, std::string_view Message)
    {
        Write(TLogLevel::Verbose, Module, Message);
    }

    void TLogger::Debug(std::string_view Module, std::string_view Message)
    {
        Write(TLogLevel::Debug, Module, Message);
    }

    void TLogger::Info(std::string_view Module, std::string_view Message)
    {
        Write(TLogLevel::Info, Module, Message);
    }

    void TLogger::Warning(std::string_view Module, std::string_view Message)
    {
        Write(TLogLevel::Warning, Module, Message);
    }

    void TLogger::Error(std::string_view Module, std::string_view Message)
    {
        Write(TLogLevel::Error, Module, Message);
    }

    void TLogger::Write(TLogLevel Level, std::string_view Module, std::string_view Message)
    {
        const std::string ModuleText = Module.empty() ? "General" : std::string(Module);
        const std::string MessageText(Message);
        const std::string Formatted = "[" + std::string(GetLevelName(Level)) + "][" + ModuleText + "] " + MessageText;

        TLoggerStorage&   Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);

        // Keep terminal output and TDebugUI backed by the exact same formatted entry.
        std::ostream& Stream = (Level == TLogLevel::Warning || Level == TLogLevel::Error) ? std::cerr : std::cout;
        Stream << Formatted << '\n';
        Stream.flush();

        if (Storage.Entries.size() >= MaxRetainedEntries)
        {
            Storage.Entries.erase(Storage.Entries.begin(),
                                  Storage.Entries.begin() + static_cast<std::ptrdiff_t>(MaxRetainedEntries / 10));
        }

        Storage.Entries.push_back({Storage.NextSequence++, Level, ModuleText, MessageText, Formatted});
        ++Storage.Revision;
    }

    std::vector<TLogEntry> TLogger::GetEntries()
    {
        TLoggerStorage&   Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Entries;
    }

    std::uint64_t TLogger::GetRevision()
    {
        TLoggerStorage&   Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Revision;
    }

    void TLogger::Clear()
    {
        TLoggerStorage&   Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        Storage.Entries.clear();
        ++Storage.Revision;
    }

    const char* TLogger::GetLevelName(TLogLevel Level) noexcept
    {
        switch (Level)
        {
            case TLogLevel::Verbose:
                return "VERBOSE";
            case TLogLevel::Debug:
                return "DEBUG";
            case TLogLevel::Info:
                return "INFO";
            case TLogLevel::Warning:
                return "WARNING";
            case TLogLevel::Error:
                return "ERROR";
            case TLogLevel::Count:
                break;
        }
        return "UNKNOWN";
    }
} // namespace MDSS
