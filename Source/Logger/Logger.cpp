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
        struct FLoggerStorage
        {
            std::mutex             Mutex;
            std::vector<FLogEntry> Entries;
            std::uint64_t          NextSequence = 1;
            std::uint64_t          Revision = 0;
        };

        FLoggerStorage& GetStorage()
        {
            static FLoggerStorage Storage;
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

        const char* GetAnsiColor(ELogLevel Level) noexcept
        {
            switch (Level)
            {
                case ELogLevel::Verbose:
                    return "\033[90m";
                case ELogLevel::Debug:
                    return "\033[36m";
                case ELogLevel::Info:
                    return "\033[32m";
                case ELogLevel::Warning:
                    return "\033[33m";
                case ELogLevel::Error:
                    return "\033[31m";
                case ELogLevel::Count:
                    break;
            }

            return "\033[0m";
        }
    } // namespace

// ============================================================
// 00. Log Writing
// ============================================================
#pragma region 00_Log_Writing
// level·module·메시지를 기록하고 관련 출력 경로로 전달한다.

    void FLogger::Verbose(std::string_view Module, std::string_view Message)
    {
        Write(ELogLevel::Verbose, Module, Message);
    }

    void FLogger::Debug(std::string_view Module, std::string_view Message)
    {
        Write(ELogLevel::Debug, Module, Message);
    }

    void FLogger::Info(std::string_view Module, std::string_view Message)
    {
        Write(ELogLevel::Info, Module, Message);
    }

    void FLogger::Warning(std::string_view Module, std::string_view Message)
    {
        Write(ELogLevel::Warning, Module, Message);
    }

    void FLogger::Error(std::string_view Module, std::string_view Message)
    {
        Write(ELogLevel::Error, Module, Message);
    }

    void FLogger::Write(ELogLevel Level, std::string_view Module, std::string_view Message)
    {
        const std::string ModuleText = Module.empty() ? "General" : std::string(Module);
        const std::string MessageText(Message);
        const std::string Formatted = "[" + std::string(GetLevelName(Level)) + "] [" + ModuleText + "] " + MessageText;

        FLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);

        // Keep terminal output and FDebugUI backed by the exact same formatted entry.
        std::ostream& Stream = (Level == ELogLevel::Warning || Level == ELogLevel::Error) ? std::cerr : std::cout;

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

// ============================================================
// 01. Log Entry Access
// ============================================================
#pragma region 01_Log_Entry_Access
// 현재 저장된 로그와 필터에 필요한 항목을 조회한다.

    std::vector<FLogEntry> FLogger::GetEntries()
    {
        FLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Entries;
    }

    std::uint64_t FLogger::GetRevision()
    {
        FLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        return Storage.Revision;
    }

    void FLogger::Clear()
    {
        FLoggerStorage&  Storage = GetStorage();
        std::scoped_lock Lock(Storage.Mutex);
        Storage.Entries.clear();
        ++Storage.Revision;
    }

#pragma endregion

// ============================================================
// 02. Log Level Names
// ============================================================
#pragma region 02_Log_Level_Names
// 로그 level을 UI와 텍스트 출력에서 사용할 이름으로 바꾼다.

    const char* FLogger::GetLevelName(ELogLevel Level) noexcept
    {
        switch (Level)
        {
            case ELogLevel::Verbose:
                return "VERBOSE";
            case ELogLevel::Debug:
                return "DEBUG";
            case ELogLevel::Info:
                return "INFO";
            case ELogLevel::Warning:
                return "WARNING";
            case ELogLevel::Error:
                return "ERROR";
            case ELogLevel::Count:
                break;
        }

        return "UNKNOWN";
    }
#pragma endregion
} // namespace MDSS
