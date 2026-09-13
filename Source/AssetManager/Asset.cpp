#include "AssetManager/Asset.h"

#include <utility>

namespace MDSS
{
    Asset::Asset(AssetID ID, std::string Name, std::filesystem::path SourcePath)
        : ID(ID), Name(std::move(Name)), SourcePath(std::move(SourcePath))
    {
    }

    AssetID Asset::GetID() const noexcept
    {
        return ID;
    }

    const std::string& Asset::GetName() const noexcept
    {
        return Name;
    }

    const std::filesystem::path& Asset::GetSourcePath() const noexcept
    {
        return SourcePath;
    }
} // namespace MDSS
