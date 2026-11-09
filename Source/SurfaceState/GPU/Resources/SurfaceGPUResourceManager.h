/**
 * @file SurfaceGPUResourceManager.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/GPU/Resources/DescriptorResources.h"
#include "SurfaceState/GPU/Resources/InstanceResources.h"
#include "SurfaceState/GPU/Resources/ProfileResources.h"
#include "SurfaceState/GPU/Resources/SharedGeometryResources.h"
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace MDSS::Asset
{
    class TAssetManager;
}
namespace MDSS::SurfaceState
{
    class TSurfaceDataManager;
    class TSurfaceGPUBufferPool;
}
namespace MDSS::GPU
{
    class TVulkanContext;
}

namespace MDSS
{
    struct TTransform;
    class TScene;
}

namespace MDSS::SurfaceState
{
    class TSurfaceGPUResourceManager final
    {
    public:
        // Resource lifecycle
        TSurfaceGPUResourceManager(const GPU::TVulkanContext&   Context,
                                   const Asset::TAssetManager&  Assets,
                                   const TSurfaceDataManager&   SurfaceData,
                                   const TScene&                Scene,
                                   TSurfaceGPUBufferPool*       Pool = nullptr,
                                   TSurfaceScratchStorageFormat Storage = {});
        ~TSurfaceGPUResourceManager();

        void RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept;
        void TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay);
        bool ConfigureScratchStorage(
            const GPU::TVulkanContext& Context, bool Cache, bool HalfRaw, bool HalfWeights, bool Compact);

        TSurfaceGPUResourceManager(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager& operator=(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager(TSurfaceGPUResourceManager&&) = delete;
        TSurfaceGPUResourceManager& operator=(TSurfaceGPUResourceManager&&) = delete;

        // Scene and instance resource access
        [[nodiscard]] std::size_t GetManagedInstanceCount() const noexcept;
        [[nodiscard]] std::size_t GetSharedSurfaceDataCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneProfileCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneInstanceCount() const noexcept;
        /** @brief Scene 벡터 인덱스에 해당하는 instance 디스크립터 리소스를 반환한다. */
        [[nodiscard]] const TSurfaceStateDescriptorResources*   GetInstanceDescriptors(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceSharedGeometryGPUResources* GetInstanceSharedGeometry(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceStateDescriptorResources*   GetAnyInstanceDescriptors() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceAccumulationHeightBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceReactionScratchBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetSceneProfileParametersBuffer() const;
        [[nodiscard]] bool                   UpdateProfileParameters(Asset::TSRProfileAssetHandle   ProfileHandle,
                                                                     TStateId                       State,
                                                                     const TSurfaceStateParameters& Parameters);

        // State and transfer-weight cache management
        [[nodiscard]] std::size_t GetInstanceTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceValidTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceChannelCount(std::size_t SceneIndex) const;
        [[nodiscard]] bool        IsCurrentStateAB(std::size_t SceneIndex) const;
        void                      ResetStates();
        [[nodiscard]] bool NeedsTransferWeightCacheUpdate(std::size_t SceneIndex, const TTransform& Transform) const;
        void               UpdateTransferWeightCache(std::size_t       SceneIndex,
                                                     const TTransform& Transform,
                                                     bool              bUseNormalWeight = true,
                                                     bool              bUseDistanceWeight = true,
                                                     bool              bUseProfileBoundaryWeight = true,
                                                     bool              bHalfPrecision = false);
        void               InvalidateTransferWeightCache(std::size_t SceneIndex);
        void               AdvanceCurrentState(std::size_t SceneIndex);

    private:
        std::optional<std::array<bool, 4>> ScratchStorageFormat;
        struct TSharedSurfaceResources
        {
            std::unique_ptr<TSurfaceSharedGeometryGPUResources> Geometry;
            std::vector<TSurfaceProfileIndex>                   SceneProfileIndices;
            const TSharedSurfaceGeometryData*                   CPUGeometry = nullptr;
        };

        struct TInstanceResources
        {
            std::unique_ptr<TSurfaceInstanceGPUResources> State;
            // 디스크립터가 참조하는 버퍼보다 디스크립터 세트와 레이아웃을 먼저 파괴한다.
            std::unique_ptr<TSurfaceStateDescriptorResources> Descriptors;
            TSurfaceRuntimeDataHandle                         SurfaceDataHandle{};
            std::size_t                                       ValidTexelCount = 0;
            glm::vec3                                         TransferWeightScale{1.0F};
            bool                                              bTransferWeightCacheValid = false;
            bool                                              bCurrentStateAB = true;
        };

        // Reverse destruction order: descriptors → Geometry → Scene Profile buffers.
        std::vector<Asset::TSRProfileAssetHandle>                              SceneProfileHandles;
        std::unique_ptr<TSurfaceProfileGPUResources>                           SceneProfiles;
        std::unordered_map<TSurfaceRuntimeDataHandle, TSharedSurfaceResources> SharedSurfaceData;
        std::vector<std::unique_ptr<TInstanceResources>>                       InstanceResources;
    };
} // namespace MDSS::SurfaceState
