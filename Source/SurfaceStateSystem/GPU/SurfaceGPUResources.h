/**
 * @file SurfaceGPUResources.h
 * @brief Vulkan storage buffers and descriptor sets for Surface simulation data.
 */

#pragma once

#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "AssetManager/Core/Asset.h"
#include "VulkanContext/GPU/GPUBuffer.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace MDSS
{
    enum class TSurfaceGPUDescriptorBinding : std::uint32_t
    {
        TexelSurfaceIndices = 0,
        TexelProfileIndices,
        Positions,
        Normals,
        GeometryScalars,
        NeighborIndices,
        ProfileParameters,
        ProfileSupported,
        CurrentState,
        NextState,
        OutgoingFluxScale,
        InputDelta,
        SurfaceRanges,
        TexelChartIndices,
        Count
    };

    class TAssetManager;
    class TScene;
    class TVulkanContext;

    class TSurfaceSharedGeometryGPUResources final
    {
    public:
        TSurfaceSharedGeometryGPUResources(VkPhysicalDevice PhysicalDevice,
                                           VkDevice         Device,
                                           const TSharedSurfaceGeometryData& Geometry);

        [[nodiscard]] const TGPUBuffer& GetTexelSurfaceIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTexelProfileIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetPositionBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetNormalBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetGeometryScalarBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetNeighborIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetSurfaceRangeBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTexelChartIndexBuffer() const noexcept;
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;

    private:
        std::size_t TexelCount = 0;
        std::unique_ptr<TGPUBuffer> TexelSurfaceIndexBuffer;
        std::unique_ptr<TGPUBuffer> TexelProfileIndexBuffer;
        std::unique_ptr<TGPUBuffer> PositionBuffer;
        std::unique_ptr<TGPUBuffer> NormalBuffer;
        std::unique_ptr<TGPUBuffer> GeometryScalarBuffer;
        std::unique_ptr<TGPUBuffer> NeighborIndexBuffer;
        std::unique_ptr<TGPUBuffer> SurfaceRangeBuffer;
        std::unique_ptr<TGPUBuffer> TexelChartIndexBuffer;
    };

    class TSurfaceProfileGPUResources final
    {
    public:
        TSurfaceProfileGPUResources(VkPhysicalDevice PhysicalDevice,
                                    VkDevice         Device,
                                    const std::vector<TSurfaceResponseProfileData>& Profiles,
                                    const TSurfaceStateRegistry& Registry);

        [[nodiscard]] const TGPUBuffer& GetParametersBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetSupportedBuffer() const noexcept;
        [[nodiscard]] std::size_t GetProfileCount() const noexcept;
        [[nodiscard]] std::size_t GetChannelCount() const noexcept;

    private:
        std::size_t ProfileCount = 0;
        std::size_t ChannelCount = 0;
        std::unique_ptr<TGPUBuffer> ParametersBuffer;
        std::unique_ptr<TGPUBuffer> SupportedBuffer;
    };

    class TSurfaceInstanceGPUResources final
    {
    public:
        TSurfaceInstanceGPUResources(VkPhysicalDevice PhysicalDevice,
                                     VkDevice         Device,
                                     std::size_t      TexelCount,
                                     std::size_t      ChannelCount);

        [[nodiscard]] const TGPUBuffer& GetStateABuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetStateBBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetOutgoingFluxScaleBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetInputDeltaBuffer() const noexcept;
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;
        [[nodiscard]] std::size_t GetChannelCount() const noexcept;

    private:
        std::size_t TexelCount = 0;
        std::size_t ChannelCount = 0;
        std::size_t ScalarCount = 0;
        std::unique_ptr<TGPUBuffer> StateABuffer;
        std::unique_ptr<TGPUBuffer> StateBBuffer;
        std::unique_ptr<TGPUBuffer> OutgoingFluxScaleBuffer;
        std::unique_ptr<TGPUBuffer> InputDeltaBuffer;
    };

    class TSurfaceStateDescriptorResources final
    {
    public:
        TSurfaceStateDescriptorResources(VkDevice                                 Device,
                                         const TSurfaceSharedGeometryGPUResources& SharedGeometry,
                                         const TSurfaceProfileGPUResources&        Profiles,
                                         const TSurfaceInstanceGPUResources&       Instance);
        ~TSurfaceStateDescriptorResources();

        TSurfaceStateDescriptorResources(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources& operator=(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources(TSurfaceStateDescriptorResources&&) = delete;
        TSurfaceStateDescriptorResources& operator=(TSurfaceStateDescriptorResources&&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept;
        [[nodiscard]] VkDescriptorSet GetABSet() const noexcept;
        [[nodiscard]] VkDescriptorSet GetBASet() const noexcept;
        [[nodiscard]] VkBuffer GetBoundBufferHandle(TSurfaceGPUDescriptorBinding Binding, bool bAB) const;

    private:
        VkDevice Device = VK_NULL_HANDLE;
        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
        VkDescriptorPool Pool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2> Sets{VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<std::array<VkBuffer, static_cast<std::size_t>(TSurfaceGPUDescriptorBinding::Count)>, 2>
            BoundBufferHandles{};
    };

    /**
     * @brief Scene의 Runtime Surface Data 조합별 공유 buffer와 instance별 solver buffer를 소유한다.
     * @note Compute dispatch는 수행하지 않으며 Branch 4 GPU resource 수명만 관리한다.
     */
    class TSurfaceGPUResourceManager final
    {
    public:
        TSurfaceGPUResourceManager(const TVulkanContext& Context, const TAssetManager& Assets, const TScene& Scene);
        ~TSurfaceGPUResourceManager();

        TSurfaceGPUResourceManager(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager& operator=(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager(TSurfaceGPUResourceManager&&) = delete;
        TSurfaceGPUResourceManager& operator=(TSurfaceGPUResourceManager&&) = delete;

        [[nodiscard]] std::size_t GetManagedInstanceCount() const noexcept;
        [[nodiscard]] std::size_t GetSharedSurfaceDataCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneInstanceCount() const noexcept;
        /** @brief Scene vector index에 해당하는 instance descriptor resources를 반환한다. */
        [[nodiscard]] const TSurfaceStateDescriptorResources* GetInstanceDescriptors(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceStateDescriptorResources* GetAnyInstanceDescriptors() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceChannelCount(std::size_t SceneIndex) const;
        [[nodiscard]] bool IsCurrentStateAB(std::size_t SceneIndex) const;
        void AdvanceCurrentState(std::size_t SceneIndex);

    private:
        struct TSharedSurfaceResources
        {
            // Reverse member destruction releases Profile resources before Geometry resources.
            std::unique_ptr<TSurfaceSharedGeometryGPUResources> Geometry;
            std::unique_ptr<TSurfaceProfileGPUResources> Profiles;
        };

        struct TInstanceResources
        {
            std::unique_ptr<TSurfaceInstanceGPUResources> State;
            // Descriptor sets/layout must die before the buffers they reference.
            std::unique_ptr<TSurfaceStateDescriptorResources> Descriptors;
            bool bCurrentStateAB = true;
        };

        std::unordered_map<TSurfaceRuntimeDataHandle, TSharedSurfaceResources> SharedSurfaceData;
        std::vector<std::unique_ptr<TInstanceResources>> InstanceResources;
    };
} // namespace MDSS
