/**
 * @file SceneLoader.h
 * @brief TScene JSON 파일 로더.
 */

#pragma once

#include "Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace MDSS::Asset
{
    class TAssetManager;
}
namespace MDSS::SurfaceState
{
    class TSurfaceDataManager;
}

namespace MDSS
{
    class TSceneLoader final
    {
    public:
        // Scene serialization
        /** @brief Scene과 그 Scene이 참조하는 Mesh/Profile Distribution을 로드한다. */
        [[nodiscard]] static TScene Load(const std::filesystem::path&       Path,
                                         Asset::TAssetManager&              Assets,
                                         SurfaceState::TSurfaceDataManager& SurfaceData,
                                         std::optional<std::uint32_t>        ResolutionOverride = std::nullopt);
        static void                 Save(const TScene& Scene, const std::filesystem::path& Path);
    };
} // namespace MDSS
