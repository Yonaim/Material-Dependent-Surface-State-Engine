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
    class FAssetManager;
}

namespace MDSS::SurfaceState
{
    class FSurfaceDataManager;
    class FSurfaceGPUBufferPool;
}

namespace MDSS::GPU
{
    class FVulkanContext;
}

namespace MDSS
{
    struct FTransform;
    class FScene;
}

namespace MDSS::SurfaceState
{
    class FSurfaceGPUResourceManager final
    {
    public:
        // Resource lifecycle
        FSurfaceGPUResourceManager(const GPU::FVulkanContext&   Context,
                                   const Asset::FAssetManager&  Assets,
                                   const FSurfaceDataManager&   SurfaceData,
                                   const FScene&                Scene,
                                   FSurfaceGPUBufferPool*       Pool = nullptr,
                                   FSurfaceScratchStorageFormat Storage = {});
        ~FSurfaceGPUResourceManager();

        void RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept;
        void TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay);
        bool ConfigureScratchStorage(
            const GPU::FVulkanContext& Context, bool bCache, bool bHalfRaw, bool bHalfWeights, bool bCompact);

        FSurfaceGPUResourceManager(const FSurfaceGPUResourceManager&) = delete;
        FSurfaceGPUResourceManager& operator=(const FSurfaceGPUResourceManager&) = delete;
        FSurfaceGPUResourceManager(FSurfaceGPUResourceManager&&) = delete;
        FSurfaceGPUResourceManager& operator=(FSurfaceGPUResourceManager&&) = delete;

        // Scene and instance resource access
        [[nodiscard]] std::size_t GetManagedInstanceCount() const noexcept;
        [[nodiscard]] std::size_t GetSharedSurfaceDataCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneProfileCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneInstanceCount() const noexcept;
        /** @brief Scene 벡터 인덱스에 해당하는 instance 디스크립터 리소스를 반환한다. */
        [[nodiscard]] const FSurfaceStateDescriptorResources*   GetInstanceDescriptors(std::size_t SceneIndex) const;
        [[nodiscard]] const FSurfaceSharedGeometryGPUResources* GetInstanceSharedGeometry(std::size_t SceneIndex) const;
        [[nodiscard]] const FSurfaceStateDescriptorResources*   GetAnyInstanceDescriptors() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceAccumulationHeightBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const;
        /** @brief instance의 월드 texel 면적 버퍼를 빌린다. 자원 재구성 전까지만 참조가 유효하다. */
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceWorldTexelAreaBuffer(std::size_t SceneIndex) const;
        /** @brief instance의 동적 Geometry 버퍼를 빌린다. GPU 완료 후 readback해야 한다. */
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceDynamicGeometryBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetInstanceReactionScratchBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::FGPUBuffer& GetSceneProfileParametersBuffer() const;
        [[nodiscard]] bool                   UpdateProfileParameters(Asset::FSRProfileAssetHandle   ProfileHandle,
                                                                     FStateId                       State,
                                                                     const FSurfaceStateParameters& Parameters);

        // State and transfer-weight cache management
        [[nodiscard]] std::size_t GetInstanceTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceValidTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceChannelCount(std::size_t SceneIndex) const;
        [[nodiscard]] bool        IsCurrentStateAB(std::size_t SceneIndex) const;
        void                      ResetStates();
        [[nodiscard]] bool NeedsTransferWeightCacheUpdate(std::size_t SceneIndex, const FTransform& Transform) const;
        void               UpdateTransferWeightCache(std::size_t       SceneIndex,
                                                     const FTransform& Transform,
                                                     bool              bUseNormalWeight = true,
                                                     bool              bUseDistanceWeight = true,
                                                     bool              bUseProfileBoundaryWeight = true,
                                                     bool              bHalfPrecision = false);
        void               InvalidateTransferWeightCache(std::size_t SceneIndex);
        void               AdvanceCurrentState(std::size_t SceneIndex);

    private:
        std::optional<std::array<bool, 4>> ScratchStorageFormat;
        struct FSharedSurfaceResources
        {
            std::unique_ptr<FSurfaceSharedGeometryGPUResources> Geometry;
            std::vector<FSurfaceProfileIndex>                   SceneProfileIndices;
            const FSharedSurfaceGeometryData*                   CPUGeometry = nullptr;
        };

        struct FInstanceResources
        {
            std::unique_ptr<FSurfaceInstanceGPUResources> State;

            // 디스크립터가 참조하는 버퍼보다 디스크립터 세트와 레이아웃을 먼저 파괴한다.
            std::unique_ptr<FSurfaceStateDescriptorResources> Descriptors;
            FSurfaceRuntimeDataHandle                         SurfaceDataHandle{};
            std::size_t                                       ValidTexelCount = 0;
            glm::vec3                                         TransferWeightScale{1.0F};
            bool                                              bTransferWeightCacheValid = false;
            bool                                              bCurrentStateAB = true;
        };

        // Reverse destruction order: descriptors → Geometry → Scene Profile buffers.
        std::vector<Asset::FSRProfileAssetHandle>                              SceneProfileHandles;
        std::unique_ptr<FSurfaceProfileGPUResources>                           SceneProfiles;
        std::unordered_map<FSurfaceRuntimeDataHandle, FSharedSurfaceResources> SharedSurfaceData;
        std::vector<std::unique_ptr<FInstanceResources>>                       InstanceResources;
    };
} // namespace MDSS::SurfaceState
