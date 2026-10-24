#include "SurfaceState/GPU/Resources/SharedGeometryResources.h"

#include "SurfaceState/GPU/Resources/ResourceBufferHelpers.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/packing.hpp>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace MDSS::SurfaceState
{
    using namespace Detail;

#pragma region Shared_Geometry_Resources

    void TSurfaceTexelMeshGPUVariant::RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept
    {
        Pool.Release(std::move(IndexBuffer));
        Pool.Release(std::move(VertexBuffer));
        Pool.Release(std::move(BoundaryBuffer));
    }

    TSurfaceSharedGeometryGPUResources::TSurfaceSharedGeometryGPUResources(
        VkPhysicalDevice                            PhysicalDevice,
        VkDevice                                    Device,
        const TSharedSurfaceGeometryData&           Geometry,
        std::span<const TSurfaceProfileIndex>       ProfileIndexRemap,
        std::span<const Asset::TVertex>             SourceVertices,
        std::span<const Asset::TMeshTriangleSource> SourceTriangles,
        TSurfaceGPUBufferPool*                      Pool)
        : TexelCount(Geometry.GetTexelCount())
    {
        const std::size_t                     MaxRange = GetMaximumStorageBufferRange(PhysicalDevice);
        const TSurfaceGPUSharedGeometryUpload Upload = PackSharedSurfaceGeometry(Geometry, ProfileIndexRemap);
        SurfaceRanges = Upload.SurfaceRanges;

        TexelSurfaceIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                       Device,
                                                       Upload.TexelSurfaceIndices.data(),
                                                       Upload.TexelSurfaceIndices.size(),
                                                       sizeof(std::uint32_t),
                                                       MaxRange,
                                                       GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                       Pool);
        TexelProfileIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                       Device,
                                                       Upload.TexelProfileIndices.data(),
                                                       Upload.TexelProfileIndices.size(),
                                                       sizeof(std::uint32_t),
                                                       MaxRange,
                                                       GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                       Pool);
        PositionBuffer = CreateUploadedBuffer(PhysicalDevice,
                                              Device,
                                              Upload.Positions.data(),
                                              Upload.Positions.size(),
                                              sizeof(TSurfaceGPUVec4),
                                              MaxRange,
                                              GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                              Pool);
        NormalBuffer = CreateUploadedBuffer(PhysicalDevice,
                                            Device,
                                            Upload.Normals.data(),
                                            Upload.Normals.size(),
                                            sizeof(TSurfaceGPUVec4),
                                            MaxRange,
                                            GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                            Pool);

        // 렌더링과 디버그 뷰가 복원 노멀을 GPU에서 텍셀별로 조회한다.
        MesoNormalBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                Device,
                                                Upload.MesoNormals.data(),
                                                Upload.MesoNormals.size(),
                                                sizeof(TSurfaceGPUVec4),
                                                MaxRange,
                                                GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                Pool);
        GeometryScalarBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                    Device,
                                                    Upload.GeometryScalars.data(),
                                                    Upload.GeometryScalars.size(),
                                                    sizeof(TSurfaceGPUGeometryScalar),
                                                    MaxRange,
                                                    GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                    Pool);
        NeighborIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                   Device,
                                                   Upload.NeighborIndices.data(),
                                                   Upload.NeighborIndices.size(),
                                                   sizeof(TSurfaceGPUNeighborIndices),
                                                   MaxRange,
                                                   GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                   Pool);
        ReverseNeighborDirectionIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                                   Device,
                                                                   Upload.ReverseNeighborDirectionIndices.data(),
                                                                   Upload.ReverseNeighborDirectionIndices.size(),
                                                                   sizeof(std::uint32_t),
                                                                   MaxRange,
                                                                   GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                                   Pool);

        SurfaceRangeBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                  Device,
                                                  Upload.SurfaceRanges.data(),
                                                  Upload.SurfaceRanges.size(),
                                                  sizeof(TSurfaceGPUSurfaceRange),
                                                  MaxRange,
                                                  GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                  Pool);

        TexelChartIndexBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                     Device,
                                                     Upload.TexelChartIndices.data(),
                                                     Upload.TexelChartIndices.size(),
                                                     sizeof(std::uint32_t),
                                                     MaxRange,
                                                     GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                     Pool);

        std::vector<std::uint32_t> BoundaryFlags(TexelCount, 0U);
        for (std::uint32_t Surface = 0; Surface < SurfaceRanges.size(); ++Surface)
        {
            const auto& Range = SurfaceRanges[Surface];
            for (std::uint32_t Y = 0; Y < Range.Height; ++Y)
                for (std::uint32_t X = 0; X < Range.Width; ++X)
                {
                    const std::size_t Center = Range.FirstTexel + std::size_t(Y) * Range.Width + X;
                    const auto        Profile = Upload.TexelProfileIndices[Center];
                    const auto        Chart = Upload.TexelChartIndices[Center];
                    if (Upload.TexelSurfaceIndices[Center] != Surface)
                    {
                        BoundaryFlags[Center] = 1U;
                        continue;
                    }
                    for (int OffsetY = -1; OffsetY <= 1; ++OffsetY)
                        for (int OffsetX = -1; OffsetX <= 1; ++OffsetX)
                        {
                            const auto NeighborX = static_cast<std::uint32_t>(
                                std::clamp<int>(static_cast<int>(X) + OffsetX, 0, static_cast<int>(Range.Width) - 1));
                            const auto NeighborY = static_cast<std::uint32_t>(
                                std::clamp<int>(static_cast<int>(Y) + OffsetY, 0, static_cast<int>(Range.Height) - 1));
                            const std::size_t Neighbor =
                                Range.FirstTexel + std::size_t(NeighborY) * Range.Width + NeighborX;
                            if (Upload.TexelSurfaceIndices[Neighbor] != Surface ||
                                Upload.TexelProfileIndices[Neighbor] != Profile ||
                                Upload.TexelChartIndices[Neighbor] != Chart)
                                BoundaryFlags[Center] = 1U;
                        }
                }
        }

        RenderSamplingBoundaryFlagBuffer = CreateUploadedBuffer(PhysicalDevice,
                                                                Device,
                                                                BoundaryFlags.data(),
                                                                BoundaryFlags.size(),
                                                                sizeof(std::uint32_t),
                                                                MaxRange,
                                                                GPU::TGPUBufferMemoryCategory::SurfaceGeometry,
                                                                Pool);

        SimulationResolution = Geometry.GetSurfaces().empty() ? 0U : Geometry.GetSurfaces().front().Resolution.Width;
        const auto MeshMemoryCategory = [](std::uint32_t Resolution)
        {
            switch (Resolution)
            {
                case 64:
                    return GPU::TGPUBufferMemoryCategory::RenderMesh64;
                case 128:
                    return GPU::TGPUBufferMemoryCategory::RenderMesh128;
                case 256:
                    return GPU::TGPUBufferMemoryCategory::RenderMesh256;
                case 512:
                    return GPU::TGPUBufferMemoryCategory::RenderMesh512;
                default:
                    throw std::invalid_argument("Unsupported render mesh resolution for memory tracking.");
            }
        };

        const auto UploadTexelMesh =
            [&](TSurfaceTexelMesh Mesh, TSurfaceTexelMeshGPUVariant& Output, GPU::TGPUBufferMemoryCategory Category)
        {
            Output.Ranges = std::move(Mesh.Surfaces);
            Output.VertexCount = static_cast<std::uint32_t>(Mesh.Vertices.size());
            Output.BoundaryCount = static_cast<std::uint32_t>(Mesh.BoundaryEdges.size());

            if (Mesh.Indices.empty())
                return;
            if (Mesh.SideAdjacency.size() != Mesh.Indices.size() / 3U + Mesh.BoundaryEdges.size())
                throw std::logic_error("Overlay side adjacency does not match render mesh topology.");

            const auto Bytes = static_cast<VkDeviceSize>(Mesh.Indices.size() * sizeof(std::uint32_t));
            Output.IndexBuffer =
                AcquireOrCreateBuffer(PhysicalDevice,
                                      Device,
                                      Bytes,
                                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      Category,
                                      Pool);
            Output.IndexBuffer->Upload(Mesh.Indices.data(), Bytes);

            const auto VertexBytes = static_cast<VkDeviceSize>(Mesh.Vertices.size() * sizeof(TSurfaceTexelMeshVertex));
            Output.VertexBuffer =
                AcquireOrCreateBuffer(PhysicalDevice,
                                      Device,
                                      VertexBytes,
                                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                      Category,
                                      Pool);
            Output.VertexBuffer->Upload(Mesh.Vertices.data(), VertexBytes);

            // One immutable CSR vertex list per render tile size. Geometry shares these lists;
            // coverage and its history remain owned by each Scene instance.
            for (const std::uint32_t TileSize : {8U, 16U, 32U})
            {
                std::vector<std::uint32_t> SurfaceTileBases(SurfaceRanges.size() + 1U, 0U);
                for (std::size_t Surface = 0; Surface < SurfaceRanges.size(); ++Surface)
                {
                    const auto&       Range = SurfaceRanges[Surface];
                    const std::size_t Tiles = ((static_cast<std::size_t>(Range.Width) + TileSize - 1U) / TileSize) *
                                              ((static_cast<std::size_t>(Range.Height) + TileSize - 1U) / TileSize);
                    if (Tiles > std::numeric_limits<std::uint32_t>::max() - SurfaceTileBases[Surface])
                        throw std::overflow_error("Overlay tile count exceeds uint32 range.");
                    SurfaceTileBases[Surface + 1U] = SurfaceTileBases[Surface] + static_cast<std::uint32_t>(Tiles);
                }

                const std::uint32_t                     TileCount = SurfaceTileBases.back();
                std::vector<std::vector<std::uint32_t>> TileVertices(TileCount);
                for (std::uint32_t Vertex = 0; Vertex < Mesh.Vertices.size(); ++Vertex)
                {
                    const auto& UVSurface = Mesh.Vertices[Vertex].UVSurface;
                    if (!std::isfinite(UVSurface.x) || !std::isfinite(UVSurface.y) || !std::isfinite(UVSurface.w) ||
                        UVSurface.w < 0.0F || UVSurface.w >= static_cast<float>(SurfaceRanges.size()))
                        throw std::runtime_error("Overlay vertex has an invalid surface or UV.");
                    const auto  Surface = static_cast<std::uint32_t>(UVSurface.w);
                    const auto& Range = SurfaceRanges[Surface];
                    if (Range.Width == 0U || Range.Height == 0U)
                        throw std::runtime_error("Overlay vertex belongs to an empty surface range.");
                    const auto X =
                        std::min(static_cast<std::uint32_t>(std::clamp(UVSurface.x, 0.0F, 1.0F) * Range.Width),
                                 Range.Width - 1U);
                    const auto Y =
                        std::min(static_cast<std::uint32_t>(std::clamp(UVSurface.y, 0.0F, 1.0F) * Range.Height),
                                 Range.Height - 1U);
                    const auto TilesX = (Range.Width + TileSize - 1U) / TileSize;
                    const auto Candidate = SurfaceTileBases[Surface] + (Y / TileSize) * TilesX + X / TileSize;
                    TileVertices[Candidate].push_back(Vertex);
                }

                std::vector<std::uint32_t> Packed(TileCount + 1U, 0U);
                for (std::uint32_t Tile = 0; Tile < TileCount; ++Tile)
                {
                    Packed[Tile] = static_cast<std::uint32_t>(Packed.size());
                    Packed.insert(Packed.end(), TileVertices[Tile].begin(), TileVertices[Tile].end());
                }
                Packed[TileCount] = static_cast<std::uint32_t>(Packed.size());

                TSurfaceTexelMeshGPUVariant::TTileVertexList List;
                List.TileCount = TileCount;
                List.Words = std::move(Packed);
                Output.TileVertexLists.emplace(TileSize, std::move(List));
            }

            // The boundary shader reads immutable side neighbors after the boundary edge range.
            // Packing both into one buffer keeps the overlay descriptor count unchanged.
            std::vector<glm::uvec4> PackedBoundaries;
            PackedBoundaries.reserve(Mesh.BoundaryEdges.size() + Mesh.SideAdjacency.size());
            PackedBoundaries.insert(PackedBoundaries.end(), Mesh.BoundaryEdges.begin(), Mesh.BoundaryEdges.end());
            PackedBoundaries.insert(PackedBoundaries.end(), Mesh.SideAdjacency.begin(), Mesh.SideAdjacency.end());
            const auto BoundaryBytes = static_cast<VkDeviceSize>(PackedBoundaries.size() * sizeof(glm::uvec4));
            if (BoundaryBytes == 0 || BoundaryBytes > MaxRange)
                throw std::length_error("Overlay boundary and side adjacency exceed maxStorageBufferRange.");

            Output.BoundaryBuffer = AcquireOrCreateBuffer(
                PhysicalDevice, Device, BoundaryBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, Category, Pool);
            Output.BoundaryBuffer->Upload(PackedBoundaries.data(), BoundaryBytes);
        };

        UploadTexelMesh(BuildSurfaceTexelMesh(Geometry, SourceVertices, SourceTriangles),
                        FullTexelMesh,
                        MeshMemoryCategory(SimulationResolution));

        for (const auto& Preset : SurfaceRenderMeshResolutionPresets)
        {
            if (Preset.Resolution == SimulationResolution)
                continue;
            TSurfaceTexelMeshGPUVariant Variant;
            UploadTexelMesh(BuildSurfaceTexelMesh(Geometry, SourceVertices, SourceTriangles, Preset.Resolution),
                            Variant,
                            MeshMemoryCategory(Preset.Resolution));
            TexelMeshVariants.emplace(Preset.Resolution, std::move(Variant));
        }
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelSurfaceIndexBuffer() const noexcept
    {
        return *TexelSurfaceIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelProfileIndexBuffer() const noexcept
    {
        return *TexelProfileIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetPositionBuffer() const noexcept
    {
        return *PositionBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNormalBuffer() const noexcept
    {
        return *NormalBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetMesoNormalBuffer() const noexcept
    {
        return *MesoNormalBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetGeometryScalarBuffer() const noexcept
    {
        return *GeometryScalarBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetNeighborIndexBuffer() const noexcept
    {
        return *NeighborIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetReverseNeighborDirectionIndexBuffer() const noexcept
    {
        return *ReverseNeighborDirectionIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetSurfaceRangeBuffer() const noexcept
    {
        return *SurfaceRangeBuffer;
    }

    const std::vector<TSurfaceGPUSurfaceRange>& TSurfaceSharedGeometryGPUResources::GetSurfaceRanges() const noexcept
    {
        return SurfaceRanges;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetTexelChartIndexBuffer() const noexcept
    {
        return *TexelChartIndexBuffer;
    }

    const GPU::TGPUBuffer& TSurfaceSharedGeometryGPUResources::GetRenderSamplingBoundaryFlagBuffer() const noexcept
    {
        return *RenderSamplingBoundaryFlagBuffer;
    }

    std::size_t TSurfaceSharedGeometryGPUResources::GetTexelCount() const noexcept
    {
        return TexelCount;
    }

    void TSurfaceSharedGeometryGPUResources::RetireBuffers(TSurfaceGPUBufferPool& Pool) noexcept
    {
        Pool.Release(std::move(TexelSurfaceIndexBuffer));
        Pool.Release(std::move(TexelProfileIndexBuffer));
        Pool.Release(std::move(PositionBuffer));
        Pool.Release(std::move(NormalBuffer));
        Pool.Release(std::move(MesoNormalBuffer));
        Pool.Release(std::move(GeometryScalarBuffer));
        Pool.Release(std::move(NeighborIndexBuffer));
        Pool.Release(std::move(ReverseNeighborDirectionIndexBuffer));
        Pool.Release(std::move(SurfaceRangeBuffer));
        Pool.Release(std::move(TexelChartIndexBuffer));
        Pool.Release(std::move(RenderSamplingBoundaryFlagBuffer));
        FullTexelMesh.RetireBuffers(Pool);
        for (auto& [Resolution, Variant] : TexelMeshVariants)
            Variant.RetireBuffers(Pool);
    }

#pragma endregion
}
