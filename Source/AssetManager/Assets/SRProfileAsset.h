/**
 * @file SRProfileAsset.h
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <filesystem>
#include <string>

namespace MDSS::Asset
{
    class FSRProfileAsset final : public FAsset
    {
    public:
        // Profile construction and data access
        /** @brief Profile 데이터를 보관하고 생성 시 전체 데이터 계약을 검증한다. */
        FSRProfileAsset(FAssetID                                  ID,
                        std::string                               Name,
                        std::filesystem::path                     SourcePath,
                        SurfaceState::FSurfaceResponseProfileData Data);

        [[nodiscard]] const SurfaceState::FSurfaceResponseProfileData& GetData() const noexcept;

    private:
        SurfaceState::FSurfaceResponseProfileData Data;
    };
} // namespace MDSS::Asset
