/**
 * @file SurfaceGPUResources.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "VulkanContext/GPU/GPUBuffer.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
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
        TransferWeights,
        RawOutgoing,
        TransferWeightDebugAverages,
        MesoNormals,
        ReverseNeighborSlots,
        RawFlux,
        WorldTexelAreas,
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
                                           const TSharedSurfaceGeometryData& Geometry,
                                           std::span<const TSurfaceProfileIndex> ProfileIndexRemap = {});

        [[nodiscard]] const TGPUBuffer& GetTexelSurfaceIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTexelProfileIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetPositionBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetNormalBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetMesoNormalBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetGeometryScalarBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetNeighborIndexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetReverseNeighborSlotBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetSurfaceRangeBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTexelChartIndexBuffer() const noexcept;
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;

    private:
        std::size_t TexelCount = 0;
        std::unique_ptr<TGPUBuffer> TexelSurfaceIndexBuffer;
        std::unique_ptr<TGPUBuffer> TexelProfileIndexBuffer;
        std::unique_ptr<TGPUBuffer> PositionBuffer;
        std::unique_ptr<TGPUBuffer> NormalBuffer;
        std::unique_ptr<TGPUBuffer> MesoNormalBuffer;
        std::unique_ptr<TGPUBuffer> GeometryScalarBuffer;
        std::unique_ptr<TGPUBuffer> NeighborIndexBuffer;
        std::unique_ptr<TGPUBuffer> ReverseNeighborSlotBuffer;
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
        [[nodiscard]] bool IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept;
        void
        UpdateParameters(std::size_t ProfileIndex, std::size_t ChannelIndex, const TSurfaceStateParameters& Parameters);

    private:
        std::size_t ProfileCount = 0;
        std::size_t ChannelCount = 0;
        std::vector<std::uint32_t> SupportedChannels;
        std::unique_ptr<TGPUBuffer> ParametersBuffer;
        std::unique_ptr<TGPUBuffer> SupportedBuffer;
    };

    class TSurfaceInstanceGPUResources final
    {
    public:
        TSurfaceInstanceGPUResources(VkPhysicalDevice PhysicalDevice,
                                     VkDevice         Device,
                                     std::size_t      TexelCount,
                                     std::size_t      ChannelCount,
                                     const std::vector<float>& TransferWeights,
                                     const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                     const std::vector<float>& WorldTexelAreas = {});

        [[nodiscard]] const TGPUBuffer& GetStateABuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetStateBBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetOutgoingFluxScaleBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetInputDeltaBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTransferWeightBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetTransferWeightDebugAverageBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetRawOutgoingBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetRawFluxBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetWorldTexelAreaBuffer() const noexcept;
        void UpdateWorldTexelAreas(const std::vector<float>& WorldTexelAreas);
        void UpdateTransferWeights(const std::vector<float>& TransferWeights,
                                   const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {});
        [[nodiscard]] std::size_t GetTexelCount() const noexcept;
        [[nodiscard]] std::size_t GetChannelCount() const noexcept;
        void ResetState(const std::vector<float>& OutgoingFluxScale);

    private:
        std::size_t TexelCount = 0;
        std::size_t ChannelCount = 0;
        std::size_t ScalarCount = 0;
        std::unique_ptr<TGPUBuffer> StateABuffer;
        std::unique_ptr<TGPUBuffer> StateBBuffer;
        std::unique_ptr<TGPUBuffer> OutgoingFluxScaleBuffer;
        std::unique_ptr<TGPUBuffer> InputDeltaBuffer;
        std::unique_ptr<TGPUBuffer> TransferWeightBuffer;
        std::unique_ptr<TGPUBuffer> TransferWeightDebugAverageBuffer;
        std::unique_ptr<TGPUBuffer> RawOutgoingBuffer;
        std::unique_ptr<TGPUBuffer> RawFluxBuffer;
        std::unique_ptr<TGPUBuffer> WorldTexelAreaBuffer;
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

    struct TSurfaceRawFluxMemoryUsage
    {
        VkDeviceSize InstanceRawFluxBytes = 0;
        VkDeviceSize SharedReverseSlotBytes = 0;
    };

    /**
     * @brief Scene 전체 Profile table, Runtime별 Geometry, instance별 Solver buffer를 소유한다.
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
        [[nodiscard]] std::size_t GetSceneProfileCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneInstanceCount() const noexcept;
        /** @brief Actual buffer sizes, counting shared geometry once; excludes allocator overhead. */
        [[nodiscard]] TSurfaceRawFluxMemoryUsage GetRawFluxMemoryUsage() const noexcept;
        /** @brief Scene 벡터 인덱스에 해당하는 instance 디스크립터 리소스를 반환한다. */
        [[nodiscard]] const TSurfaceStateDescriptorResources* GetInstanceDescriptors(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceStateDescriptorResources* GetAnyInstanceDescriptors() const noexcept;
        [[nodiscard]] const TGPUBuffer& GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const TGPUBuffer& GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const TGPUBuffer& GetSceneProfileParametersBuffer() const;
        [[nodiscard]] bool UpdateProfileParameters(TSRProfileAssetHandle ProfileHandle,
                                                   TStateId State,
                                                   const TSurfaceStateParameters& Parameters);
        [[nodiscard]] std::size_t GetInstanceTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceValidTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t GetInstanceChannelCount(std::size_t SceneIndex) const;
        [[nodiscard]] bool IsCurrentStateAB(std::size_t SceneIndex) const;
        void ResetStates();
        [[nodiscard]] bool NeedsTransferWeightCacheUpdate(std::size_t SceneIndex, const glm::mat4& ModelMatrix) const;
        void
        UpdateTransferWeightCache(std::size_t SceneIndex,
                                  const glm::mat4& ModelMatrix,
                                  bool bUseNormalWeight = true,
                                  bool bUseDistanceWeight = true,
                                  bool bUseProfileBoundaryWeight = true,
                                  bool bUseCurvatureWeight = false);
        void InvalidateTransferWeightCache(std::size_t SceneIndex);
        void AdvanceCurrentState(std::size_t SceneIndex);

    private:
        struct TSharedSurfaceResources
        {
            std::unique_ptr<TSurfaceSharedGeometryGPUResources> Geometry;
            std::vector<TSurfaceProfileIndex> SceneProfileIndices;
            const TSharedSurfaceGeometryData* CPUGeometry = nullptr;
        };

        struct TInstanceResources
        {
            std::unique_ptr<TSurfaceInstanceGPUResources> State;
            // 디스크립터가 참조하는 버퍼보다 디스크립터 세트와 레이아웃을 먼저 파괴한다.
            std::unique_ptr<TSurfaceStateDescriptorResources> Descriptors;
            TSurfaceRuntimeDataHandle SurfaceDataHandle{};
            std::size_t ValidTexelCount = 0;
            glm::mat4 TransferWeightModelMatrix{1.0F};
            bool bTransferWeightCacheValid = false;
            bool bCurrentStateAB = true;
        };

        // Reverse destruction order: descriptors → Geometry → Scene Profile buffers.
        std::vector<TSRProfileAssetHandle> SceneProfileHandles;
        std::unique_ptr<TSurfaceProfileGPUResources> SceneProfiles;
        std::unordered_map<TSurfaceRuntimeDataHandle, TSharedSurfaceResources> SharedSurfaceData;
        std::vector<std::unique_ptr<TInstanceResources>> InstanceResources;
    };
} // MDSS 네임스페이스
