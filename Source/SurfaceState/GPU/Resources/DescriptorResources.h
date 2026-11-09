/**
 * @file DescriptorResources.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/Abi/SurfaceGPUDescriptorBinding.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
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
    class TSurfaceStateDescriptorResources final
    {
    public:
        // Descriptor creation and access
        TSurfaceStateDescriptorResources(VkDevice                                  Device,
                                         const TSurfaceSharedGeometryGPUResources& SharedGeometry,
                                         const TSurfaceProfileGPUResources&        Profiles,
                                         const TSurfaceInstanceGPUResources&       Instance);
        ~TSurfaceStateDescriptorResources();

        TSurfaceStateDescriptorResources(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources& operator=(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources(TSurfaceStateDescriptorResources&&) = delete;
        TSurfaceStateDescriptorResources& operator=(TSurfaceStateDescriptorResources&&) = delete;

        [[nodiscard]] bool HasReactions() const noexcept
        {
            return ProfileResources->GetMaxReactionCount() != 0;
        }
        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetABSet() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetBASet() const noexcept;
        [[nodiscard]] VkBuffer              GetBoundBufferHandle(TSurfaceGPUDescriptorBinding Binding, bool bAB) const;
        [[nodiscard]] std::array<std::uint64_t, 6> GetGeometryInputRevisions() const noexcept;
        [[nodiscard]] std::uint64_t                GetProfileParameterRevision() const noexcept
        {
            return ProfileResources->GetParametersBuffer().GetUploadRevision();
        }
        [[nodiscard]] const TSurfaceSharedGeometryGPUResources& GetSharedGeometry() const noexcept
        {
            return *SharedGeometryResources;
        }

    private:
        VkDevice                                  Device = VK_NULL_HANDLE;
        const TSurfaceSharedGeometryGPUResources* SharedGeometryResources = nullptr;
        const TSurfaceProfileGPUResources*        ProfileResources = nullptr;
        VkDescriptorSetLayout                     Layout = VK_NULL_HANDLE;
        VkDescriptorPool                          Pool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2>            Sets{VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<std::array<VkBuffer, static_cast<std::size_t>(TSurfaceGPUDescriptorBinding::Count)>, 2>
            BoundBufferHandles{};
    };

    /**
     * @brief Scene 전체 Profile table, Runtime별 Geometry, instance별 Solver buffer를 소유한다.
     */
} // namespace MDSS::SurfaceState
