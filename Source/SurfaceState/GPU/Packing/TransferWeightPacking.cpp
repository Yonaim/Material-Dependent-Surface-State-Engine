#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <stdexcept>

namespace
{
    constexpr float GeometryEpsilon = 1.0e-6F;
    bool            IsFinite(const glm::vec3& Value)
    {
        return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
    }
}

namespace MDSS::SurfaceState
{
// ============================================================
// 00. Transform Dependent Geometry Data
// ============================================================
#pragma region 00_Transform_Dependent_Geometry_Data

    float GetSurfaceWorldTexelArea(const FSurfaceTexelGeometry& Texel, const glm::mat4& ModelMatrix) noexcept
    {
        if (!Texel.IsValid())
            return 0.0F;
        const glm::mat3 L(ModelMatrix);
        const glm::mat3 Cofactor(glm::cross(L[1], L[2]), glm::cross(L[2], L[0]), glm::cross(L[0], L[1]));
        const float     Area = glm::length(Cofactor * Texel.AreaVector);

        return std::isfinite(Area) && Area > 0.0F ? Area : 0.0F;
    }

    std::vector<float> BuildSurfaceGPUWorldTexelAreas(const FSharedSurfaceGeometryData& Geometry,
                                                      const glm::mat4&                  ModelMatrix)
    {
        std::vector<float> Areas;
        Areas.reserve(Geometry.GetTexelCount());

        for (const auto& Texel : Geometry.GetTexels())
            Areas.push_back(GetSurfaceWorldTexelArea(Texel, ModelMatrix));
        return Areas;
    }

