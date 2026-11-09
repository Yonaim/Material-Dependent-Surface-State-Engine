#pragma once
#include "SurfaceState/Types/SurfaceMappingTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace MDSS::SurfaceState
{
    struct alignas(16) FSurfaceGPUVec4
    {
        float X = 0.0F;
        float Y = 0.0F;
        float Z = 0.0F;
        float W = 0.0F;
    };

    struct FSurfaceGPUGeometryScalar
    {
        float MesoVirtualHeight = 0.0F;
        float ConcavityWeight = 0.0F;
        float MesoMeanCurvature = 0.0F;
        float MesoGaussianCurvature = 0.0F;
    };

    struct alignas(16) FSurfaceGPUSurfaceRange
    {
        std::uint32_t FirstTexel = 0;
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;
        std::uint32_t TexelCount = 0;
    };

    struct alignas(16) FSurfaceGPUNeighborIndices
    {
        std::array<std::uint32_t, SurfaceNeighborCount> Indices{};
    };

    struct alignas(16) FSurfaceGPUProfileParameters
    {
        std::array<float, 4> CapacityInputAndTransfer{};
        std::array<float, 4> DecayAndGeometry{};
        std::array<float, 4> AccumulationThickness{};
    };

    struct alignas(16) FSurfaceSolverPushConstants
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

    static_assert(sizeof(FSurfaceGPUVec4) == 16);
    static_assert(alignof(FSurfaceGPUVec4) == 16);
    static_assert(offsetof(FSurfaceGPUVec4, W) == 12);
    static_assert(sizeof(FSurfaceGPUGeometryScalar) == 16);
    static_assert(offsetof(FSurfaceGPUGeometryScalar, ConcavityWeight) == 4);
    static_assert(offsetof(FSurfaceGPUGeometryScalar, MesoGaussianCurvature) == 12);
    static_assert(sizeof(FSurfaceGPUNeighborIndices) == 32);
    static_assert(alignof(FSurfaceGPUNeighborIndices) == 16);
    static_assert(sizeof(FSurfaceGPUSurfaceRange) == 16);
    static_assert(alignof(FSurfaceGPUSurfaceRange) == 16);
    static_assert(sizeof(FSurfaceGPUProfileParameters) == 48);
    static_assert(alignof(FSurfaceGPUProfileParameters) == 16);
    static_assert(offsetof(FSurfaceGPUProfileParameters, DecayAndGeometry) == 16);
    static_assert(offsetof(FSurfaceGPUProfileParameters, AccumulationThickness) == 32);
    static_assert(sizeof(FSurfaceSolverPushConstants) == 128);
    static_assert(alignof(FSurfaceSolverPushConstants) == 16);
    static_assert(offsetof(FSurfaceSolverPushConstants, GravityWorld) == 16);
    static_assert(offsetof(FSurfaceSolverPushConstants, ModelLinearColumns) == 32);
    static_assert(offsetof(FSurfaceSolverPushConstants, NormalMatrixAndUpColumns) == 80);

} // namespace MDSS::SurfaceState
