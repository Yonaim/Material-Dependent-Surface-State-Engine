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
    class FVulkanContext;
}

namespace MDSS::Asset
{

    struct FMeshSection
    {
        std::uint32_t                 FirstIndex = 0;
        std::uint32_t                 IndexCount = 0;
        FMaterialAssetHandle          Material = InvalidAssetHandle;
        SurfaceState::FSurfaceLocalID Surface = SurfaceState::InvalidSurfaceID;
    };

    class FMeshAsset final : public FAsset
    {
    public:
        // Mesh asset construction
        /**
         * @brief CPU mesh data를 보관하고 정점·인덱스 GPU buffer를 업로드한다.
         * @throws std::runtime_error GPU buffer 생성 또는 업로드가 실패한 경우.
         */
        FMeshAsset(FAssetID                         ID,
                   std::string                      Name,
                   std::filesystem::path            SourcePath,
                   const GPU::FVulkanContext&       Context,
                   std::vector<FVertex>             Vertices,
                   std::vector<std::uint32_t>       Indices,
                   std::vector<FMeshSection>        Sections,
                   std::vector<FMeshTriangleSource> Triangles);

        // CPU mesh data and GPU buffer access
        [[nodiscard]] const std::vector<FVertex>& GetVertices() const noexcept;
        [[nodiscard]] const glm::vec3&            GetBoundsCenter() const noexcept
        {
            return BoundsCenter;
        }

        [[nodiscard]] float GetBoundsRadius() const noexcept
        {
            return BoundsRadius;
        }

        [[nodiscard]] const std::vector<std::uint32_t>&       GetIndices() const noexcept;
        [[nodiscard]] const std::vector<FMeshSection>&        GetSections() const noexcept;
        [[nodiscard]] const std::vector<FMeshTriangleSource>& GetTriangles() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer&                  GetVertexBuffer() const noexcept;
        [[nodiscard]] const GPU::FGPUBuffer&                  GetIndexBuffer() const noexcept;

    private:
        std::vector<FVertex>             Vertices;
        glm::vec3                        BoundsCenter{0.0F};
        float                            BoundsRadius = 0.0F;
        std::vector<std::uint32_t>       Indices;
        std::vector<FMeshSection>        Sections;
        std::vector<FMeshTriangleSource> Triangles;
        std::unique_ptr<GPU::FGPUBuffer> VertexBuffer;
        std::unique_ptr<GPU::FGPUBuffer> IndexBuffer;
    };
} // namespace MDSS::Asset
