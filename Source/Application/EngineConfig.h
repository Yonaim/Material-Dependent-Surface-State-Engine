/**
 * @file EngineConfig.h
 * @brief 실행 위치와 무관한 설정 경로 및 시작 Scene 선택.
 */
#pragma once
#include <filesystem>
namespace MDSS
{
    [[nodiscard]] std::filesystem::path GetEngineConfigDirectory();
    // 상대 Scene 경로는 설정 파일이 있는 디렉터리를 기준으로 해석한다.
    [[nodiscard]] std::filesystem::path LoadStartupScenePath(const std::filesystem::path& ConfigPath);
}
