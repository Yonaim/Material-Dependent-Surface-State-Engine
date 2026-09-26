/**
 * @file Asset.h
 * @brief 에셋 공통 식별자와 원본 경로 메타데이터.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>

namespace MDSS
{
    using TAssetID = std::uint32_t;
    using TMeshAssetHandle = std::uint32_t;
    using TSurfaceRuntimeDataHandle = std::uint32_t;
    using TMaterialAssetHandle = std::uint32_t;
    using TextureAssetHandle = std::uint32_t;
    using TSRProfileAssetHandle = std::uint32_t;

    inline constexpr std::uint32_t InvalidAssetHandle = std::numeric_limits<std::uint32_t>::max();
    inline constexpr TSurfaceRuntimeDataHandle InvalidSurfaceRuntimeDataHandle =
        std::numeric_limits<TSurfaceRuntimeDataHandle>::max();

    class TAsset
    {
    public:
        TAsset(TAssetID ID, std::string Name, std::filesystem::path SourcePath = {});
        virtual ~TAsset() = default;

        [[nodiscard]] TAssetID                      GetID() const noexcept;
        [[nodiscard]] const std::string&           GetName() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetSourcePath() const noexcept;

    private:
        TAssetID               ID = InvalidAssetHandle;
        std::string           Name;
        std::filesystem::path SourcePath;
    };
} // namespace MDSS
