/**
 * @file GeometryExperiments.h
 * @brief 동일한 표면을 여러 형상 입력으로 불러오는 비교 검사의 진입점을 정의한다.
 */

#pragma once

#include <filesystem>

namespace MDSS::SurfaceSimulation
{
    /** @brief 형상 입력 간 높이 오차와 대역 분리를 검사하고 결과를 Output에 기록한다. */
    void CheckGeometryImports(const std::filesystem::path &FixtureDirectory, const std::filesystem::path &Output);
} // namespace MDSS::SurfaceSimulation