    std::vector<float> BuildSurfaceGPUTransferWeights(const FSharedSurfaceGeometryData& Geometry,
                                                      const glm::mat4&                  ModelMatrix,
                                                      std::vector<FSurfaceGPUVec4>*     OutDebugAverages,
                                                      bool                              bUseNormalWeight,
                                                      bool                              bUseDistanceWeight,
                                                      bool                              bUseProfileBoundaryWeight)
    {
        const std::vector<FSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
        const std::vector<FSurfaceProfileIndex>&  Profiles = Geometry.GetProfileMap();
        const std::size_t                         TexelCount = Geometry.GetTexelCount();

        if (TexelCount == 0 || Profiles.size() != TexelCount)
        {
            throw std::invalid_argument("TransferWeight cache requires non-empty, profile-mapped Geometry.");
        }

        const glm::mat3        Linear(ModelMatrix);
        const float            Determinant = glm::determinant(Linear);
        const bool             bInvertible = std::isfinite(Determinant) && std::abs(Determinant) > GeometryEpsilon;
        const glm::mat3        NormalMatrix = bInvertible ? glm::transpose(glm::inverse(Linear)) : glm::mat3(0.0F);
        std::vector<glm::vec3> WorldPositions(TexelCount, glm::vec3(0.0F));
        std::vector<glm::vec3> WorldNormals(TexelCount, glm::vec3(0.0F));
        std::vector<float>     MeanNeighborDistances(TexelCount, 0.0F);
        std::vector<float>     Result(TexelCount * SurfaceNeighborCount, 0.0F);
        std::vector<std::array<float, 4>> DebugSums(TexelCount, {0.0F, 0.0F, 0.0F, 0.0F});
        std::vector<std::uint32_t>        DebugCounts(TexelCount, 0U);
        std::vector<bool>                 bValidPosition(TexelCount, false);
        std::vector<bool>                 bValidNormal(TexelCount, false);

        for (std::size_t Index = 0; Index < TexelCount; ++Index)
        {
            const FSurfaceTexelGeometry& Texel = Texels[Index];

            if (!Texel.IsValid() || Profiles[Index] == InvalidSurfaceProfileIndex)
            {
                continue;
            }

            const glm::vec3 EffectiveLocalPosition = Texel.Position + Texel.Normal * Texel.Geometry.MesoVirtualHeight;
            const glm::vec4 WorldPosition = ModelMatrix * glm::vec4(EffectiveLocalPosition, 1.0F);

            // NormalWeight cache도 복원된 Meso normal을 우선 사용한다.
            const glm::vec3 TransferNormal = Texel.bHasMesoNormal
                                                 ? Texel.MesoNormal
                                                 : (Texel.bHasTransferNormal ? Texel.TransferNormal : Texel.Normal);
            const glm::vec3 TransformedNormal = bInvertible ? NormalMatrix * TransferNormal : glm::vec3(0.0F);
            const float     NormalLength = glm::length(TransformedNormal);

            if (!IsFinite(EffectiveLocalPosition) || !IsFinite(glm::vec3(WorldPosition)))
            {
                continue;
            }

            WorldPositions[Index] = glm::vec3(WorldPosition);
            bValidPosition[Index] = true;

            if (std::isfinite(NormalLength) && NormalLength > GeometryEpsilon)
            {
                WorldNormals[Index] = TransformedNormal / NormalLength;
                bValidNormal[Index] = true;
            }
        }

        for (std::size_t Index = 0; Index < TexelCount; ++Index)
        {
            if (!bUseDistanceWeight || !bValidPosition[Index])
            {
                continue;
            }

            float         DistanceSum = 0.0F;
            std::uint32_t ValidDistanceCount = 0;

            for (const FLocalTexelIndex NeighborIndex : Texels[Index].NeighborIndices)
            {
                if (NeighborIndex == InvalidTexelIndex || NeighborIndex >= TexelCount || !bValidPosition[NeighborIndex])
                {
                    continue;
                }

                const float Distance = glm::length(WorldPositions[NeighborIndex] - WorldPositions[Index]);

                if (std::isfinite(Distance) && Distance > GeometryEpsilon)
                {
                    DistanceSum += Distance;
                    ++ValidDistanceCount;
                }
            }

            if (ValidDistanceCount > 0)
            {
                MeanNeighborDistances[Index] = DistanceSum / static_cast<float>(ValidDistanceCount);
            }
        }

        for (std::size_t Index = 0; Index < TexelCount; ++Index)
        {
            if (!bValidPosition[Index] || (bUseNormalWeight && !bValidNormal[Index]) ||
                (bUseDistanceWeight && MeanNeighborDistances[Index] <= GeometryEpsilon))
            {
                continue;
            }

            for (std::size_t DirectionIndex = 0; DirectionIndex < SurfaceNeighborCount; ++DirectionIndex)
            {
                const FLocalTexelIndex NeighborIndex = Texels[Index].NeighborIndices[DirectionIndex];

                if (NeighborIndex == InvalidTexelIndex || NeighborIndex >= TexelCount ||
                    !bValidPosition[NeighborIndex] || (bUseNormalWeight && !bValidNormal[NeighborIndex]) ||
                    (bUseDistanceWeight && MeanNeighborDistances[NeighborIndex] <= GeometryEpsilon))
                {
                    continue;
                }

                float DistanceWeight = 1.0F;

                if (bUseDistanceWeight)
                {
                    const float EdgeDistance = glm::length(WorldPositions[NeighborIndex] - WorldPositions[Index]);
                    const float ReferenceDistance =
                        0.5F * (MeanNeighborDistances[Index] + MeanNeighborDistances[NeighborIndex]);
                    if (!std::isfinite(EdgeDistance) || !std::isfinite(ReferenceDistance) ||
                        EdgeDistance <= GeometryEpsilon || ReferenceDistance <= GeometryEpsilon)
                    {
                        continue;
                    }

                    DistanceWeight = std::clamp(ReferenceDistance / EdgeDistance, 0.0F, 1.0F);
                }

                const float NormalWeight =
                    bUseNormalWeight
                        ? std::clamp(glm::dot(WorldNormals[Index], WorldNormals[NeighborIndex]), 0.0F, 1.0F)
                        : 1.0F;
                const float ProfileBoundaryWeight =
                    !bUseProfileBoundaryWeight || Profiles[Index] == Profiles[NeighborIndex] ? 1.0F : 0.5F;
                const float TransferWeight = DistanceWeight * NormalWeight * ProfileBoundaryWeight;
                Result[Index * SurfaceNeighborCount + DirectionIndex] = TransferWeight;
                DebugSums[Index][0] += TransferWeight;
                DebugSums[Index][1] += DistanceWeight;
                DebugSums[Index][2] += NormalWeight;
                DebugSums[Index][3] += ProfileBoundaryWeight;
                ++DebugCounts[Index];
            }
        }

        if (OutDebugAverages != nullptr)
        {
            OutDebugAverages->assign(TexelCount, {});

            for (std::size_t Index = 0; Index < TexelCount; ++Index)
            {
                if (DebugCounts[Index] == 0U)
                {
                    continue;
                }

                const float Count = static_cast<float>(DebugCounts[Index]);
                (*OutDebugAverages)[Index] = {DebugSums[Index][0] / Count,
                                              DebugSums[Index][1] / Count,
                                              DebugSums[Index][2] / Count,
                                              DebugSums[Index][3] / Count};
            }
        }

        return Result;
    }

#pragma endregion
}
