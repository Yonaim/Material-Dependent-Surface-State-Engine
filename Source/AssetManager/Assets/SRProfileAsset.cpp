/**
 * @file SRProfileAsset.cpp
 * @brief 검증된 Surface Response Profile 데이터의 에셋 표현.
 */

#include "AssetManager/Assets/SRProfileAsset.h"

#include <utility>

namespace MDSS::Asset
{
// ============================================================
// 00. FSRProfileAsset Implementation
// ============================================================
#pragma region 00_TSRProfileAsset_Implementation

    FSRProfileAsset::FSRProfileAsset(FAssetID                                  ID,
                                     std::string                               Name,
                                     std::filesystem::path                     SourcePath,
                                     SurfaceState::FSurfaceResponseProfileData Data)
        : FAsset(ID, std::move(Name), std::move(SourcePath)), Data(std::move(Data))
    {
        ValidateSurfaceResponseProfileData(this->Data);
    }

    const SurfaceState::FSurfaceResponseProfileData& FSRProfileAsset::GetData() const noexcept
    {
        return Data;
    }
#pragma endregion
} // namespace MDSS::Asset
