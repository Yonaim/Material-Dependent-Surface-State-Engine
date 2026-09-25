/**
 * @file SRProfileAsset.cpp
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#include "AssetManager/Assets/SRProfileAsset.h"

#include <utility>

namespace MDSS
{
    TSRProfileAsset::TSRProfileAsset(TAssetID                    ID,
                                   std::string                Name,
                                   std::filesystem::path      SourcePath,
                                   TSurfaceResponseProfileData Data)
        : TAsset(ID, std::move(Name), std::move(SourcePath)), Data(std::move(Data))
    {
        ValidateSurfaceResponseProfileData(this->Data);
    }

    const TSurfaceResponseProfileData& TSRProfileAsset::GetData() const noexcept
    {
        return Data;
    }
} // namespace MDSS
