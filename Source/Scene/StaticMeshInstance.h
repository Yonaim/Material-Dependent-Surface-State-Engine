/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Asset.h"
#include "Scene/Transform.h"

namespace MDSS
{
    class StaticMeshInstance
    {
    public:
        StaticMeshInstance() = default;
        StaticMeshInstance(MeshAssetHandle Mesh, Transform InstanceTransform = {});

        [[nodiscard]] Transform&       GetTransform() noexcept;
        [[nodiscard]] const Transform& GetTransform() const noexcept;
        [[nodiscard]] MeshAssetHandle  GetMesh() const noexcept;

    private:
        MeshAssetHandle Mesh = InvalidAssetHandle;
        Transform       InstanceTransform;
    };
} // namespace MDSS
