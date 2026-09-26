/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Scene/Transform.h"

#include <filesystem>

namespace MDSS
{
    class TStaticMeshInstance
    {
    public:
        TStaticMeshInstance() = default;
        TStaticMeshInstance(TMeshAssetHandle Mesh, TTransform InstanceTransform = {});
        TStaticMeshInstance(TMeshAssetHandle Mesh,
                            TSurfaceRuntimeDataHandle SurfaceData,
                            TTransform InstanceTransform,
                            std::filesystem::path MeshPath = {},
                            std::filesystem::path ProfileMapPath = {});

        [[nodiscard]] TTransform&                GetTransform() noexcept;
        [[nodiscard]] const TTransform&          GetTransform() const noexcept;
        [[nodiscard]] TMeshAssetHandle           GetMesh() const noexcept;
        [[nodiscard]] TSurfaceRuntimeDataHandle GetSurfaceData() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetMeshPath() const noexcept;
        [[nodiscard]] const std::filesystem::path& GetProfileMapPath() const noexcept;

    private:
        TMeshAssetHandle Mesh = InvalidAssetHandle;
        TSurfaceRuntimeDataHandle SurfaceData = InvalidSurfaceRuntimeDataHandle;
        TTransform       InstanceTransform;
        std::filesystem::path SourceMeshPath;
        std::filesystem::path SourceProfileMapPath;
    };
} // namespace MDSS
