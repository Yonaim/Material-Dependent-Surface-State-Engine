/**
 * @file StaticMeshInstance.cpp
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
#pragma region TStaticMeshInstance_Implementation

    TStaticMeshInstance::TStaticMeshInstance(Asset::TMeshAssetHandle Mesh, TTransform InstanceTransform)
        : TStaticMeshInstance(Mesh, SurfaceState::InvalidSurfaceRuntimeDataHandle, std::move(InstanceTransform))
    {
    }

    TStaticMeshInstance::TStaticMeshInstance(Asset::TMeshAssetHandle                 Mesh,
                                             SurfaceState::TSurfaceRuntimeDataHandle SurfaceData,
                                             TTransform                              InstanceTransform,
                                             std::filesystem::path                   MeshPath,
                                             std::filesystem::path                   ProfileMapPath,
                                             std::string                             ObjectId)
        : Mesh(Mesh), SurfaceData(SurfaceData), InstanceTransform(std::move(InstanceTransform)),
          SourceMeshPath(std::move(MeshPath)), SourceProfileMapPath(std::move(ProfileMapPath)), Id(std::move(ObjectId))
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

    Asset::TMeshAssetHandle TStaticMeshInstance::GetMesh() const noexcept
    {
        return Mesh;
    }

    SurfaceState::TSurfaceRuntimeDataHandle TStaticMeshInstance::GetSurfaceData() const noexcept
    {
        return SurfaceData;
    }

    void TStaticMeshInstance::SetSurfaceData(SurfaceState::TSurfaceRuntimeDataHandle Handle) noexcept
    {
        SurfaceData = Handle;
    }

    const std::filesystem::path& TStaticMeshInstance::GetMeshPath() const noexcept
    {
        return SourceMeshPath;
    }

    const std::filesystem::path& TStaticMeshInstance::GetProfileMapPath() const noexcept
    {
        return SourceProfileMapPath;
    }

    const std::string& TStaticMeshInstance::GetId() const noexcept
    {
        return Id;
    }
#pragma endregion
} // namespace MDSS
