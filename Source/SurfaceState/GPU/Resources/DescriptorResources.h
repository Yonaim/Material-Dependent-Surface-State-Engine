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
    class FSurfaceStateDescriptorResources final
    {
    public:
        // Descriptor creation and access
        FSurfaceStateDescriptorResources(VkDevice                                  Device,
                                         const FSurfaceSharedGeometryGPUResources& SharedGeometry,
                                         const FSurfaceProfileGPUResources&        Profiles,
                                         const FSurfaceInstanceGPUResources&       Instance);
        ~FSurfaceStateDescriptorResources();

        FSurfaceStateDescriptorResources(const FSurfaceStateDescriptorResources&) = delete;
        FSurfaceStateDescriptorResources& operator=(const FSurfaceStateDescriptorResources&) = delete;
        FSurfaceStateDescriptorResources(FSurfaceStateDescriptorResources&&) = delete;
        FSurfaceStateDescriptorResources& operator=(FSurfaceStateDescriptorResources&&) = delete;

        [[nodiscard]] bool HasReactions() const noexcept
        {
            return ProfileResources->GetMaxReactionCount() != 0;
        }
        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetABSet() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetBASet() const noexcept;
        [[nodiscard]] VkBuffer              GetBoundBufferHandle(ESurfaceGPUDescriptorBinding Binding, bool bAB) const;
        [[nodiscard]] std::array<std::uint64_t, 6> GetGeometryInputRevisions() const noexcept;
        [[nodiscard]] std::uint64_t                GetProfileParameterRevision() const noexcept
        {
            return ProfileResources->GetParametersBuffer().GetUploadRevision();
        }
        [[nodiscard]] const FSurfaceSharedGeometryGPUResources& GetSharedGeometry() const noexcept
        {
            return *SharedGeometryResources;
        }

    private:
        VkDevice                                  Device = VK_NULL_HANDLE;
        const FSurfaceSharedGeometryGPUResources* SharedGeometryResources = nullptr;
        const FSurfaceProfileGPUResources*        ProfileResources = nullptr;
        VkDescriptorSetLayout                     Layout = VK_NULL_HANDLE;
        VkDescriptorPool                          Pool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2>            Sets{VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<std::array<VkBuffer, static_cast<std::size_t>(ESurfaceGPUDescriptorBinding::Count)>, 2>
            BoundBufferHandles{};
    };

    /**
     * @brief Scene 전체 Profile table, Runtime별 Geometry, instance별 Solver buffer를 소유한다.
     */
} // namespace MDSS::SurfaceState
