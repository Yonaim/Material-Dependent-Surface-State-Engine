/**
 * @file Asset.cpp
 * @brief 에셋 공통 식별자와 원본 경로 메타데이터.
 */

#include "AssetManager/Core/Asset.h"

#include <utility>

namespace MDSS
{
    TAsset::TAsset(TAssetID ID, std::string Name, std::filesystem::path SourcePath)
        : ID(ID), Name(std::move(Name)), SourcePath(std::move(SourcePath))
    {
    }

    TAssetID TAsset::GetID() const noexcept
    {
        return ID;
    }

    const std::string& TAsset::GetName() const noexcept
    {
        return Name;
    }

    const std::filesystem::path& TAsset::GetSourcePath() const noexcept
    {
        return SourcePath;
    }
} // namespace MDSS
