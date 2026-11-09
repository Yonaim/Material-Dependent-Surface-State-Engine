/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Scene/Transform.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include <filesystem>
#include <string>

namespace MDSS
{
    class FStaticMeshInstance
    {
    public:
        // Instance construction
        FStaticMeshInstance() = default;
        FStaticMeshInstance(Asset::FMeshAssetHandle Mesh, FTransform InstanceTransform = {});
        FStaticMeshInstance(Asset::FMeshAssetHandle                 Mesh,
                            SurfaceState::FSurfaceRuntimeDataHandle SurfaceData,
                            FTransform                              InstanceTransform,
                            std::filesystem::path                   MeshPath = {},
                            std::filesystem::path                   ProfileMapPath = {},
                            std::string                             ObjectId = {});

        // Instance properties and asset references
        [[nodiscard]] FTransform&                             GetTransform() noexcept;
        [[nodiscard]] const FTransform&                       GetTransform() const noexcept;
        [[nodiscard]] Asset::FMeshAssetHandle                 GetMesh() const noexcept;
        [[nodiscard]] SurfaceState::FSurfaceRuntimeDataHandle GetSurfaceData() const noexcept;
        void SetSurfaceData(SurfaceState::FSurfaceRuntimeDataHandle Handle) noexcept;
        [[nodiscard]] const std::filesystem::path& GetMeshPath() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetProfileMapPath() const noexcept;
        [[nodiscard]] const std::string&           GetId() const noexcept;

    private:
        Asset::FMeshAssetHandle                 Mesh = Asset::InvalidAssetHandle;
        SurfaceState::FSurfaceRuntimeDataHandle SurfaceData = SurfaceState::InvalidSurfaceRuntimeDataHandle;
        FTransform                              InstanceTransform;
        std::filesystem::path                   SourceMeshPath;
        std::filesystem::path                   SourceProfileMapPath;
        std::string                             Id;
    };
} // namespace MDSS
