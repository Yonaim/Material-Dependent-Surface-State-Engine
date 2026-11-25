/**
 * @file SceneAsset.h
 * @brief 저작 에셋에서 현재 박막 솔버에 연결할 장면 설정을 읽는다.
 */
#pragma once

#include "Application/Editor/SurfaceSimulation/SurfaceSceneBuilder.h"
#include <optional>
#include <string>
#include <vector>

namespace MDSS::SurfaceSimulation
{
    inline constexpr int MaxSceneComparisonCases = 4;

    struct FSceneComparison
    {
        std::filesystem::path              Path;
        std::string                        Title;
        std::string                        KoreanTitle;
        std::string                        Description;
        std::vector<std::filesystem::path> Scenes;
        std::vector<std::string>           Labels;
    };

    /** @brief 두 개에서 네 개의 원본 씬을 상대 경로로 참조하는 비교 씬을 읽는다. 단일 씬이면 빈 값을 반환한다. */
    [[nodiscard]] std::optional<FSceneComparison> ReadSceneComparison(const std::filesystem::path &Path);
    /** @brief 비교 조건이 같은 계열의 독립 실행인지 검사하고 설정을 반환한다. */
    [[nodiscard]] std::vector<FSurfaceSceneSettings> LoadComparisonCases(const FSceneComparison &Comparison);
    /** @brief 지원하는 .Scene과 프로파일의 참조·단위를 검증해 기존 솔버 입력을 구성한다. */
    [[nodiscard]] FSurfaceSceneSettings LoadSceneAsset(const std::filesystem::path &Path);
    /** @brief 장면 유형에 해당하는 기본 배포 에셋의 경로를 반환한다. */
    [[nodiscard]] std::filesystem::path BundledScenePath(EScene Scene);
    /** @brief 기본 씬과 같은 접두어를 가진 배포 변형을 파일명 순으로 찾는다. */
    [[nodiscard]] std::vector<std::filesystem::path> BundledSceneVariants(EScene Scene);
    /** @brief 공용 .Scene 열기 경로에서 Surface Simulation 장면 타입을 식별한다. */
    [[nodiscard]] bool IsSurfaceSceneAsset(const std::filesystem::path &Path);
} // namespace MDSS::SurfaceSimulation
