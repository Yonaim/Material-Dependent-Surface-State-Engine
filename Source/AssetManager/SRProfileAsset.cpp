/**
 * @file SRProfileAsset.cpp
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#include "AssetManager/SRProfileAsset.h"

#include <utility>

namespace MDSS
{
    SRProfileAsset::SRProfileAsset(AssetID                    ID,
                                   std::string                Name,
                                   std::filesystem::path      SourcePath,
                                   SurfaceResponseProfileData Data)
        : Asset(ID, std::move(Name), std::move(SourcePath)), Data(std::move(Data))
    {
        ValidateSurfaceResponseProfileData(this->Data);
    }

    const SurfaceResponseProfileData& SRProfileAsset::GetData() const noexcept
    {
        return Data;
    }
} // namespace MDSS
