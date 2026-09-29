/**
 * @file SurfaceGPUResourceLayout.cpp
 * @brief 검증된 CPU Surface 데이터를 셰이더 입력 배열 형식으로 패킹한다.
 */

#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <limits>
#include <stdexcept>

namespace
{
    constexpr float GeometryEpsilon = 1.0e-6F;

    bool IsFinite(const glm::vec3& Value)
    {
        return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
    }
}

namespace MDSS
{
    namespace
    {
        TSurfaceGPUVec4 ToGPUVec4(const glm::vec3& Value)
        {
            return {Value.x, Value.y, Value.z, 0.0F};
        }
    } // 내부 네임스페이스

    TSurfaceGPUSharedGeometryUpload PackSharedSurfaceGeometry(
        const TSharedSurfaceGeometryData& Geometry,
        std::span<const TSurfaceProfileIndex> ProfileIndexRemap)
    {
        const std::size_t TexelCount = Geometry.GetTexelCount();
        if (TexelCount == 0)
        {
            throw std::invalid_argument("Cannot pack empty Surface Geometry.");
        }
        if (Geometry.GetProfileMap().size() != TexelCount)
        {
            throw std::invalid_argument("Surface Geometry Profile map size does not match its texel count.");
        }

        TSurfaceGPUSharedGeometryUpload Result;
        Result.TexelSurfaceIndices.reserve(TexelCount);
        Result.TexelProfileIndices = Geometry.GetProfileMap();
        if (!ProfileIndexRemap.empty())
        {
            for (TSurfaceProfileIndex& Index : Result.TexelProfileIndices)
            {
                if (Index == InvalidSurfaceProfileIndex)
                {
                    continue;
                }
                if (Index >= ProfileIndexRemap.size() || ProfileIndexRemap[Index] == InvalidSurfaceProfileIndex)
                {
                    throw std::invalid_argument("Surface Geometry references an unmapped Scene Profile.");
                }
                Index = ProfileIndexRemap[Index];
            }
        }
        Result.Positions.reserve(TexelCount);
        Result.Normals.reserve(TexelCount);
        Result.MesoNormals.reserve(TexelCount);
        Result.GeometryScalars.reserve(TexelCount);
        Result.NeighborIndices.reserve(TexelCount);
        static_assert(SurfaceNeighborCount == 8);
        Result.ReverseNeighborSlots.assign(TexelCount, UINT32_MAX);
        Result.TexelChartIndices.reserve(TexelCount);
        Result.SurfaceRanges.reserve(Geometry.GetSurfaces().size());
        for (const TSurfaceTexelRange& Range : Geometry.GetSurfaces())
        {
            Result.SurfaceRanges.push_back(
                {Range.FirstTexel, Range.Resolution.Width, Range.Resolution.Height, Range.TexelCount});
        }

        for (std::size_t TexelIndex = 0; TexelIndex < TexelCount; ++TexelIndex)
        {
            const TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[TexelIndex];
            Result.TexelSurfaceIndices.push_back(Texel.Surface);
            Result.Positions.push_back(ToGPUVec4(Texel.Position));
            Result.Normals.push_back(ToGPUVec4(Texel.Normal));
            Result.GeometryScalars.push_back({Texel.Geometry.MesoVirtualHeight,
                                              Texel.Geometry.ConcavityWeight,
                                              Texel.Geometry.MesoMeanCurvature,
                                              Texel.Geometry.MesoGaussianCurvature});
            // 높이에서 구한 normal이 없으면 샘플 Normal Map, 그것도 없으면 Macro normal을 올린다.
            const glm::vec3 MesoNormal = Texel.HasMesoNormal
                                             ? Texel.MesoNormal
                                             : (Texel.HasTransferNormal ? Texel.TransferNormal : Texel.Normal);
            Result.MesoNormals.push_back(ToGPUVec4(MesoNormal));
            Result.NeighborIndices.push_back({Texel.NeighborIndices});
            for (std::size_t Slot = 0; Slot < SurfaceNeighborCount; ++Slot)
            {
                const TLocalTexelIndex Neighbor = Texel.NeighborIndices[Slot];
                if (Neighbor >= TexelCount)
                {
                    continue;
                }
                const auto& NeighborSlots = Geometry.GetTexels()[Neighbor].NeighborIndices;
                for (std::uint32_t ReverseSlot = 0; ReverseSlot < SurfaceNeighborCount; ++ReverseSlot)
                {
                    if (NeighborSlots[ReverseSlot] == TexelIndex)
                    {
                        // UV seam connections have no fixed opposite direction slot.
                        const std::uint32_t Shift = static_cast<std::uint32_t>(Slot * 4U);
                        Result.ReverseNeighborSlots[TexelIndex] =
                            (Result.ReverseNeighborSlots[TexelIndex] & ~(0xfU << Shift)) | (ReverseSlot << Shift);
                        break;
                    }
                }
            }
            Result.TexelChartIndices.push_back(Texel.Chart);
        }

        return Result;
    }

