/**
 * @file YamlAssetLoader.h
 * @brief Surface Simulation에서 편집하는 YAML 에셋을 읽는다.
 */

#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace MDSS::Asset
{
    /** @brief 파일명과 일치하는 ID를 가진 버전 1 YAML 에셋을 읽는다. */
    nlohmann::json LoadVersionedYamlAsset(const std::filesystem::path &Path);
} // namespace MDSS::Asset
