#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

namespace MDSS
{
    using AssetID = std::uint32_t;
    using MeshAssetHandle = std::uint32_t;
    using MaterialAssetHandle = std::uint32_t;
    using TextureAssetHandle = std::uint32_t;
    using SRProfileAssetHandle = std::uint32_t;

    inline constexpr std::uint32_t InvalidAssetHandle = std::numeric_limits<std::uint32_t>::max();

    class Asset
    {
    public:
        Asset(AssetID ID, std::string Name, std::filesystem::path SourcePath = {});
        virtual ~Asset() = default;

        [[nodiscard]] AssetID                      GetID() const noexcept;
        [[nodiscard]] const std::string&           GetName() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;

    private:
        AssetID               ID = InvalidAssetHandle;
        std::string           Name;
        std::filesystem::path SourcePath;
    };
} // namespace MDSS
