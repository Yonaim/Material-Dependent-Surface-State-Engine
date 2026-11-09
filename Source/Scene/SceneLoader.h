/**
 * @file SceneLoader.h
 * @brief FScene JSON 파일 로더.
 */

#pragma once

#include "Scene/Scene.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace MDSS::Asset
{
    class FAssetManager;
}
namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
}

namespace MDSS
{
    class FSceneLoader final
    {
    public:
        // Scene serialization
        /** @brief Scene과 그 Scene이 참조하는 Mesh/Profile Distribution을 로드한다. */
        [[nodiscard]] static FScene Load(const std::filesystem::path&       Path,
                                         Asset::FAssetManager&              Assets,
                                         SurfaceState::FSurfaceDataManager& SurfaceData,
                                         std::optional<std::uint32_t>       ResolutionOverride = std::nullopt);
        static void                 Save(const FScene& Scene, const std::filesystem::path& Path);
    };
} // namespace MDSS
