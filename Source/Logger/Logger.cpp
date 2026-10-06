/**
 * @file Logger.cpp
 * @brief 모듈별 로그 기록과 로그 항목 조회.
 */

#include "Logger/Logger.h"

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <utility>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace MDSS
{
    namespace
    {
        struct TLoggerStorage
        {
            std::mutex             Mutex;
            std::vector<TLogEntry> Entries;
            std::uint64_t          NextSequence = 1;
            std::uint64_t          Revision = 0;
        };

        TLoggerStorage& GetStorage()
        {
            static TLoggerStorage Storage;
            return Storage;
        }

        constexpr std::size_t MaxRetainedEntries = 10000;

        bool IsTerminal(std::ostream& Stream)
        {
            if (&Stream == &std::cout)
            {
#if defined(_WIN32)
                return _isatty(_fileno(stdout)) != 0;
#else
                return isatty(fileno(stdout)) != 0;
#endif
            }
            if (&Stream == &std::cerr)
            {
#if defined(_WIN32)
                return _isatty(_fileno(stderr)) != 0;
#else
                return isatty(fileno(stderr)) != 0;
#endif
            }
            return false;
        }

        const char* GetAnsiColor(TLogLevel Level) noexcept
        {
            switch (Level)
            {
                case TLogLevel::Verbose:
                    return "\033[90m";
                case TLogLevel::Debug:
                    return "\033[36m";
                case TLogLevel::Info:
                    return "\033[32m";
                case TLogLevel::Warning:
                    return "\033[33m";
                case TLogLevel::Error:
                    return "\033[31m";
                case TLogLevel::Count:
                    break;
            }
            return "\033[0m";
        }
    } // namespace

#pragma region Log_Writing

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
        const std::string Formatted = "[" + std::string(GetLevelName(Level)) + "] [" + ModuleText + "] " + MessageText;

        TLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);

        // Keep terminal output and TDebugUI backed by the exact same formatted entry.
        std::ostream& Stream = (Level == TLogLevel::Warning || Level == TLogLevel::Error) ? std::cerr : std::cout;
        if (IsTerminal(Stream))
        {
            Stream << GetAnsiColor(Level) << Formatted << "\033[0m\n";
        }
        else
        {
            Stream << Formatted << '\n';
        }
        Stream.flush();

        if (Storage.Entries.size() >= MaxRetainedEntries)
        {
            Storage.Entries.erase(Storage.Entries.begin(),
                                  Storage.Entries.begin() + static_cast<std::ptrdiff_t>(MaxRetainedEntries / 10));
        }

        Storage.Entries.push_back({Storage.NextSequence++, Level, ModuleText, MessageText, Formatted});
        ++Storage.Revision;
    }

#pragma endregion

#pragma region Log_Entry_Access

    std::vector<TLogEntry> TLogger::GetEntries()
    {
        TLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Entries;
    }

    std::uint64_t TLogger::GetRevision()
    {
        TLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Revision;
    }

    void TLogger::Clear()
    {
        TLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        Storage.Entries.clear();
        ++Storage.Revision;
    }

#pragma endregion

#pragma region Log_Level_Names

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
#pragma endregion
} // namespace MDSS
