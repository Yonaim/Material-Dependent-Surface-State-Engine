/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Scene/Transform.h"

#include <filesystem>
#include <string>

namespace MDSS
{
    class TStaticMeshInstance
    {
    public:
        TStaticMeshInstance() = default;
        TStaticMeshInstance(Asset::TMeshAssetHandle Mesh, TTransform InstanceTransform = {});
        TStaticMeshInstance(Asset::TMeshAssetHandle          Mesh,
                            Asset::TSurfaceRuntimeDataHandle SurfaceData,
                            TTransform                InstanceTransform,
                            std::filesystem::path     MeshPath = {},
                            std::filesystem::path     ProfileMapPath = {},
                            std::string               ObjectId = {});

        [[nodiscard]] TTransform&                  GetTransform() noexcept;
        [[nodiscard]] const TTransform&            GetTransform() const noexcept;
        [[nodiscard]] Asset::TMeshAssetHandle             GetMesh() const noexcept;
        [[nodiscard]] Asset::TSurfaceRuntimeDataHandle    GetSurfaceData() const noexcept;
        void                                       SetSurfaceData(Asset::TSurfaceRuntimeDataHandle Handle) noexcept;
        [[nodiscard]] const std::filesystem::path& GetMeshPath() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetProfileMapPath() const noexcept;
        [[nodiscard]] const std::string&           GetId() const noexcept;

    private:
        Asset::TMeshAssetHandle          Mesh = Asset::InvalidAssetHandle;
        Asset::TSurfaceRuntimeDataHandle SurfaceData = Asset::InvalidSurfaceRuntimeDataHandle;
        TTransform                InstanceTransform;
        std::filesystem::path     SourceMeshPath;
        std::filesystem::path     SourceProfileMapPath;
        std::string               Id;
    };
} // namespace MDSS
