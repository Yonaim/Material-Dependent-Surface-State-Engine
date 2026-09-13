#pragma once

#include "AssetManager/Asset.h"
#include "VulkanContext/GPU/GPUBuffer.h"

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace MDSS
{
    class VulkanContext;

    struct Vertex
    {
        glm::vec3 Position{0.0F};
        glm::vec3 Normal{0.0F, 1.0F, 0.0F};
        glm::vec2 UV{0.0F};
        glm::vec4 Tangent{1.0F, 0.0F, 0.0F, 1.0F}; // precomputed
    };

    struct MeshSection
    {
        std::uint32_t       FirstIndex = 0;
        std::uint32_t       IndexCount = 0;
        MaterialAssetHandle Material = InvalidAssetHandle;
    };

    class MeshAsset final : public Asset
    {
    public:
        MeshAsset(AssetID                    ID,
                  std::string                Name,
                  std::filesystem::path      SourcePath,
                  const VulkanContext&       Context,
                  std::vector<Vertex>        Vertices,
                  std::vector<std::uint32_t> Indices,
                  std::vector<MeshSection>   Sections);

        [[nodiscard]] const std::vector<Vertex>&        GetVertices() const noexcept;
        [[nodiscard]] const std::vector<std::uint32_t>& GetIndices() const noexcept;
        [[nodiscard]] const std::vector<MeshSection>&   GetSections() const noexcept;
        [[nodiscard]] const GPUBuffer&                  GetVertexBuffer() const noexcept;
        [[nodiscard]] const GPUBuffer&                  GetIndexBuffer() const noexcept;

    private:
        std::vector<Vertex>        Vertices;
        std::vector<std::uint32_t> Indices;
        std::vector<MeshSection>   Sections;
        std::unique_ptr<GPUBuffer> VertexBuffer;
        std::unique_ptr<GPUBuffer> IndexBuffer;
    };
} // namespace MDSS
