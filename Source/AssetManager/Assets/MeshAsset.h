/**
 * @file MeshAsset.h
 * @brief 정점·인덱스 데이터와 GPU 메시 버퍼의 소유권.
 */

#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "AssetManager/Core/Asset.h"
#include "GPU/Vulkan/Resource/GPUBuffer.h"
#include "SurfaceState/Types/SurfaceMappingTypes.h"

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace MDSS::GPU
{
    class TVulkanContext;
}

namespace MDSS::Asset
{

    struct TMeshSection
    {
        std::uint32_t                 FirstIndex = 0;
        std::uint32_t                 IndexCount = 0;
        TMaterialAssetHandle          Material = InvalidAssetHandle;
        SurfaceState::TSurfaceLocalID Surface = SurfaceState::InvalidSurfaceID;
    };

    class TMeshAsset final : public TAsset
    {
    public:
        // Mesh asset construction
        /**
         * @brief CPU mesh data를 보관하고 정점·인덱스 GPU buffer를 업로드한다.
         * @throws std::runtime_error GPU buffer 생성 또는 업로드가 실패한 경우.
         */
        TMeshAsset(TAssetID                         ID,
                   std::string                      Name,
                   std::filesystem::path            SourcePath,
                   const GPU::TVulkanContext&       Context,
                   std::vector<TVertex>             Vertices,
                   std::vector<std::uint32_t>       Indices,
                   std::vector<TMeshSection>        Sections,
                   std::vector<TMeshTriangleSource> Triangles);

        // CPU mesh data and GPU buffer access
        [[nodiscard]] const std::vector<TVertex>&             GetVertices() const noexcept;
        [[nodiscard]] const std::vector<std::uint32_t>&       GetIndices() const noexcept;
        [[nodiscard]] const std::vector<TMeshSection>&        GetSections() const noexcept;
        [[nodiscard]] const std::vector<TMeshTriangleSource>& GetTriangles() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer&                  GetVertexBuffer() const noexcept;
        [[nodiscard]] const GPU::TGPUBuffer&                  GetIndexBuffer() const noexcept;

    private:
        std::vector<TVertex>             Vertices;
        std::vector<std::uint32_t>       Indices;
        std::vector<TMeshSection>        Sections;
        std::vector<TMeshTriangleSource> Triangles;
        std::unique_ptr<GPU::TGPUBuffer> VertexBuffer;
        std::unique_ptr<GPU::TGPUBuffer> IndexBuffer;
    };
} // namespace MDSS::Asset
