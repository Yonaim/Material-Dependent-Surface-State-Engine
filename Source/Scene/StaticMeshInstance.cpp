/**
 * @file StaticMeshInstance.cpp
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#include "Scene/StaticMeshInstance.h"

#include <utility>

namespace MDSS
{
    TStaticMeshInstance::TStaticMeshInstance(TMeshAssetHandle Mesh, TTransform InstanceTransform)
        : Mesh(Mesh), InstanceTransform(std::move(InstanceTransform))
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
} // namespace MDSS
