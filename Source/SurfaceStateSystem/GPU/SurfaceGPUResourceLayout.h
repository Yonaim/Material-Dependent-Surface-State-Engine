/**
 * @file SurfaceGPUResourceLayout.h
 * @brief Surface GPU 버퍼 레이아웃에 맞는 CPU 패킹 레코드와 변환 함수를 정의한다.
 */

#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <glm/glm.hpp>
#include <vector>

namespace MDSS
{
    struct alignas(16) TSurfaceGPUVec4
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };

    struct TSurfaceGPUGeometryScalar
    {
        float MesoVirtualHeight = 0.0F;
        float ConcavityWeight = 0.0F;
        float MesoMeanCurvature = 0.0F;
        float MesoGaussianCurvature = 0.0F;
    };

    struct alignas(16) TSurfaceGPUSurfaceRange
    {
        std::uint32_t FirstTexel = 0;
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::uint32_t TexelCount = 0;
    };

    struct alignas(16) TSurfaceGPUNeighborIndices
    {
        std::array<std::uint32_t, SurfaceNeighborCount> Indices{};
    };

    struct alignas(16) TSurfaceGPUProfileParameters
    {
        std::array<float, 4> CapacityInputAndTransfer{};
        std::array<float, 4> DecayAndGeometry{};
    };

    struct alignas(16) TSurfaceSolverPushConstants
    {
        float                        DeltaTime = 0.0F;
        std::uint32_t                StateChannelCount = 0;
        std::uint32_t                LocalTexelCount = 0;
        std::uint32_t                Flags = 0;
        std::array<float, 4>         GravityWorld{};
        std::array<std::array<float, 4>, 3> ModelLinearColumns{};
        // xyz: inverse-transpose columns, w: corresponding component of the gravity up axis.
        std::array<std::array<float, 4>, 3> NormalMatrixAndUpColumns{};
    };

    struct TSurfaceGPUSharedGeometryUpload
    {
        std::vector<std::uint32_t>               TexelSurfaceIndices;
        std::vector<std::uint32_t>               TexelProfileIndices;
        std::vector<TSurfaceGPUVec4>              Positions;
        std::vector<TSurfaceGPUVec4>              Normals;
        std::vector<TSurfaceGPUVec4>            MesoNormals;
        std::vector<TSurfaceGPUGeometryScalar>     GeometryScalars;
        std::vector<TSurfaceGPUNeighborIndices>    NeighborIndices;
        // Eight 4-bit reverse slots per texel; 0xf means no reciprocal neighbor.
        std::vector<std::uint32_t>                 ReverseNeighborSlots;
        std::vector<TSurfaceGPUSurfaceRange>       SurfaceRanges;
        std::vector<std::uint32_t>                 TexelChartIndices;
    };

    struct TSurfaceGPUProfileUpload
    {
        std::vector<TSurfaceGPUProfileParameters> Parameters;
        std::vector<std::uint32_t>                Supported;
    };

    static_assert(sizeof(TSurfaceGPUVec4) == 16);
    static_assert(alignof(TSurfaceGPUVec4) == 16);
    static_assert(offsetof(TSurfaceGPUVec4, W) == 12);
    static_assert(sizeof(TSurfaceGPUGeometryScalar) == 16);
    static_assert(offsetof(TSurfaceGPUGeometryScalar, ConcavityWeight) == 4);
    static_assert(offsetof(TSurfaceGPUGeometryScalar, MesoGaussianCurvature) == 12);
    static_assert(sizeof(TSurfaceGPUNeighborIndices) == 32);
    static_assert(alignof(TSurfaceGPUNeighborIndices) == 16);
    static_assert(sizeof(TSurfaceGPUSurfaceRange) == 16);
    static_assert(alignof(TSurfaceGPUSurfaceRange) == 16);
    static_assert(sizeof(TSurfaceGPUProfileParameters) == 32);
    static_assert(alignof(TSurfaceGPUProfileParameters) == 16);
    static_assert(offsetof(TSurfaceGPUProfileParameters, DecayAndGeometry) == 16);
    static_assert(sizeof(TSurfaceSolverPushConstants) == 128);
    static_assert(alignof(TSurfaceSolverPushConstants) == 16);
    static_assert(offsetof(TSurfaceSolverPushConstants, GravityWorld) == 16);
    static_assert(offsetof(TSurfaceSolverPushConstants, ModelLinearColumns) == 32);
    static_assert(offsetof(TSurfaceSolverPushConstants, NormalMatrixAndUpColumns) == 80);

    /** @brief Optional Runtime-local → Scene Profile index remap; CPU Geometry remains unchanged. */
    [[nodiscard]] TSurfaceGPUSharedGeometryUpload PackSharedSurfaceGeometry(
        const TSharedSurfaceGeometryData& Geometry,
        std::span<const TSurfaceProfileIndex> ProfileIndexRemap = {});

    /** @brief 대칭 TransferWeight 규칙을 각 텍셀의 이웃 슬롯별 cache로 만든다. */
    [[nodiscard]] std::vector<float>
    BuildSurfaceGPUTransferWeights(const TSharedSurfaceGeometryData& Geometry,
        const glm::mat4& ModelMatrix,
        std::vector<TSurfaceGPUVec4>* OutDebugAverages = nullptr,
        bool bUseNormalWeight = true,
        bool bUseDistanceWeight = true,
        bool bUseProfileBoundaryWeight = true,
        bool bUseCurvatureWeight = false);

    [[nodiscard]] TSurfaceGPUProfileUpload PackSurfaceProfiles(const std::vector<TSurfaceResponseProfileData>& Profiles,
        const TSurfaceStateRegistry& Registry);

    [[nodiscard]] std::size_t
    GetSurfaceGPUStateValueIndex(std::size_t TexelIndex, std::size_t ChannelIndex, std::size_t ChannelCount);

    [[nodiscard]] std::size_t
    GetSurfaceGPUProfileRecordIndex(std::size_t ProfileIndex, std::size_t ChannelIndex, std::size_t ChannelCount);

    [[nodiscard]] std::size_t
    GetSurfaceGPUBufferByteSize(std::size_t ElementCount, std::size_t ElementStride, std::size_t MaxStorageBufferRange);
} // MDSS 네임스페이스
