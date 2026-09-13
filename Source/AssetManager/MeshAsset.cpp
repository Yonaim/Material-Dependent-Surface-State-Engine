#include "AssetManager/MeshAsset.h"

#include "Logger/Logger.h"
#include "VulkanContext/VulkanContext.h"

#include <stdexcept>
#include <utility>

namespace MDSS
{
    MeshAsset::MeshAsset(AssetID                    ID,
                         std::string                Name,
                         std::filesystem::path      SourcePath,
                         const VulkanContext&       Context,
                         std::vector<Vertex>        Vertices,
                         std::vector<std::uint32_t> Indices,
                         std::vector<MeshSection>   Sections)
        : Asset(ID, std::move(Name), std::move(SourcePath)), Vertices(std::move(Vertices)), Indices(std::move(Indices)),
          Sections(std::move(Sections))
    {
        if (this->Vertices.empty() || this->Indices.empty())
        {
            throw std::invalid_argument("MeshAsset requires non-empty vertex and index data.");
        }

        const VkDeviceSize VertexBytes = sizeof(Vertex) * this->Vertices.size();
        const VkDeviceSize IndexBytes = sizeof(std::uint32_t) * this->Indices.size();

        VertexBuffer =
            std::make_unique<GPUBuffer>(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        VertexBytes,
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        IndexBuffer =
            std::make_unique<GPUBuffer>(Context.GetPhysicalDevice(),
                                        Context.GetDevice(),
                                        IndexBytes,
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VertexBuffer->Upload(this->Vertices.data(), VertexBytes);
        IndexBuffer->Upload(this->Indices.data(), IndexBytes);

        Logger::Debug("AssetManager",
                      "Uploaded MeshAsset '" + GetName() + "' to GPU (vertex bytes=" + std::to_string(VertexBytes) +
                          ", index bytes=" + std::to_string(IndexBytes) + ").");
    }

    const std::vector<Vertex>& MeshAsset::GetVertices() const noexcept
    {
        return Vertices;
    }

    const std::vector<std::uint32_t>& MeshAsset::GetIndices() const noexcept
    {
        return Indices;
    }

    const std::vector<MeshSection>& MeshAsset::GetSections() const noexcept
    {
        return Sections;
    }

    const GPUBuffer& MeshAsset::GetVertexBuffer() const noexcept
    {
        return *VertexBuffer;
    }

    const GPUBuffer& MeshAsset::GetIndexBuffer() const noexcept
    {
        return *IndexBuffer;
    }
} // namespace MDSS
