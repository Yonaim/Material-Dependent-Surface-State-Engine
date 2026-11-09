/**
 * @file SurfaceGPUResourceLayout.h
 * @brief Surface GPU 버퍼 레이아웃에 맞는 CPU 패킹 레코드와 변환 함수를 정의한다.
 */

#pragma once

#include "SurfaceState/GPU/Abi/SurfaceGPUAbi.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <span>
#include <vector>

namespace MDSS::SurfaceState
{
    struct TSurfaceGPUSharedGeometryUpload
    {
        std::vector<std::uint32_t>   TexelSurfaceIndices;
        std::vector<std::uint32_t>   TexelProfileIndices;
        std::vector<TSurfaceGPUVec4> Positions;
        std::vector<TSurfaceGPUVec4> Normals;
        // xyz: Meso normal; w: maximum Meso height in the same surface/chart.
        std::vector<TSurfaceGPUVec4>            MesoNormals;
        std::vector<TSurfaceGPUGeometryScalar>  GeometryScalars;
        std::vector<TSurfaceGPUNeighborIndices> NeighborIndices;
        // Eight 4-bit reverse direction indices per texel; 0xf means no reciprocal neighbor.
        std::vector<std::uint32_t>           ReverseNeighborDirectionIndices;
        std::vector<TSurfaceGPUSurfaceRange> SurfaceRanges;
        std::vector<std::uint32_t>           TexelChartIndices;
    };

    struct TSurfaceGPUProfileUpload
    {
        std::vector<TSurfaceGPUProfileParameters> Parameters;
        std::vector<std::uint32_t>                Supported;
        std::vector<std::uint32_t>                Reactions;
        std::uint32_t                             MaxReactionCount = 0;
    };

    struct TSurfaceSparseMetadataLayout
    {
        std::size_t WorkgroupCount = 0;
        std::size_t SolverACommandWord = 0;
        std::size_t SolverAFlagWord = 0;
        std::size_t SolverAListWord = 0;
        std::size_t SolverAMaskWord = 0;
        std::size_t SolverAWorkgroupMaskWord = 0;
        std::size_t SolverBCommandWord = 0;
        std::size_t SolverBFlagWord = 0;
        std::size_t SolverBListWord = 0;
        std::size_t SolverBMaskWord = 0;
        std::size_t SolverBWorkgroupMaskWord = 0;
        std::size_t InputCountWord = 0;
        std::size_t InputMaskWord = 0;
        std::size_t InputListWord = 0;
        std::size_t InputGroupMaskWord = 0;
        std::size_t AccumulationCommandWord = 0;
        std::size_t AccumulationFlagWord = 0;
        std::size_t AccumulationListWord = 0;
        std::size_t GeometryCommandWord = 0;
        std::size_t GeometryFlagWord = 0;
        std::size_t GeometryListWord = 0;
        std::size_t TotalWordCount = 0;
    };

    /** @brief Accumulation-height 뒤에 붙는 persistent solver/sparse scheduling metadata의 word layout을 계산한다. */
    [[nodiscard]] TSurfaceSparseMetadataLayout GetSurfaceSparseMetadataLayout(std::size_t TexelCount);

    // Shared geometry upload layout
    /** @brief Optional Runtime-local → Scene Profile index remap; CPU Geometry remains unchanged. */
    [[nodiscard]] TSurfaceGPUSharedGeometryUpload
    PackSharedSurfaceGeometry(const TSharedSurfaceGeometryData&     Geometry,
                              std::span<const TSurfaceProfileIndex> ProfileIndexRemap = {});

    // Transform-dependent instance data
    [[nodiscard]] float              GetSurfaceWorldTexelArea(const TSurfaceTexelGeometry& Texel,
                                                              const glm::mat4&             ModelMatrix) noexcept;
    [[nodiscard]] std::vector<float> BuildSurfaceGPUWorldTexelAreas(const TSharedSurfaceGeometryData& Geometry,
                                                                    const glm::mat4&                  ModelMatrix);

    /** @brief 대칭 TransferWeight 규칙을 각 텍셀의 이웃 방향 인덱스별 cache로 만든다. */
    [[nodiscard]] std::vector<float>
    BuildSurfaceGPUTransferWeights(const TSharedSurfaceGeometryData& Geometry,
                                   const glm::mat4&                  ModelMatrix,
                                   std::vector<TSurfaceGPUVec4>*     OutDebugAverages = nullptr,
                                   bool                              bUseNormalWeight = true,
                                   bool                              bUseDistanceWeight = true,
                                   bool                              bUseProfileBoundaryWeight = true);

    // Profile and buffer layouts
    [[nodiscard]] TSurfaceGPUProfileUpload PackSurfaceProfiles(const std::vector<TSurfaceResponseProfileData>& Profiles,
                                                               const TSurfaceStateRegistry& Registry);

    [[nodiscard]] std::size_t
    GetSurfaceGPUStateValueIndex(std::size_t TexelIndex, std::size_t ChannelIndex, std::size_t ChannelCount);

    [[nodiscard]] std::size_t
    GetSurfaceGPUProfileRecordIndex(std::size_t ProfileIndex, std::size_t ChannelIndex, std::size_t ChannelCount);

    [[nodiscard]] std::size_t
    GetSurfaceGPUBufferByteSize(std::size_t ElementCount, std::size_t ElementStride, std::size_t MaxStorageBufferRange);
} // MDSS 네임스페이스
