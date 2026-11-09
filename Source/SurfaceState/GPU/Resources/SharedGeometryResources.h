/**
 * @file SharedGeometryResources.h
 * @brief Surface 시뮬레이션 데이터용 Vulkan 스토리지 버퍼와 디스크립터 세트를 관리한다.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <map>
#include <functional>
#include <stdexcept>
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
    struct TSurfaceTexelMeshGPUVariant
    {
        struct TTileVertexList
        {
            std::vector<std::uint32_t> Words;
            std::uint32_t              TileCount = 0;
        };
        std::unique_ptr<GPU::TGPUBuffer>         IndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>         VertexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>         BoundaryBuffer;
        std::uint32_t                            VertexCount = 0;
        std::uint32_t                            BoundaryCount = 0;
        std::vector<TSurfaceTexelMeshRange>      Ranges;
        std::map<std::uint32_t, TTileVertexList> TileVertexLists;

        void RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept;
    };

    class TSurfaceSharedGeometryGPUResources final
    {
    public:
        // Shared geometry resource creation
        TSurfaceSharedGeometryGPUResources(VkPhysicalDevice                            PhysicalDevice,
                                           VkDevice                                    Device,
                                           const TSharedSurfaceGeometryData&           Geometry,
                                           std::span<const TSurfaceProfileIndex>       ProfileIndexRemap = {},
                                           std::span<const Asset::TVertex>             SourceVertices = {},
                                           std::span<const Asset::TMeshTriangleSource> SourceTriangles = {},
                                           TSurfaceGPUBufferPool*                      Pool = nullptr);
        void RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept;

        // Geometry buffers
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
        [[nodiscard]] const GPU::TGPUBuffer&                      GetTexelChartIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer&                      GetRenderSamplingBoundaryFlagBuffer() const noexcept;
        [[nodiscard]] std::size_t                                 GetTexelCount() const noexcept;

        // Render mesh variants
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
        [[nodiscard]] const TSurfaceTexelMeshGPUVariant&
        GetTexelMeshVariant(std::uint32_t RenderResolution) const
        {
            if (RenderResolution == SimulationResolution) return FullTexelMesh;
            if (!IsSurfaceRenderMeshResolution(RenderResolution))
                throw std::invalid_argument("Unsupported render mesh resolution.");
            auto Found = TexelMeshVariants.find(RenderResolution);
            if (Found == TexelMeshVariants.end())
                Found = TexelMeshVariants.emplace(RenderResolution, MeshVariantFactory(RenderResolution)).first;
            return Found->second;
        }
        // GPU-idle only. The simulation-resolution mesh remains available to topology/debug consumers.
        void TrimRenderMeshVariants(std::uint32_t Surface, std::uint32_t Overlay)
        {
            std::erase_if(TexelMeshVariants, [=](const auto& Item) { return Item.first != Surface && Item.first != Overlay; });
        }

    private:
        std::size_t                                          TexelCount = 0;
        std::unique_ptr<GPU::TGPUBuffer>                     TexelSurfaceIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     TexelProfileIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     PositionBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     NormalBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     MesoNormalBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     GeometryScalarBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     NeighborIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     ReverseNeighborDirectionIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     SurfaceRangeBuffer;
        std::vector<TSurfaceGPUSurfaceRange>                 SurfaceRanges;
        std::unique_ptr<GPU::TGPUBuffer>                     TexelChartIndexBuffer;
        std::unique_ptr<GPU::TGPUBuffer>                     RenderSamplingBoundaryFlagBuffer;
        std::uint32_t                                        SimulationResolution = 0;
        TSurfaceTexelMeshGPUVariant                          FullTexelMesh;
        mutable std::map<std::uint32_t, TSurfaceTexelMeshGPUVariant> TexelMeshVariants;
        std::function<TSurfaceTexelMeshGPUVariant(std::uint32_t)> MeshVariantFactory;
    };

} // namespace MDSS::SurfaceState
