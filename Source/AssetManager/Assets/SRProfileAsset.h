/**
 * @file SRProfileAsset.h
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <filesystem>
#include <string>

namespace MDSS
{
    class SRProfileAsset final : public Asset
    {
    public:
        /** @brief Profile 데이터를 보관하고 생성 시 전체 데이터 계약을 검증한다. */
        SRProfileAsset(AssetID ID, std::string Name, std::filesystem::path SourcePath, SurfaceResponseProfileData Data);

        [[nodiscard]] const SurfaceResponseProfileData& GetData() const noexcept;

    private:
        SurfaceResponseProfileData Data;
    };
} // namespace MDSS
