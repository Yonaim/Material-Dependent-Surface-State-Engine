/**
 * @file MeshAsset.cpp
 * @brief 정점·인덱스 데이터와 GPU 메시 버퍼의 소유권.
 */

#include "AssetManager/Assets/MeshAsset.h"

#include "Logger/Logger.h"
#include "VulkanContext/VulkanContext.h"

#include <stdexcept>
#include <utility>

namespace MDSS
{
    TMeshAsset::TMeshAsset(TAssetID                         ID,
                         std::string                     Name,
                         std::filesystem::path           SourcePath,
                         const TVulkanContext&            Context,
                         std::vector<TVertex>             Vertices,
                         std::vector<std::uint32_t>      Indices,
                         std::vector<TMeshSection>        Sections,
                         std::vector<TMeshTriangleSource> Triangles)
        : TAsset(ID, std::move(Name), std::move(SourcePath)), Vertices(std::move(Vertices)), Indices(std::move(Indices)),
          Sections(std::move(Sections)), Triangles(std::move(Triangles))
    {
        if (this->Vertices.empty() || this->Indices.empty())
        {
            throw std::invalid_argument("TMeshAsset requires non-empty vertex and index data.");
        }
        if (this->Indices.size() % 3U != 0 || this->Triangles.size() != this->Indices.size() / 3U)
        {
            throw std::invalid_argument("TMeshAsset source triangle count must match its render index data.");
        }

        const VkDeviceSize VertexBytes = sizeof(TVertex) * this->Vertices.size();
        const VkDeviceSize IndexBytes = sizeof(std::uint32_t) * this->Indices.size();

        VertexBuffer =
            std::make_unique<TGPUBuffer>(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        VertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        IndexBuffer =
            std::make_unique<TGPUBuffer>(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        IndexBytes,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VertexBuffer->Upload(this->Vertices.data(), VertexBytes);
        IndexBuffer->Upload(this->Indices.data(), IndexBytes);

        TLogger::Debug("TAssetManager",
                      "Uploaded TMeshAsset '" + GetName() + "' to GPU (vertex bytes=" + std::to_string(VertexBytes) +
                          ", index bytes=" + std::to_string(IndexBytes) + ").");
    }

    const std::vector<TVertex>& TMeshAsset::GetVertices() const noexcept
    {
        return Vertices;
    }

    const std::vector<std::uint32_t>& TMeshAsset::GetIndices() const noexcept
    {
        return Indices;
    }

    const std::vector<TMeshSection>& TMeshAsset::GetSections() const noexcept
    {
        return Sections;
    }

    const std::vector<TMeshTriangleSource>& TMeshAsset::GetTriangles() const noexcept
    {
        return Triangles;
    }

    const TGPUBuffer& TMeshAsset::GetVertexBuffer() const noexcept
    {
        return *VertexBuffer;
    }

    const TGPUBuffer& TMeshAsset::GetIndexBuffer() const noexcept
    {
        return *IndexBuffer;
    }
} // namespace MDSS
