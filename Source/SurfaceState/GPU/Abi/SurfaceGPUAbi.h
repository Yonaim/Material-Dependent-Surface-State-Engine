#pragma once
#include "SurfaceState/Types/SurfaceMappingTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace MDSS::SurfaceState
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
        std::array<float, 4> AccumulationThickness{};
    };

    struct alignas(16) TSurfaceSolverPushConstants
    {
        float                               DeltaTime = 0.0F;
        std::uint32_t                       StateChannelCount = 0;
        std::uint32_t                       LocalTexelCount = 0;
        std::uint32_t                       Flags = 0;
        std::array<float, 4>                GravityWorld{};
        std::array<std::array<float, 4>, 3> ModelLinearColumns{};
        // xyz: inverse-transpose columns, w: corresponding component of the gravity up axis.
        std::array<std::array<float, 4>, 3> NormalMatrixAndUpColumns{};
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
    static_assert(sizeof(TSurfaceGPUProfileParameters) == 48);
    static_assert(alignof(TSurfaceGPUProfileParameters) == 16);
    static_assert(offsetof(TSurfaceGPUProfileParameters, DecayAndGeometry) == 16);
    static_assert(offsetof(TSurfaceGPUProfileParameters, AccumulationThickness) == 32);
    static_assert(sizeof(TSurfaceSolverPushConstants) == 128);
    static_assert(alignof(TSurfaceSolverPushConstants) == 16);
    static_assert(offsetof(TSurfaceSolverPushConstants, GravityWorld) == 16);
    static_assert(offsetof(TSurfaceSolverPushConstants, ModelLinearColumns) == 32);
    static_assert(offsetof(TSurfaceSolverPushConstants, NormalMatrixAndUpColumns) == 80);

} // namespace MDSS::SurfaceState
