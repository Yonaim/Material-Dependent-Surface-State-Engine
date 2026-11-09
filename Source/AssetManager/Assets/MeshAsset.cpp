/**
 * @file MeshAsset.cpp
 * @brief 정점·인덱스 데이터와 GPU 메시 버퍼의 소유권.
 */

#include "AssetManager/Assets/MeshAsset.h"

#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <stdexcept>
#include <utility>

namespace MDSS::Asset
{
// ============================================================
// 00. FMeshAsset Implementation
// ============================================================
#pragma region 00_TMeshAsset_Implementation

    FMeshAsset::FMeshAsset(FAssetID                         ID,
                           std::string                      Name,
                           std::filesystem::path            SourcePath,
                           const GPU::FVulkanContext&       Context,
                           std::vector<FVertex>             Vertices,
                           std::vector<std::uint32_t>       Indices,
                           std::vector<FMeshSection>        Sections,
                           std::vector<FMeshTriangleSource> Triangles)
        : FAsset(ID, std::move(Name), std::move(SourcePath)), Vertices(std::move(Vertices)),
          Indices(std::move(Indices)), Sections(std::move(Sections)), Triangles(std::move(Triangles))
    {
        if (this->Vertices.empty() || this->Indices.empty())
        {
            throw std::invalid_argument("TMeshAsset requires non-empty vertex and index data.");
        }

        if (this->Indices.size() % 3U != 0 || this->Triangles.size() != this->Indices.size() / 3U)
        {
            throw std::invalid_argument("TMeshAsset source triangle count must match its render index data.");
        }

        glm::vec3 Minimum(std::numeric_limits<float>::max());
        glm::vec3 Maximum(std::numeric_limits<float>::lowest());
        bool      bFiniteBounds = true;

        for (const FVertex& Vertex : this->Vertices)
        {
            const glm::vec3 P = Vertex.Position;

            if (!std::isfinite(P.x) || !std::isfinite(P.y) || !std::isfinite(P.z))
            {
                bFiniteBounds = false;
                break;
            }

            Minimum = glm::min(Minimum, P);
            Maximum = glm::max(Maximum, P);
        }

        if (bFiniteBounds)
        {
            BoundsCenter = Minimum + (Maximum - Minimum) * 0.5F;

            for (const FVertex& Vertex : this->Vertices)
                BoundsRadius = std::max(BoundsRadius, glm::length(Vertex.Position - BoundsCenter));
        }
        else
            BoundsRadius = std::numeric_limits<float>::infinity();

        const VkDeviceSize VertexBytes = sizeof(FVertex) * this->Vertices.size();
        const VkDeviceSize IndexBytes = sizeof(std::uint32_t) * this->Indices.size();

        VertexBuffer = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                         Context.GetDevice(),
                                                         VertexBytes,
                                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                         GPU::EGPUBufferMemoryCategory::MeshAsset);
        IndexBuffer = std::make_unique<GPU::FGPUBuffer>(Context.GetPhysicalDevice(),
                                                        Context.GetDevice(),
                                                        IndexBytes,
                                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                        GPU::EGPUBufferMemoryCategory::MeshAsset);

        VertexBuffer->Upload(this->Vertices.data(), VertexBytes);
        IndexBuffer->Upload(this->Indices.data(), IndexBytes);

        FLogger::Debug("TAssetManager",
                       "Uploaded TMeshAsset '" + GetName() + "' to GPU (vertex bytes=" + std::to_string(VertexBytes) +
                           ", index bytes=" + std::to_string(IndexBytes) + ").");
    }

    const std::vector<FVertex>& FMeshAsset::GetVertices() const noexcept
    {
        return Vertices;
    }

    const std::vector<std::uint32_t>& FMeshAsset::GetIndices() const noexcept
    {
        return Indices;
    }

    const std::vector<FMeshSection>& FMeshAsset::GetSections() const noexcept
    {
        return Sections;
    }

    const std::vector<FMeshTriangleSource>& FMeshAsset::GetTriangles() const noexcept
    {
        return Triangles;
    }

    const GPU::FGPUBuffer& FMeshAsset::GetVertexBuffer() const noexcept
    {
        return *VertexBuffer;
    }

    const GPU::FGPUBuffer& FMeshAsset::GetIndexBuffer() const noexcept
    {
        return *IndexBuffer;
    }
#pragma endregion
} // namespace MDSS::Asset
