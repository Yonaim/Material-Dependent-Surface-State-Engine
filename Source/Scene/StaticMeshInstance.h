/**
 * @file StaticMeshInstance.h
 * @brief 정적 메시 에셋 참조와 인스턴스 transform.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "Scene/Transform.h"

namespace MDSS
{
    class TStaticMeshInstance
    {
    public:
        TStaticMeshInstance() = default;
        TStaticMeshInstance(TMeshAssetHandle Mesh, TTransform InstanceTransform = {});
        TStaticMeshInstance(TMeshAssetHandle Mesh,
                            TSurfaceRuntimeDataHandle SurfaceData,
                            TTransform InstanceTransform);

        [[nodiscard]] TTransform&                GetTransform() noexcept;
        [[nodiscard]] const TTransform&          GetTransform() const noexcept;
        [[nodiscard]] TMeshAssetHandle           GetMesh() const noexcept;
        [[nodiscard]] TSurfaceRuntimeDataHandle GetSurfaceData() const noexcept;

    private:
        TMeshAssetHandle Mesh = InvalidAssetHandle;
        TSurfaceRuntimeDataHandle SurfaceData = InvalidSurfaceRuntimeDataHandle;
        TTransform       InstanceTransform;
    };
} // namespace MDSS
