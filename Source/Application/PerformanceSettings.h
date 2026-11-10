/**
 * @file PerformanceSettings.h
 * @brief 연구·benchmark 설정 JSON을 환경 변수로 지정한 파일에서 읽는다.
 */

#pragma once

#include <cstdlib>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace MDSS
{
    // Read only by the explicit benchmark path. The runner preserves this file with each run.
    inline const nlohmann::json& PerformanceSettings()
    {
        static const auto Settings = []
        {
            const char* Path = std::getenv("MDSS_PERF_SETTINGS");

            if (!Path || !*Path)
                return nlohmann::json::object();
            std::ifstream Input(Path);

            if (!Input)
                throw std::runtime_error("Cannot read benchmark settings.");
            auto Result = nlohmann::json::parse(Input);

            if (!Result.is_object())
                throw std::runtime_error("Benchmark settings must be an object.");
            return Result;
        }();
        return Settings;
    }
}
