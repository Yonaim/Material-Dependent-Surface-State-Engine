/**
 * @file Asset.cpp
 * @brief 에셋 공통 식별자와 원본 경로 메타데이터.
 */

#include "AssetManager/Core/Asset.h"

#include <utility>

namespace MDSS::Asset
{
// ============================================================
// 00. FAsset Implementation
// ============================================================
#pragma region 00_TAsset_Implementation

    FAsset::FAsset(FAssetID ID, std::string Name, std::filesystem::path SourcePath)
        : ID(ID), Name(std::move(Name)), SourcePath(std::move(SourcePath))
    {
    }

    FAssetID FAsset::GetID() const noexcept
    {
        return ID;
    }

    const std::string& FAsset::GetName() const noexcept
    {
        return Name;
    }

    const std::filesystem::path& FAsset::GetSourcePath() const noexcept
    {
        return SourcePath;
    }
#pragma endregion
} // namespace MDSS::Asset
