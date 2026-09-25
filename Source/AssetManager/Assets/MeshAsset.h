/**
 * @file MeshAsset.h
 * @brief 정점·인덱스 데이터와 GPU 메시 버퍼의 소유권.
 */

#pragma once

#include "AssetManager/Core/Asset.h"
#include "AssetManager/Assets/MeshSourceData.h"
#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"
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

    struct MeshSection
    {
        std::uint32_t       FirstIndex = 0;
        std::uint32_t       IndexCount = 0;
        MaterialAssetHandle Material = InvalidAssetHandle;
        SurfaceLocalID      Surface = InvalidSurfaceID;
    };

    class MeshAsset final : public Asset
    {
    public:
        /**
         * @brief CPU mesh data를 보관하고 정점·인덱스 GPU buffer를 업로드한다.
         * @throws std::runtime_error GPU buffer 생성 또는 업로드가 실패한 경우.
         */
        MeshAsset(AssetID                         ID,
                  std::string                     Name,
                  std::filesystem::path           SourcePath,
                  const VulkanContext&            Context,
                  std::vector<Vertex>             Vertices,
                  std::vector<std::uint32_t>      Indices,
                  std::vector<MeshSection>        Sections,
                  std::vector<MeshTriangleSource> Triangles);

        [[nodiscard]] const std::vector<Vertex>&             GetVertices() const noexcept;
        [[nodiscard]] const std::vector<std::uint32_t>&      GetIndices() const noexcept;
        [[nodiscard]] const std::vector<MeshSection>&        GetSections() const noexcept;
        [[nodiscard]] const std::vector<MeshTriangleSource>& GetTriangles() const noexcept;
        [[nodiscard]] const GPUBuffer&                       GetVertexBuffer() const noexcept;
        [[nodiscard]] const GPUBuffer&                       GetIndexBuffer() const noexcept;

    private:
        std::vector<Vertex>             Vertices;
        std::vector<std::uint32_t>      Indices;
        std::vector<MeshSection>        Sections;
        std::vector<MeshTriangleSource> Triangles;
        std::unique_ptr<GPUBuffer>      VertexBuffer;
        std::unique_ptr<GPUBuffer>      IndexBuffer;
    };
} // namespace MDSS
