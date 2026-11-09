/**
 * @file Asset.h
 * @brief 에셋 공통 식별자와 원본 경로 메타데이터.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

namespace MDSS::Asset
{
    using FAssetID = std::uint32_t;
    using FMeshAssetHandle = std::uint32_t;
    using FMaterialAssetHandle = std::uint32_t;
    using FTextureAssetHandle = std::uint32_t;
    using FSRProfileAssetHandle = std::uint32_t;

    inline constexpr std::uint32_t InvalidAssetHandle = std::numeric_limits<std::uint32_t>::max();

    class FAsset
    {
    public:
        // Asset identity and source access
        FAsset(FAssetID ID, std::string Name, std::filesystem::path SourcePath = {});
        virtual ~FAsset() = default;

        [[nodiscard]] FAssetID                     GetID() const noexcept;
        [[nodiscard]] const std::string&           GetName() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;

    private:
        FAssetID              ID = InvalidAssetHandle;
        std::string           Name;
        std::filesystem::path SourcePath;
    };
} // namespace MDSS::Asset
