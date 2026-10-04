/**
 * @file SRProfileAsset.cpp
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#include "AssetManager/Assets/SRProfileAsset.h"

#include <utility>

namespace MDSS::Asset
{
    TSRProfileAsset::TSRProfileAsset(TAssetID                                  ID,
                                     std::string                               Name,
                                     std::filesystem::path                     SourcePath,
                                     SurfaceState::TSurfaceResponseProfileData Data)
        : TAsset(ID, std::move(Name), std::move(SourcePath)), Data(std::move(Data))
    {
        ValidateSurfaceResponseProfileData(this->Data);
    }

    const SurfaceState::TSurfaceResponseProfileData& TSRProfileAsset::GetData() const noexcept
    {
        return Data;
    }
} // namespace MDSS::Asset