    std::vector<float> BuildSurfaceGPUTransferWeights(const TSharedSurfaceGeometryData& Geometry,
                                                      const glm::mat4&                  ModelMatrix,
                                                      std::vector<TSurfaceGPUVec4>*     OutDebugAverages,
                                                      bool                             bUseNormalWeight,
                                                      bool                             bUseDistanceWeight,
                                                      bool                             bUseProfileBoundaryWeight,
                                                      bool                             bUseCurvatureWeight)
    {
        const std::vector<TSurfaceTexelGeometry>& Texels = Geometry.GetTexels();
        const std::vector<TSurfaceProfileIndex>&  Profiles = Geometry.GetProfileMap();
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
            const TSurfaceTexelGeometry& Texel = Texels[Index];
            if (!Texel.IsValid() || Profiles[Index] == InvalidSurfaceProfileIndex)
            {
                continue;
            }

            const glm::vec3 EffectiveLocalPosition = Texel.Position + Texel.Normal * Texel.Geometry.MesoVirtualHeight;
            const glm::vec4 WorldPosition = ModelMatrix * glm::vec4(EffectiveLocalPosition, 1.0F);
            // NormalWeight cache도 복원된 Meso normal을 우선 사용한다.
            const glm::vec3 TransferNormal = Texel.HasMesoNormal
                                                 ? Texel.MesoNormal
                                                 : (Texel.HasTransferNormal ? Texel.TransferNormal : Texel.Normal);
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
            for (const TLocalTexelIndex NeighborIndex : Texels[Index].NeighborIndices)
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
            for (std::size_t Slot = 0; Slot < SurfaceNeighborCount; ++Slot)
            {
                const TLocalTexelIndex NeighborIndex = Texels[Index].NeighborIndices[Slot];
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
                float CurvatureWeight = 1.0F;
                if (bUseCurvatureWeight)
                {
                    // H is inverse mesh-local length. Keep the edge length in the same metric.
                    const float SourceCurvature = Texels[Index].Geometry.MesoMeanCurvature;
                    const float TargetCurvature = Texels[NeighborIndex].Geometry.MesoMeanCurvature;
                    const glm::vec3 SourcePosition = Texels[Index].Position +
                        Texels[Index].Normal * Texels[Index].Geometry.MesoVirtualHeight;
                    const glm::vec3 TargetPosition = Texels[NeighborIndex].Position +
                        Texels[NeighborIndex].Normal * Texels[NeighborIndex].Geometry.MesoVirtualHeight;
                    const float Bend = (0.5F * std::abs(SourceCurvature) +
                                        0.5F * std::abs(TargetCurvature)) *
                                       glm::length(TargetPosition - SourcePosition);
                    CurvatureWeight = std::isfinite(Bend) && Bend >= 0.0F ? 1.0F / (1.0F + Bend) : 0.0F;
                }
                const float ProfileBoundaryWeight = !bUseProfileBoundaryWeight || Profiles[Index] == Profiles[NeighborIndex]
                                                        ? 1.0F
                                                        : 0.5F;
                const float TransferWeight = DistanceWeight * NormalWeight * CurvatureWeight * ProfileBoundaryWeight;
                Result[Index * SurfaceNeighborCount + Slot] = TransferWeight;
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

    TSurfaceGPUProfileUpload PackSurfaceProfiles(const std::vector<TSurfaceResponseProfileData>& Profiles,
                                                 const TSurfaceStateRegistry&                    Registry)
    {
        if (Profiles.empty())
        {
            throw std::invalid_argument("Cannot pack an empty Surface Profile table.");
        }
        const std::size_t ChannelCount = Registry.GetStateCount();
        if (ChannelCount == 0)
        {
            throw std::invalid_argument("Cannot pack Surface Profiles without registered State channels.");
        }
        if (Profiles.size() > std::numeric_limits<std::size_t>::max() / ChannelCount)
        {
            throw std::overflow_error("Surface Profile table element count overflowed.");
        }

        const std::size_t        RecordCount = Profiles.size() * ChannelCount;
        TSurfaceGPUProfileUpload Result;
        Result.Parameters.resize(RecordCount);
        Result.Supported.resize(RecordCount, 0U);

        for (std::size_t ProfileIndex = 0; ProfileIndex < Profiles.size(); ++ProfileIndex)
        {
            const TRegisteredSurfaceResponseProfileData Resolved = Registry.ResolveProfile(Profiles[ProfileIndex]);
            for (std::size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
            {
                const std::size_t RecordIndex = ProfileIndex * ChannelCount + ChannelIndex;
                if (!Resolved.States[ChannelIndex].has_value())
                {
                    continue;
                }

                const TSurfaceStateParameters& Parameters = *Resolved.States[ChannelIndex];
                Result.Parameters[RecordIndex] = {{Parameters.StateCapacity,
                                                   Parameters.InputFactor,
                                                   Parameters.SaturationTransferRate,
                                                   Parameters.GeometryTransferRate},
                                                  {Parameters.DecayRate,
                                                   Parameters.CavityRetentionFactor,
                                                   Parameters.AccumulationFactor,
                                                   Parameters.CavityFillFactor}};
                Result.Supported[RecordIndex] = 1U;
            }
        }

        return Result;
    }

    std::size_t GetSurfaceGPUStateValueIndex(std::size_t TexelIndex, std::size_t ChannelIndex, std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface State channel index is outside the Registry range.");
        }
        if (TexelIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface State scalar index overflowed.");
        }
        return TexelIndex * ChannelCount + ChannelIndex;
    }

    std::size_t
    GetSurfaceGPUProfileRecordIndex(std::size_t ProfileIndex, std::size_t ChannelIndex, std::size_t ChannelCount)
    {
        if (ChannelCount == 0 || ChannelIndex >= ChannelCount)
        {
            throw std::out_of_range("Surface Profile channel index is outside the Registry range.");
        }
        if (ProfileIndex > (std::numeric_limits<std::size_t>::max() - ChannelIndex) / ChannelCount)
        {
            throw std::overflow_error("Surface Profile record index overflowed.");
        }
        return ProfileIndex * ChannelCount + ChannelIndex;
    }

    std::size_t
    GetSurfaceGPUBufferByteSize(std::size_t ElementCount, std::size_t ElementStride, std::size_t MaxStorageBufferRange)
    {
        if (ElementCount == 0 || ElementStride == 0)
        {
            throw std::invalid_argument("Surface GPU buffers require non-zero element count and stride.");
        }
        if (ElementCount > std::numeric_limits<std::size_t>::max() / ElementStride)
        {
            throw std::overflow_error("Surface GPU buffer byte size overflowed.");
        }
        const std::size_t ByteSize = ElementCount * ElementStride;
        if (ByteSize > MaxStorageBufferRange)
        {
            throw std::length_error("Surface GPU buffer exceeds maxStorageBufferRange.");
        }
        return ByteSize;
    }
} // MDSS 네임스페이스
