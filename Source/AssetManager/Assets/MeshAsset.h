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
    class TVulkanContext;

    struct TMeshSection
    {
        std::uint32_t       FirstIndex = 0;
        std::uint32_t       IndexCount = 0;
        TMaterialAssetHandle Material = InvalidAssetHandle;
        TSurfaceLocalID      Surface = InvalidSurfaceID;
    };

    class TMeshAsset final : public TAsset
    {
    public:
        /**
         * @brief CPU mesh data를 보관하고 정점·인덱스 GPU buffer를 업로드한다.
         * @throws std::runtime_error GPU buffer 생성 또는 업로드가 실패한 경우.
         */
        TMeshAsset(TAssetID                         ID,
                  std::string                     Name,
                  std::filesystem::path           SourcePath,
                  const TVulkanContext&            Context,
                  std::vector<TVertex>             Vertices,
                  std::vector<std::uint32_t>      Indices,
                  std::vector<TMeshSection>        Sections,
                  std::vector<TMeshTriangleSource> Triangles);

        [[nodiscard]] const std::vector<TVertex>&             GetVertices() const noexcept;
        [[nodiscard]] const std::vector<std::uint32_t>&      GetIndices() const noexcept;
        [[nodiscard]] const std::vector<TMeshSection>&        GetSections() const noexcept;
        [[nodiscard]] const std::vector<TMeshTriangleSource>& GetTriangles() const noexcept;
        [[nodiscard]] const TGPUBuffer&                       GetVertexBuffer() const noexcept;
        [[nodiscard]] const TGPUBuffer&                       GetIndexBuffer() const noexcept;

    private:
        std::vector<TVertex>             Vertices;
        std::vector<std::uint32_t>      Indices;
        std::vector<TMeshSection>        Sections;
        std::vector<TMeshTriangleSource> Triangles;
        std::unique_ptr<TGPUBuffer>      VertexBuffer;
        std::unique_ptr<TGPUBuffer>      IndexBuffer;
    };
} // namespace MDSS
