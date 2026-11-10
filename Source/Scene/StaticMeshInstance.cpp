/**
 * @file StaticMeshInstance.cpp
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
// ============================================================
// 00. FStaticMeshInstance Implementation
// ============================================================
#pragma region 00_TStaticMeshInstance_Implementation
// 정적 메시 에셋 참조와 인스턴스 transform.

    FStaticMeshInstance::FStaticMeshInstance(Asset::FMeshAssetHandle Mesh, FTransform InstanceTransform)
        : FStaticMeshInstance(Mesh, SurfaceState::InvalidSurfaceRuntimeDataHandle, std::move(InstanceTransform))
    {
    }

    FStaticMeshInstance::FStaticMeshInstance(Asset::FMeshAssetHandle                 Mesh,
                                             SurfaceState::FSurfaceRuntimeDataHandle SurfaceData,
                                             FTransform                              InstanceTransform,
                                             std::filesystem::path                   MeshPath,
                                             std::filesystem::path                   ProfileMapPath,
                                             std::string                             ObjectId)
        : Mesh(Mesh), SurfaceData(SurfaceData), InstanceTransform(std::move(InstanceTransform)),
          SourceMeshPath(std::move(MeshPath)), SourceProfileMapPath(std::move(ProfileMapPath)), Id(std::move(ObjectId))
    {
    }

    FTransform& FStaticMeshInstance::GetTransform() noexcept
    {
        return InstanceTransform;
    }

    const FTransform& FStaticMeshInstance::GetTransform() const noexcept
    {
        return InstanceTransform;
    }

    Asset::FMeshAssetHandle FStaticMeshInstance::GetMesh() const noexcept
    {
        return Mesh;
    }

    SurfaceState::FSurfaceRuntimeDataHandle FStaticMeshInstance::GetSurfaceData() const noexcept
    {
        return SurfaceData;
    }

    void FStaticMeshInstance::SetSurfaceData(SurfaceState::FSurfaceRuntimeDataHandle Handle) noexcept
    {
        SurfaceData = Handle;
    }

    const std::filesystem::path& FStaticMeshInstance::GetMeshPath() const noexcept
    {
        return SourceMeshPath;
    }

    const std::filesystem::path& FStaticMeshInstance::GetProfileMapPath() const noexcept
    {
        return SourceProfileMapPath;
    }

    const std::string& FStaticMeshInstance::GetId() const noexcept
    {
        return Id;
    }
#pragma endregion
} // namespace MDSS
