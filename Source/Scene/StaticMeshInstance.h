/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Scene/Transform.h"
#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"

#include <filesystem>
#include <string>

namespace MDSS
{
    class TStaticMeshInstance
    {
    public:
        // Instance construction
        TStaticMeshInstance() = default;
        TStaticMeshInstance(Asset::TMeshAssetHandle Mesh, TTransform InstanceTransform = {});
        TStaticMeshInstance(Asset::TMeshAssetHandle                 Mesh,
                            SurfaceState::TSurfaceRuntimeDataHandle SurfaceData,
                            TTransform                              InstanceTransform,
                            std::filesystem::path                   MeshPath = {},
                            std::filesystem::path                   ProfileMapPath = {},
                            std::string                             ObjectId = {});

        // Instance properties and asset references
        [[nodiscard]] TTransform&                             GetTransform() noexcept;
        [[nodiscard]] const TTransform&                       GetTransform() const noexcept;
        [[nodiscard]] Asset::TMeshAssetHandle                 GetMesh() const noexcept;
        [[nodiscard]] SurfaceState::TSurfaceRuntimeDataHandle GetSurfaceData() const noexcept;
        void SetSurfaceData(SurfaceState::TSurfaceRuntimeDataHandle Handle) noexcept;
        [[nodiscard]] const std::filesystem::path& GetMeshPath() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetProfileMapPath() const noexcept;
        [[nodiscard]] const std::string&           GetId() const noexcept;

    private:
        Asset::TMeshAssetHandle                 Mesh = Asset::InvalidAssetHandle;
        SurfaceState::TSurfaceRuntimeDataHandle SurfaceData = SurfaceState::InvalidSurfaceRuntimeDataHandle;
        TTransform                              InstanceTransform;
        std::filesystem::path                   SourceMeshPath;
        std::filesystem::path                   SourceProfileMapPath;
        std::string                             Id;
    };
} // namespace MDSS
