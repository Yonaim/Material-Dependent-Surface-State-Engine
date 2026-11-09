#include "SurfaceState/GPU/Packing/SurfaceGPUResourceLayout.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace MDSS::SurfaceState
{
    namespace
    {
        TSurfaceGPUVec4 ToGPUVec4(const glm::vec3& Value)
        {
            return {Value.x, Value.y, Value.z, 0.0F};
        }
    }
// ============================================================
// 00. Shared Geometry Upload
// ============================================================
#pragma region 00_Shared_Geometry_Upload

    TSurfaceGPUSharedGeometryUpload PackSharedSurfaceGeometry(const TSharedSurfaceGeometryData&     Geometry,
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
        Result.ReverseNeighborDirectionIndices.assign(TexelCount, UINT32_MAX);
        Result.TexelChartIndices.reserve(TexelCount);
        Result.SurfaceRanges.reserve(Geometry.GetSurfaces().size());
        for (const TSurfaceTexelRange& Range : Geometry.GetSurfaces())
        {
            Result.SurfaceRanges.push_back(
                {Range.FirstTexel, Range.Resolution.Width, Range.Resolution.Height, Range.TexelCount});
        }

        // The fourth MesoNormal component carries the highest Meso elevation in
        // the same surface/chart. A filled layer can then cover positive relief
        // as well as negative cavities without another GPU buffer or reduction.
        std::unordered_map<std::uint64_t, float> ChartMesoPeaks;
        for (const TSurfaceTexelGeometry& Texel : Geometry.GetTexels())
        {
            if (!Texel.IsValid() || !std::isfinite(Texel.Geometry.MesoVirtualHeight))
                continue;
            const std::uint64_t Key = (std::uint64_t{Texel.Surface} << 32U) | Texel.Chart;
            auto [It, Inserted] = ChartMesoPeaks.try_emplace(Key, Texel.Geometry.MesoVirtualHeight);
            if (!Inserted)
                It->second = std::max(It->second, Texel.Geometry.MesoVirtualHeight);
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
            TSurfaceGPUVec4 PackedMesoNormal = ToGPUVec4(MesoNormal);
            if (Texel.IsValid())
            {
                const std::uint64_t Key = (std::uint64_t{Texel.Surface} << 32U) | Texel.Chart;
                const auto          Peak = ChartMesoPeaks.find(Key);
                if (Peak != ChartMesoPeaks.end())
                    PackedMesoNormal.W = Peak->second;
            }
            Result.MesoNormals.push_back(PackedMesoNormal);
            Result.NeighborIndices.push_back({Texel.NeighborIndices});
            for (std::size_t DirectionIndex = 0; DirectionIndex < SurfaceNeighborCount; ++DirectionIndex)
            {
                const TLocalTexelIndex Neighbor = Texel.NeighborIndices[DirectionIndex];
                if (Neighbor >= TexelCount)
                {
                    continue;
                }
                const auto& NeighborTexelIndices = Geometry.GetTexels()[Neighbor].NeighborIndices;
                for (std::uint32_t ReverseDirectionIndex = 0; ReverseDirectionIndex < SurfaceNeighborCount;
                     ++ReverseDirectionIndex)
                {
                    if (NeighborTexelIndices[ReverseDirectionIndex] == TexelIndex)
                    {
                        // UV seam connections have no fixed opposite direction index.
                        const std::uint32_t Shift = static_cast<std::uint32_t>(DirectionIndex * 4U);
                        Result.ReverseNeighborDirectionIndices[TexelIndex] =
                            (Result.ReverseNeighborDirectionIndices[TexelIndex] & ~(0xfU << Shift)) |
                            (ReverseDirectionIndex << Shift);
                        break;
                    }
                }
            }
            Result.TexelChartIndices.push_back(Texel.Chart);
        }

        return Result;
    }

#pragma endregion
}