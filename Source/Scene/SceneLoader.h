/**
 * @file SceneLoader.h
 * @brief TScene JSON 파일 로더.
 */

#pragma once

#include "Scene/Scene.h"

#include <filesystem>

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
        /** @brief Scene과 그 Scene이 참조하는 Mesh/Profile Distribution을 로드한다. */
        [[nodiscard]] static TScene Load(const std::filesystem::path&       Path,
                                         Asset::TAssetManager&              Assets,
                                         SurfaceState::TSurfaceDataManager& SurfaceData);
        static void                 Save(const TScene& Scene, const std::filesystem::path& Path);
    };
} // namespace MDSS
