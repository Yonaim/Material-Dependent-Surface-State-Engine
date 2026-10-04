/**
 * @file SurfaceGPUResources.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"

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
    struct TSurfaceTexelMeshGPUVariant
    {
        std::unique_ptr<GPU::TGPUBuffer>    IndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    VertexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    BoundaryBuffer;
        std::uint32_t                       VertexCount = 0;
        std::uint32_t                       BoundaryCount = 0;
        std::vector<TSurfaceTexelMeshRange> Ranges;
    };

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
        ReverseNeighborDirectionIndices,
        WorldTexelAreas,
        DynamicGeometry,
        AccumulationHeights,
        DynamicConcavityWeights,
        Count
    };

    class TSurfaceSharedGeometryGPUResources final
    {
    public:
        TSurfaceSharedGeometryGPUResources(VkPhysicalDevice                            PhysicalDevice,
                                           VkDevice                                    Device,
                                           const TSharedSurfaceGeometryData&           Geometry,
                                           std::span<const TSurfaceProfileIndex>       ProfileIndexRemap = {},
                                           std::span<const Asset::TVertex>             SourceVertices = {},
                                           std::span<const Asset::TMeshTriangleSource> SourceTriangles = {});

        [[nodiscard]] const GPU::TGPUBuffer& GetTexelSurfaceIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTexelProfileIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetPositionBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetNormalBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetMesoNormalBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetGeometryScalarBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetNeighborIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetReverseNeighborDirectionIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetSurfaceRangeBuffer() const noexcept;
        [[nodiscard]] const std::vector<TSurfaceGPUSurfaceRange>& GetSurfaceRanges() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTexelChartIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetRenderSamplingBoundaryFlagBuffer() const noexcept;
        [[nodiscard]] std::size_t            GetTexelCount() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer* GetTexelMeshIndexBuffer() const noexcept
        {
            return FullTexelMesh.IndexBuffer.get();
        }
        [[nodiscard]] const GPU::TGPUBuffer* GetTexelMeshVertexBuffer() const noexcept
        {
            return FullTexelMesh.VertexBuffer.get();
        }
        [[nodiscard]] const GPU::TGPUBuffer* GetTexelMeshBoundaryBuffer() const noexcept
        {
            return FullTexelMesh.BoundaryBuffer.get();
        }
        [[nodiscard]] std::uint32_t GetTexelMeshVertexCount() const noexcept
        {
            return FullTexelMesh.VertexCount;
        }
        [[nodiscard]] std::uint32_t GetTexelMeshBoundaryCount() const noexcept
        {
            return FullTexelMesh.BoundaryCount;
        }
        [[nodiscard]] const std::vector<TSurfaceTexelMeshRange>& GetTexelMeshRanges() const noexcept
        {
            return FullTexelMesh.Ranges;
        }
        [[nodiscard]] const TSurfaceTexelMeshGPUVariant& GetTexelMeshVariant(std::uint32_t RenderResolution) const noexcept
        {
            const auto Found = TexelMeshVariants.find(RenderResolution);
            return Found != TexelMeshVariants.end() ? Found->second : FullTexelMesh;
        }

    private:
        std::size_t                         TexelCount = 0;
        std::unique_ptr<GPU::TGPUBuffer>    TexelSurfaceIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    TexelProfileIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    PositionBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    NormalBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    MesoNormalBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    GeometryScalarBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    NeighborIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    ReverseNeighborDirectionIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    SurfaceRangeBuffer;
        std::vector<TSurfaceGPUSurfaceRange> SurfaceRanges;
        std::unique_ptr<GPU::TGPUBuffer>    TexelChartIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>    RenderSamplingBoundaryFlagBuffer;
        std::uint32_t                       SimulationResolution = 0;
        TSurfaceTexelMeshGPUVariant         FullTexelMesh;
        std::map<std::uint32_t, TSurfaceTexelMeshGPUVariant> TexelMeshVariants;
    };

    class TSurfaceProfileGPUResources final
    {
    public:
        TSurfaceProfileGPUResources(VkPhysicalDevice                                PhysicalDevice,
                                    VkDevice                                        Device,
                                    const std::vector<TSurfaceResponseProfileData>& Profiles,
                                    const TSurfaceStateRegistry&                    Registry);

        [[nodiscard]] const GPU::TGPUBuffer& GetParametersBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetSupportedBuffer() const noexcept;
        [[nodiscard]] std::size_t            GetProfileCount() const noexcept;
        [[nodiscard]] std::size_t            GetChannelCount() const noexcept;
        [[nodiscard]] bool IsSupported(std::size_t ProfileIndex, std::size_t ChannelIndex) const noexcept;
        void
        UpdateParameters(std::size_t ProfileIndex, std::size_t ChannelIndex, const TSurfaceStateParameters& Parameters);

    private:
        std::size_t                      ProfileCount = 0;
        std::size_t                      ChannelCount = 0;
        std::vector<std::uint32_t>       SupportedChannels;
        std::unique_ptr<GPU::TGPUBuffer> ParametersBuffer;
        std::unique_ptr<GPU::TGPUBuffer> SupportedBuffer;
    };

    class TSurfaceInstanceGPUResources final
    {
    public:
        TSurfaceInstanceGPUResources(VkPhysicalDevice                    PhysicalDevice,
                                     VkDevice                            Device,
                                     std::size_t                         TexelCount,
                                     std::size_t                         ChannelCount,
                                     const std::vector<float>&           TransferWeights,
                                     const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {},
                                     const std::vector<float>&           WorldTexelAreas = {});

        [[nodiscard]] const GPU::TGPUBuffer& GetStateABuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetStateBBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetOutgoingFluxScaleBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetInputDeltaBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTransferWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetTransferWeightDebugAverageBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetRawOutgoingBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetWorldTexelAreaBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetDynamicGeometryBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetDynamicConcavityWeightBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetAccumulationHeightBuffer() const noexcept;
        void                                 UpdateWorldTexelAreas(const std::vector<float>& WorldTexelAreas);
        void                                 UpdateTransferWeights(const std::vector<float>&           TransferWeights,
                                                                   const std::vector<TSurfaceGPUVec4>& TransferWeightDebugAverages = {});
        [[nodiscard]] std::size_t            GetTexelCount() const noexcept;
        [[nodiscard]] std::size_t            GetChannelCount() const noexcept;
        void                                 ResetState(const std::vector<float>& OutgoingFluxScale);

    private:
        std::size_t                      TexelCount = 0;
        std::size_t                      ChannelCount = 0;
        std::size_t                      ScalarCount = 0;
        std::unique_ptr<GPU::TGPUBuffer> StateABuffer;
        std::unique_ptr<GPU::TGPUBuffer> StateBBuffer;
        std::unique_ptr<GPU::TGPUBuffer> OutgoingFluxScaleBuffer;
        std::unique_ptr<GPU::TGPUBuffer> InputDeltaBuffer;
        std::unique_ptr<GPU::TGPUBuffer> TransferWeightBuffer;
        std::unique_ptr<GPU::TGPUBuffer> TransferWeightDebugAverageBuffer;
        std::unique_ptr<GPU::TGPUBuffer> RawOutgoingBuffer;
        std::unique_ptr<GPU::TGPUBuffer> WorldTexelAreaBuffer;
        std::unique_ptr<GPU::TGPUBuffer> DynamicGeometryBuffer;
        std::unique_ptr<GPU::TGPUBuffer> DynamicConcavityWeightBuffer;
        std::unique_ptr<GPU::TGPUBuffer> AccumulationHeightBuffer;
    };

    class TSurfaceStateDescriptorResources final
    {
    public:
        TSurfaceStateDescriptorResources(VkDevice                                  Device,
                                         const TSurfaceSharedGeometryGPUResources& SharedGeometry,
                                         const TSurfaceProfileGPUResources&        Profiles,
                                         const TSurfaceInstanceGPUResources&       Instance);
        ~TSurfaceStateDescriptorResources();

        TSurfaceStateDescriptorResources(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources& operator=(const TSurfaceStateDescriptorResources&) = delete;
        TSurfaceStateDescriptorResources(TSurfaceStateDescriptorResources&&) = delete;
        TSurfaceStateDescriptorResources& operator=(TSurfaceStateDescriptorResources&&) = delete;

        [[nodiscard]] VkDescriptorSetLayout GetLayout() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetABSet() const noexcept;
        [[nodiscard]] VkDescriptorSet       GetBASet() const noexcept;
        [[nodiscard]] VkBuffer              GetBoundBufferHandle(TSurfaceGPUDescriptorBinding Binding, bool bAB) const;
        [[nodiscard]] std::array<std::uint64_t, 6> GetGeometryInputRevisions() const noexcept;

    private:
        VkDevice                                  Device = VK_NULL_HANDLE;
        const TSurfaceSharedGeometryGPUResources* SharedGeometryResources = nullptr;
        VkDescriptorSetLayout                     Layout = VK_NULL_HANDLE;
        VkDescriptorPool                          Pool = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, 2>            Sets{VK_NULL_HANDLE, VK_NULL_HANDLE};
        std::array<std::array<VkBuffer, static_cast<std::size_t>(TSurfaceGPUDescriptorBinding::Count)>, 2>
            BoundBufferHandles{};
    };

    /**
     * @brief Scene 전체 Profile table, Runtime별 Geometry, instance별 Solver buffer를 소유한다.
     */
    class TSurfaceGPUResourceManager final
    {
    public:
        TSurfaceGPUResourceManager(const GPU::TVulkanContext&  Context,
                                   const Asset::TAssetManager& Assets,
                                   const TSurfaceDataManager&  SurfaceData,
                                   const TScene&               Scene);
        ~TSurfaceGPUResourceManager();

        TSurfaceGPUResourceManager(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager& operator=(const TSurfaceGPUResourceManager&) = delete;
        TSurfaceGPUResourceManager(TSurfaceGPUResourceManager&&) = delete;
        TSurfaceGPUResourceManager& operator=(TSurfaceGPUResourceManager&&) = delete;

        [[nodiscard]] std::size_t GetManagedInstanceCount() const noexcept;
        [[nodiscard]] std::size_t GetSharedSurfaceDataCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneProfileCount() const noexcept;
        [[nodiscard]] std::size_t GetSceneInstanceCount() const noexcept;
        /** @brief Scene 벡터 인덱스에 해당하는 instance 디스크립터 리소스를 반환한다. */
        [[nodiscard]] const TSurfaceStateDescriptorResources*   GetInstanceDescriptors(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceSharedGeometryGPUResources* GetInstanceSharedGeometry(std::size_t SceneIndex) const;
        [[nodiscard]] const TSurfaceStateDescriptorResources*   GetAnyInstanceDescriptors() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceInputDeltaBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetInstanceCurrentStateBuffer(std::size_t SceneIndex) const;
        [[nodiscard]] const GPU::TGPUBuffer& GetSceneProfileParametersBuffer() const;
        [[nodiscard]] bool                   UpdateProfileParameters(Asset::TSRProfileAssetHandle   ProfileHandle,
                                                                     TStateId                       State,
                                                                     const TSurfaceStateParameters& Parameters);
        [[nodiscard]] std::size_t            GetInstanceTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t            GetInstanceValidTexelCount(std::size_t SceneIndex) const;
        [[nodiscard]] std::size_t            GetInstanceChannelCount(std::size_t SceneIndex) const;
        [[nodiscard]] bool                   IsCurrentStateAB(std::size_t SceneIndex) const;
        void                                 ResetStates();
        [[nodiscard]] bool NeedsTransferWeightCacheUpdate(std::size_t SceneIndex, const TTransform& Transform) const;
        void               UpdateTransferWeightCache(std::size_t       SceneIndex,
                                                     const TTransform& Transform,
                                                     bool              bUseNormalWeight = true,
                                                     bool              bUseDistanceWeight = true,
                                                     bool              bUseProfileBoundaryWeight = true);
        void               InvalidateTransferWeightCache(std::size_t SceneIndex);
        void               AdvanceCurrentState(std::size_t SceneIndex);

    private:
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
} // MDSS 네임스페이스
