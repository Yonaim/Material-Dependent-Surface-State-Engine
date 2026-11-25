/**
 * @file EngineConfig.h
 * @brief 실행 위치와 무관한 설정 경로 및 시작 Scene 선택.
 */
#pragma once
#include <filesystem>

namespace MDSS
{
    [[nodiscard]] std::filesystem::path GetEngineConfigDirectory();

    // 비어 있으면 기본 장면을 연다. 상대 경로는 설정 파일 디렉터리를 기준으로 해석한다.
    [[nodiscard]] std::filesystem::path LoadStartupScenePath(const std::filesystem::path &ConfigPath);
} // namespace MDSS
