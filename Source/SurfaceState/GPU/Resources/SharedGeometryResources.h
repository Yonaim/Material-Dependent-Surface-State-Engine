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

// ============================================================
// 00. Types and Resource Interface
// ============================================================
#pragma region 00_Types_and_Resource_Interface
// 계산 단계에서 공유하는 record와 자원 참조의 형식을 먼저 정의한다.

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
    struct FSurfaceTexelMeshGPUVariant
    {
        struct FTileVertexList
        {
            std::vector<std::uint32_t> Words;
            std::uint32_t              TileCount = 0;
        };
        std::unique_ptr<GPU::FGPUBuffer>         IndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>         VertexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>         BoundaryBuffer;
        std::uint32_t                            VertexCount = 0;
        std::uint32_t                            BoundaryCount = 0;
        std::vector<FSurfaceTexelMeshRange>      Ranges;
        std::map<std::uint32_t, FTileVertexList> TileVertexLists;

        void RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept;
    };

    class FSurfaceSharedGeometryGPUResources final
    {
    public:
        // Shared geometry resource creation
        FSurfaceSharedGeometryGPUResources(VkPhysicalDevice                            PhysicalDevice,
                                           VkDevice                                    Device,
                                           const FSharedSurfaceGeometryData&           Geometry,
                                           std::span<const FSurfaceProfileIndex>       ProfileIndexRemap = {},
                                           std::span<const Asset::FVertex>             SourceVertices = {},
                                           std::span<const Asset::FMeshTriangleSource> SourceTriangles = {},
                                           FSurfaceGPUBufferPool*                      Pool = nullptr);
        void RetireBuffers(FSurfaceGPUBufferPool& Pool) noexcept;

        // Geometry buffers
        [[nodiscard]] const GPU::FGPUBuffer& GetTexelSurfaceIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetTexelProfileIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetPositionBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetNormalBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetMesoNormalBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetGeometryScalarBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetNeighborIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetReverseNeighborDirectionIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer& GetSurfaceRangeBuffer() const noexcept;
        [[nodiscard]] const std::vector<FSurfaceGPUSurfaceRange>& GetSurfaceRanges() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer&                      GetTexelChartIndexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer&                      GetRenderSamplingBoundaryFlagBuffer() const noexcept;
        [[nodiscard]] std::size_t                                 GetTexelCount() const noexcept;

        // Render mesh variants
        [[nodiscard]] const GPU::FGPUBuffer* GetTexelMeshIndexBuffer() const noexcept
        {
            return FullTexelMesh.IndexBuffer.get();
        }

        [[nodiscard]] const GPU::FGPUBuffer* GetTexelMeshVertexBuffer() const noexcept
        {
            return FullTexelMesh.VertexBuffer.get();
        }

        [[nodiscard]] const GPU::FGPUBuffer* GetTexelMeshBoundaryBuffer() const noexcept
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

        [[nodiscard]] const std::vector<FSurfaceTexelMeshRange>& GetTexelMeshRanges() const noexcept
        {
            return FullTexelMesh.Ranges;
        }

        [[nodiscard]] const FSurfaceTexelMeshGPUVariant& GetTexelMeshVariant(std::uint32_t RenderResolution) const
        {
            if (RenderResolution == SimulationResolution)
                return FullTexelMesh;
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
            std::erase_if(TexelMeshVariants,
                          [=](const auto& Item) { return Item.first != Surface && Item.first != Overlay; });
        }

    private:
        std::size_t                                                  TexelCount = 0;
        std::unique_ptr<GPU::FGPUBuffer>                             TexelSurfaceIndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             TexelProfileIndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             PositionBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             NormalBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             MesoNormalBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             GeometryScalarBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             NeighborIndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             ReverseNeighborDirectionIndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             SurfaceRangeBuffer;
        std::vector<FSurfaceGPUSurfaceRange>                         SurfaceRanges;
        std::unique_ptr<GPU::FGPUBuffer>                             TexelChartIndexBuffer;
        std::unique_ptr<GPU::FGPUBuffer>                             RenderSamplingBoundaryFlagBuffer;
        std::uint32_t                                                SimulationResolution = 0;
        FSurfaceTexelMeshGPUVariant                                  FullTexelMesh;
        mutable std::map<std::uint32_t, FSurfaceTexelMeshGPUVariant> TexelMeshVariants;
        std::function<FSurfaceTexelMeshGPUVariant(std::uint32_t)>    MeshVariantFactory;

#pragma endregion
    };

} // namespace MDSS::SurfaceState
