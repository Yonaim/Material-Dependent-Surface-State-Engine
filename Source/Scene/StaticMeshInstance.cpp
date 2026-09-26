/**
 * @file StaticMeshInstance.cpp
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
    TStaticMeshInstance::TStaticMeshInstance(TMeshAssetHandle Mesh, TTransform InstanceTransform)
        : TStaticMeshInstance(Mesh, InvalidSurfaceRuntimeDataHandle, std::move(InstanceTransform))
    {
    }

    TStaticMeshInstance::TStaticMeshInstance(TMeshAssetHandle Mesh,
                                             TSurfaceRuntimeDataHandle SurfaceData,
                                             TTransform InstanceTransform,
                                             std::filesystem::path MeshPath,
                                             std::filesystem::path ProfileMapPath)
        : Mesh(Mesh), SurfaceData(SurfaceData), InstanceTransform(std::move(InstanceTransform)),
          SourceMeshPath(std::move(MeshPath)), SourceProfileMapPath(std::move(ProfileMapPath))
    {
    }

    TTransform& TStaticMeshInstance::GetTransform() noexcept
    {
        return InstanceTransform;
    }

    const TTransform& TStaticMeshInstance::GetTransform() const noexcept
    {
        return InstanceTransform;
    }

    TMeshAssetHandle TStaticMeshInstance::GetMesh() const noexcept
    {
        return Mesh;
    }

    TSurfaceRuntimeDataHandle TStaticMeshInstance::GetSurfaceData() const noexcept
    {
        return SurfaceData;
    }

    const std::filesystem::path& TStaticMeshInstance::GetMeshPath() const noexcept
    {
        return SourceMeshPath;
    }

    const std::filesystem::path& TStaticMeshInstance::GetProfileMapPath() const noexcept
    {
        return SourceProfileMapPath;
    }
} // namespace MDSS
