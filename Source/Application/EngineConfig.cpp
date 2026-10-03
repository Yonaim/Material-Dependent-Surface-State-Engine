/**
 * @file EngineConfig.cpp
 * @brief 실행 위치와 무관하게 설정 경로를 찾고 시작 Scene을 선택한다.
 */
#include "Application/EngineConfig.h"
#include "Logger/Logger.h"
#include <fstream>
#include <stdexcept>
#include <string>

#ifndef MDSS_ASSET_DIR
#define MDSS_ASSET_DIR "Assets"
#endif

namespace MDSS
{
    namespace
    {
        std::string Trim(const std::string& Value)
        {
            const auto Begin = Value.find_first_not_of(" \t\r\n");
            if (Begin == std::string::npos) return {};
            return Value.substr(Begin, Value.find_last_not_of(" \t\r\n") - Begin + 1);
        }
    }

    std::filesystem::path GetEngineConfigDirectory()
    {
        return std::filesystem::absolute(std::filesystem::path(MDSS_ASSET_DIR)).parent_path() / "Config";
    }

    std::filesystem::path LoadStartupScenePath(const std::filesystem::path& ConfigPath)
    {
        const auto AbsoluteConfig = std::filesystem::absolute(ConfigPath).lexically_normal();
        std::ifstream File(AbsoluteConfig);
        if (!File)
        {
            if (std::filesystem::exists(AbsoluteConfig))
                throw std::runtime_error("Cannot read engine config: " + AbsoluteConfig.string());
            TLogger::Warning("EngineConfig", "Config missing; using Assets/Scenes/Mountain.Scene.");
            return std::filesystem::absolute(std::filesystem::path(MDSS_ASSET_DIR) / "Scenes/Mountain.Scene").lexically_normal();
        }
        std::string Section, Line, SceneValue;
        bool Found = false;
        while (std::getline(File, Line))
        {
            // UTF-8 BOM과 Windows 줄바꿈도 허용한다.
            if (Line.compare(0, 3, "\xEF\xBB\xBF") == 0) Line.erase(0, 3);
            Line = Trim(Line);
            if (Line.empty() || Line.front() == ';' || Line.front() == '#') continue;
            if (Line.front() == '[' && Line.back() == ']')
            {
                Section = Trim(Line.substr(1, Line.size() - 2));
                continue;
            }
            const auto Equals = Line.find('=');
            if (Section != "Application" || Equals == std::string::npos ||
                Trim(Line.substr(0, Equals)) != "StartupScene") continue;
            if (Found) throw std::runtime_error("Duplicate [Application] StartupScene in " + AbsoluteConfig.string());
            Found = true;
            SceneValue = Trim(Line.substr(Equals + 1));
            if (SceneValue.size() >= 2 && SceneValue.front() == '"' && SceneValue.back() == '"')
                SceneValue = SceneValue.substr(1, SceneValue.size() - 2);
        }
        if (!Found || Trim(SceneValue).empty())
            throw std::runtime_error("Missing or empty [Application] StartupScene in " + AbsoluteConfig.string());
        std::filesystem::path ScenePath(SceneValue);
        if (ScenePath.is_relative()) ScenePath = AbsoluteConfig.parent_path() / ScenePath;
        return ScenePath.lexically_normal();
    }
}
