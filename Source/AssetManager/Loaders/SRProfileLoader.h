/**
 * @file SRProfileLoader.h
 * @brief SRProfile JSON 파싱과 데이터 검증.
 */

#pragma once

#include "AssetManager/Core/Asset.h"

#include <filesystem>
#include <memory>

namespace MDSS
{
    class TSRProfileAsset;

    class TSRProfileLoader
    {
    public:
        /**
         * @brief version 1 `.SRProfile` JSON 파일을 읽고 검증된 TAsset을 생성한다.
         * @param ID TAssetManager가 부여한 고유 에셋 ID.
         * @param Path 읽을 Profile 파일 경로.
         * @throws std::runtime_error 파일을 읽거나 파싱·검증하지 못한 경우.
         */
        [[nodiscard]] static std::unique_ptr<TSRProfileAsset> Load(TAssetID ID, const std::filesystem::path& Path);
    };
} // namespace MDSS
