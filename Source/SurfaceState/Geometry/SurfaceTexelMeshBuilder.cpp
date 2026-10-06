/**
 * @file SurfaceTexelMeshBuilder.cpp
 * @brief 원본 mesh topology를 보존하는 texel 기반 표시 mesh를 생성한다.
 */
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <delaunator.hpp>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace MDSS::SurfaceState
{
#pragma region Texel_Mesh_Construction_Helpers

    static void BuildBoundaryEdges(TSurfaceTexelMesh& Mesh, const std::vector<std::uint32_t>& TopologyIDs)
    {
        struct TEdgeOccurrence
        {
            glm::uvec4    Edge{0};
            std::uint32_t Count = 0;
        };
        std::map<std::pair<std::uint32_t, std::uint32_t>, TEdgeOccurrence> Edges;
        for (std::size_t I = 0; I + 2 < Mesh.Indices.size(); I += 3)
            for (std::size_t C = 0; C < 3; ++C)
            {
                const auto A = Mesh.Indices[I + C];
                const auto B = Mesh.Indices[I + (C + 1) % 3];
                const auto Other = Mesh.Indices[I + (C + 2) % 3];
                const auto Key = std::minmax(TopologyIDs[A], TopologyIDs[B]);
                auto&      Occurrence = Edges[{Key.first, Key.second}];
                if (Occurrence.Count++ == 0)
                    Occurrence.Edge = {A, B, Other, 0};
            }
        for (const auto& [Key, Occurrence] : Edges)
            if (Occurrence.Count == 1)
                Mesh.BoundaryEdges.push_back(Occurrence.Edge);
    }

    static TSurfaceTexelMesh SubdivideMesh(const TSurfaceTexelMesh& Source, std::uint32_t Levels)
    {
        TSurfaceTexelMesh Mesh = Source;
        for (std::uint32_t Level = 0; Level < Levels; ++Level)
        {
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> Midpoints;
            const auto Midpoint = [&](std::uint32_t A, std::uint32_t B)
            {
                const auto Key = std::minmax(A, B);
                if (const auto Found = Midpoints.find({Key.first, Key.second}); Found != Midpoints.end())
                    return Found->second;
                if (Mesh.Vertices.size() >= std::numeric_limits<std::uint32_t>::max())
                    throw std::overflow_error("Subdivided texel mesh exceeds the vertex index range.");
                const auto& VA = Mesh.Vertices[A];
                const auto& VB = Mesh.Vertices[B];
                TSurfaceTexelMeshVertex V;
                V.Position = (VA.Position + VB.Position) * 0.5F;
                V.Normal = (VA.Normal + VB.Normal) * 0.5F;
                const auto NormalLength = glm::length(glm::vec3(V.Normal));
                if (NormalLength > 1e-8F)
                    V.Normal = glm::vec4(glm::vec3(V.Normal) / NormalLength, 0.0F);
                V.DisplacementNormal = (VA.DisplacementNormal + VB.DisplacementNormal) * 0.5F;
                const auto DisplacementLength = glm::length(glm::vec3(V.DisplacementNormal));
                if (DisplacementLength > 1e-8F)
                    V.DisplacementNormal = glm::vec4(glm::vec3(V.DisplacementNormal) / DisplacementLength, 0.0F);
                V.UVSurface = (VA.UVSurface + VB.UVSurface) * 0.5F;
                std::map<std::uint32_t, float> SampleWeights;
                for (std::size_t Slot = 0; Slot < 4; ++Slot)
                {
                    if (VA.Samples[Slot] != InvalidTexelIndex && VA.Weights[Slot] > 0.0F)
                        SampleWeights[VA.Samples[Slot]] += 0.5F * VA.Weights[Slot];
                    if (VB.Samples[Slot] != InvalidTexelIndex && VB.Weights[Slot] > 0.0F)
                        SampleWeights[VB.Samples[Slot]] += 0.5F * VB.Weights[Slot];
                }
                std::vector<std::pair<std::uint32_t, float>> OrderedSamples(SampleWeights.begin(), SampleWeights.end());
                std::sort(OrderedSamples.begin(), OrderedSamples.end(), [](const auto& A, const auto& B)
                          { return A.second > B.second; });
                float WeightSum = 0.0F;
                for (std::size_t Slot = 0; Slot < std::min<std::size_t>(4, OrderedSamples.size()); ++Slot)
                    WeightSum += OrderedSamples[Slot].second;
                if (WeightSum > 0.0F)
                    for (std::size_t Slot = 0; Slot < std::min<std::size_t>(4, OrderedSamples.size()); ++Slot)
                    {
                        V.Samples[Slot] = OrderedSamples[Slot].first;
                        V.Weights[Slot] = OrderedSamples[Slot].second / WeightSum;
                    }
                const auto Index = static_cast<std::uint32_t>(Mesh.Vertices.size());
                Mesh.Vertices.push_back(V);
                Midpoints.emplace(std::make_pair(Key.first, Key.second), Index);
                return Index;
            };

            std::vector<std::uint32_t> RefinedIndices;
            RefinedIndices.reserve(Mesh.Indices.size() * 4U);
            for (auto& Range : Mesh.Surfaces)
            {
                const auto First = static_cast<std::uint32_t>(RefinedIndices.size());
                const auto End = static_cast<std::size_t>(Range.FirstIndex) + Range.IndexCount;
                for (std::size_t I = Range.FirstIndex; I + 2 < End; I += 3)
                {
                    const auto A = Mesh.Indices[I];
                    const auto B = Mesh.Indices[I + 1];
                    const auto C = Mesh.Indices[I + 2];
                    const auto AB = Midpoint(A, B);
                    const auto BC = Midpoint(B, C);
                    const auto CA = Midpoint(C, A);
                    RefinedIndices.insert(RefinedIndices.end(),
                                          {A, AB, CA, AB, B, BC, CA, BC, C, AB, BC, CA});
                }
                Range = {First, static_cast<std::uint32_t>(RefinedIndices.size()) - First};
            }

            std::vector<glm::uvec4> RefinedBoundaries;
            RefinedBoundaries.reserve(Mesh.BoundaryEdges.size() * 2U);
            for (const auto& Edge : Mesh.BoundaryEdges)
            {
                const auto AB = Midpoint(Edge.x, Edge.y);
                const auto AC = Midpoint(Edge.x, Edge.z);
                const auto BC = Midpoint(Edge.y, Edge.z);
                RefinedBoundaries.push_back({Edge.x, AB, AC, 0U});
                RefinedBoundaries.push_back({AB, Edge.y, BC, 0U});
            }
            Mesh.Indices = std::move(RefinedIndices);
            Mesh.BoundaryEdges = std::move(RefinedBoundaries);
        }
        return Mesh;
    }

    static TSurfaceTexelMesh BuildGridMesh(const TSharedSurfaceGeometryData& Geometry, std::uint32_t RenderResolution)
    {
        TSurfaceTexelMesh Result;
        const auto&       Texels = Geometry.GetTexels();
        std::vector<std::uint32_t> VertexCharts;
        for (const auto& Range : Geometry.GetSurfaces())
        {
            const std::uint32_t Step = RenderResolution > 0 && RenderResolution < Range.Resolution.Width
                                           ? std::max(1U, Range.Resolution.Width / RenderResolution)
                                           : 1U;
            const std::uint32_t Offset = Step > 1 ? Step / 2U : 0U;
            const std::uint32_t First = static_cast<std::uint32_t>(Result.Indices.size());
            const std::uint32_t GridWidth = Range.Resolution.Width > Offset
                                                ? (Range.Resolution.Width - Offset + Step - 1U) / Step
                                                : 0U;
            const std::uint32_t GridHeight = Range.Resolution.Height > Offset
                                                 ? (Range.Resolution.Height - Offset + Step - 1U) / Step
                                                 : 0U;
            std::vector<std::uint32_t> GridVertices(static_cast<std::size_t>(GridWidth) * GridHeight,
                                                    InvalidTexelIndex);
            for (std::uint32_t Y = 0; Y < GridHeight; ++Y)
                for (std::uint32_t X = 0; X < GridWidth; ++X)
                {
                    const std::uint32_t TexelX = Offset + X * Step;
                    const std::uint32_t TexelY = Offset + Y * Step;
                    if (TexelX >= Range.Resolution.Width || TexelY >= Range.Resolution.Height)
                        continue;
                    const std::uint32_t T = Range.FirstTexel + TexelY * Range.Resolution.Width + TexelX;
                    const auto& Texel = Texels[T];
                    if (!Texel.IsValid() || !std::isfinite(Texel.Position.x) || !std::isfinite(Texel.Position.y) ||
                        !std::isfinite(Texel.Position.z) || !std::isfinite(Texel.Normal.x) ||
                        !std::isfinite(Texel.Normal.y) || !std::isfinite(Texel.Normal.z))
                        continue;
                    TSurfaceTexelMeshVertex V;
                    V.Position = glm::vec4(Texel.Position, 1);
                    V.Normal = V.DisplacementNormal = glm::vec4(Texel.Normal, 0);
                    V.UVSurface = {(float(TexelX) + 0.5F) / Range.Resolution.Width,
                                   (float(TexelY) + 0.5F) / Range.Resolution.Height,
                                   0,
                                   float(Range.Surface)};
                    V.Samples.x = T;
                    V.Weights.x = 1;
                    const auto GridIndex = static_cast<std::size_t>(Y) * GridWidth + X;
                    GridVertices[GridIndex] = static_cast<std::uint32_t>(Result.Vertices.size());
                    Result.Vertices.push_back(V);
                    VertexCharts.push_back(Texel.Chart);
                }
            const auto Emit = [&](std::uint32_t A, std::uint32_t B, std::uint32_t C)
            {
                if (A == InvalidTexelIndex || B == InvalidTexelIndex || C == InvalidTexelIndex)
                    return;
                if (VertexCharts[A] != VertexCharts[B] || VertexCharts[A] != VertexCharts[C])
                    return;
                const auto P0 = glm::vec3(Result.Vertices[A].Position);
                const auto P1 = glm::vec3(Result.Vertices[B].Position);
                const auto P2 = glm::vec3(Result.Vertices[C].Position);
                const auto Cross = glm::cross(P1 - P0, P2 - P0);
                if (glm::dot(Cross, Cross) <= 1e-16F)
                    return;
                const auto Normal = glm::vec3(Result.Vertices[A].Normal) + glm::vec3(Result.Vertices[B].Normal) +
                                    glm::vec3(Result.Vertices[C].Normal);
                if (glm::dot(Cross, Normal) < 0.0F)
                    std::swap(B, C);
                if (Result.Indices.size() > std::numeric_limits<std::uint32_t>::max() - 3U)
                    throw std::overflow_error("Texel mesh exceeds the Vulkan index-count range.");
                Result.Indices.insert(Result.Indices.end(), {A, B, C});
            };
            for (std::uint32_t Y = 0; Y + 1 < GridHeight; ++Y)
                for (std::uint32_t X = 0; X + 1 < GridWidth; ++X)
                {
                    const auto A = GridVertices[static_cast<std::size_t>(Y) * GridWidth + X];
                    const auto B = GridVertices[static_cast<std::size_t>(Y) * GridWidth + X + 1U];
                    const auto D = GridVertices[static_cast<std::size_t>(Y + 1U) * GridWidth + X];
                    const auto C = GridVertices[static_cast<std::size_t>(Y + 1U) * GridWidth + X + 1U];
                    const auto Compatible = [&](std::uint32_t I, std::uint32_t J, std::uint32_t K)
                    {
                        return I != InvalidTexelIndex && J != InvalidTexelIndex && K != InvalidTexelIndex &&
                               VertexCharts[I] == VertexCharts[J] && VertexCharts[I] == VertexCharts[K];
                    };
                    const int ACCount = int(Compatible(A, B, C)) + int(Compatible(A, C, D));
                    const int BDCount = int(Compatible(A, B, D)) + int(Compatible(B, C, D));
                    if (BDCount > ACCount)
                    {
                        Emit(A, B, D);
                        Emit(B, C, D);
                    }
                    else
                    {
                        Emit(A, B, C);
                        Emit(A, C, D);
                    }
                }
            Result.Surfaces.push_back({First, static_cast<std::uint32_t>(Result.Indices.size()) - First});
        }
        std::vector<std::uint32_t> TopologyIDs(Result.Vertices.size());
        for (std::uint32_t I = 0; I < TopologyIDs.size(); ++I)
            TopologyIDs[I] = I;
        BuildBoundaryEdges(Result, TopologyIDs);
        return Result;
    }

#pragma endregion

#pragma region Surface_Texel_Mesh_Build

    TSurfaceTexelMesh BuildSurfaceTexelMesh(const TSharedSurfaceGeometryData&           Geometry,
                                            std::span<const Asset::TVertex>             Vertices,
                                            std::span<const Asset::TMeshTriangleSource> Triangles,
                                            std::uint32_t                                RenderResolution)
    {
        if (Vertices.empty() && Triangles.empty())
        {
            auto Mesh = BuildGridMesh(Geometry, RenderResolution);
            if (RenderResolution > 0 && !Geometry.GetSurfaces().empty())
            {
                std::uint32_t SourceResolution = Geometry.GetSurfaces().front().Resolution.Width;
                std::uint32_t Levels = 0;
                while (SourceResolution > 0 && SourceResolution <= RenderResolution / 2U)
                {
                    SourceResolution *= 2U;
                    ++Levels;
                }
                if (Levels > 0)
                    Mesh = SubdivideMesh(Mesh, Levels);
            }
            return Mesh;
        }
        if (Vertices.empty() || Triangles.empty())
            throw std::invalid_argument("Texel mesh requires both source vertices and triangles.");
        const auto&                             Texels = Geometry.GetTexels();
        const auto IsSelectedRenderTexel = [&](std::uint32_t TexelIndex)
        {
            if (RenderResolution == 0)
                return true;
            const auto& Surface = Geometry.GetSurface(Texels[TexelIndex].Surface);
            if (RenderResolution >= Surface.Resolution.Width)
                return true;
            const std::uint32_t Step = std::max(1U, Surface.Resolution.Width / RenderResolution);
            const std::uint32_t X = (TexelIndex - Surface.FirstTexel) % Surface.Resolution.Width;
            const std::uint32_t Y = (TexelIndex - Surface.FirstTexel) / Surface.Resolution.Width;
            const std::uint32_t CenterOffset = Step / 2U;
            return X % Step == CenterOffset && Y % Step == CenterOffset;
        };
        std::vector<std::vector<std::uint32_t>> TriangleTexels(Triangles.size());
        for (std::uint32_t T = 0; T < Texels.size(); ++T)
            if (Texels[T].IsValid())
            {
                if (Texels[T].Triangle >= Triangles.size() ||
                    Texels[T].Surface != Triangles[Texels[T].Triangle].Surface)
                    throw std::invalid_argument("Texel does not match source triangle topology.");
                TriangleTexels[Texels[T].Triangle].push_back(T);
            }

        // Original position identity, never UV proximity, defines the weld. Distinct components
        // and genuine source holes remain distinct even when their UVs or positions coincide.
        struct TWeld
        {
            glm::vec3                  Normal{0};
            glm::vec3                  Position{0};
            glm::uvec4                 Samples{InvalidTexelIndex};
            std::array<float, 4>       Distances{INFINITY, INFINITY, INFINITY, INFINITY};
            std::vector<std::uint32_t> Adjacent;
        };
        std::unordered_map<std::int32_t, std::uint32_t> WeldIndices;
        std::vector<TWeld>                              Welds;
        std::vector<std::array<std::uint32_t, 3>>       TriangleWelds(Triangles.size());
        for (std::size_t I = 0; I < Triangles.size(); ++I)
        {
            const auto& Triangle = Triangles[I];
            (void)Geometry.GetSurface(Triangle.Surface);
            for (std::size_t C = 0; C < 3; ++C)
            {
                if (Triangle.RenderVertexIndices[C] >= Vertices.size() || Triangle.OriginalPositionIndices[C] < 0)
                    throw std::invalid_argument("Invalid source vertex in texel mesh topology.");
                const auto [It, Added] = WeldIndices.try_emplace(Triangle.OriginalPositionIndices[C], Welds.size());
                if (Added)
                    Welds.emplace_back();
                const auto W = It->second;
                TriangleWelds[I][C] = W;
                auto&       Weld = Welds[W];
                const auto& V = Vertices[Triangle.RenderVertexIndices[C]];
                Weld.Normal += V.Normal;
                Weld.Position = V.Position;
                for (const auto T : TriangleTexels[I])
                {
                    if (Weld.Samples.x == T || Weld.Samples.y == T || Weld.Samples.z == T || Weld.Samples.w == T)
                        continue;
                    const auto  Delta = Texels[T].Position - V.Position;
                    const float D = glm::dot(Delta, Delta);
                    for (std::size_t Slot = 0; Slot < 4; ++Slot)
                        if (D < Weld.Distances[Slot])
                        {
                            for (std::size_t K = 3; K > Slot; --K)
                            {
                                Weld.Distances[K] = Weld.Distances[K - 1];
                                Weld.Samples[K] = Weld.Samples[K - 1];
                            }
                            Weld.Distances[Slot] = D;
                            Weld.Samples[Slot] = T;
                            break;
                        }
                }
            }
            for (std::size_t C = 0; C < 3; ++C)
            {
                const auto A = TriangleWelds[I][C], B = TriangleWelds[I][(C + 1) % 3];
                Welds[A].Adjacent.push_back(B);
                Welds[B].Adjacent.push_back(A);
            }
        }
        // Fine scan triangles may have no texel centers. Borrow a stencil through source
        // topology, without a global nearest-neighbor search that could cross disconnected parts.
        std::queue<std::uint32_t> Pending;
        for (std::uint32_t W = 0; W < Welds.size(); ++W)
            if (Welds[W].Samples.x != InvalidTexelIndex)
                Pending.push(W);
        while (!Pending.empty())
        {
            const auto W = Pending.front();
            Pending.pop();
            for (const auto Other : Welds[W].Adjacent)
                if (Welds[Other].Samples.x == InvalidTexelIndex)
                {
                    Welds[Other].Samples = Welds[W].Samples;
                    Welds[Other].Distances = Welds[W].Distances;
                    Pending.push(Other);
                }
        }
        for (auto& Weld : Welds)
        {
            const float Length = glm::length(Weld.Normal);
            Weld.Normal = Length > 1e-8F ? Weld.Normal / Length : glm::vec3(0, 0, 1);
        }
        struct TEdgePoint
        {
            float      Parameter;
            glm::uvec4 Samples{InvalidTexelIndex};
            glm::vec4  Weights{0};
            glm::vec4  Position{0}, Direction{0};
        };
        using TEdgeKey = std::pair<std::uint32_t, std::uint32_t>;
        std::map<TEdgeKey, std::vector<std::pair<float, std::uint32_t>>> EdgeSamples;
        std::map<TEdgeKey, std::vector<TEdgePoint>>                      EdgePoints;
        std::map<TEdgeKey, std::vector<std::uint32_t>>                   EdgeTopologyIDs;
        // Union exact edge samples from both charts before refinement. Both incident source
        // triangles receive identical subdivisions, so edge samples cannot form T-junctions.
        for (std::size_t I = 0; I < Triangles.size(); ++I)
            for (const auto T : TriangleTexels[I])
            {
                if (!IsSelectedRenderTexel(T))
                    continue;
                const auto& Bary = Texels[T].Barycentric;
                const auto  Opposite = Bary.x < Bary.y ? (Bary.x < Bary.z ? 0U : 2U) : (Bary.y < Bary.z ? 1U : 2U);
                if (Bary[Opposite] > 1e-6F)
                    continue;
                auto A = (Opposite + 1U) % 3U, B = (Opposite + 2U) % 3U;
                if (TriangleWelds[I][A] > TriangleWelds[I][B])
                    std::swap(A, B);
                const float Sum = Bary[A] + Bary[B];
                const float Parameter = Sum > 0 ? Bary[B] / Sum : 0;
                if (Parameter <= 1e-6F || Parameter >= 1.0F - 1e-6F)
                    continue;
                EdgeSamples[{TriangleWelds[I][A], TriangleWelds[I][B]}].push_back({Parameter, T});
            }
        std::uint32_t NextTopologyID = static_cast<std::uint32_t>(Welds.size());
        for (auto& [Key, Samples] : EdgeSamples)
        {
            std::sort(Samples.begin(), Samples.end());
            auto& Points = EdgePoints[Key];
            for (std::size_t K = 0; K < Samples.size();)
            {
                TEdgePoint Point;
                Point.Parameter = Samples[K].first;
                std::size_t End = K + 1;
                while (End < Samples.size() && Samples[End].first - Point.Parameter <= 1e-6F)
                    ++End;
                for (std::size_t J = K; J < End && J < K + 4; ++J)
                {
                    Point.Samples[J - K] = Samples[J].second;
                    Point.Weights[J - K] = 1.0F / float(std::min<std::size_t>(End - K, 4));
                }
                Point.Position =
                    glm::vec4(glm::mix(Welds[Key.first].Position, Welds[Key.second].Position, Point.Parameter), 1);
                auto N = glm::mix(Welds[Key.first].Normal, Welds[Key.second].Normal, Point.Parameter);
                Point.Direction = glm::vec4(glm::dot(N, N) > 1e-12F ? glm::normalize(N) : Welds[Key.first].Normal, 0);
                Points.push_back(Point);
                EdgeTopologyIDs[Key].push_back(NextTopologyID++);
                K = End;
            }
        }
        TSurfaceTexelMesh          Result;
        std::vector<std::uint32_t> TopologyIDs;
        // Render vertices stay split at UV/material/normal seams; their displacement is welded.
        Result.Vertices.reserve(Vertices.size() + Texels.size());
        std::unordered_map<std::uint64_t, std::uint32_t> RenderIndices;
        std::vector<std::array<std::uint32_t, 3>>        TriangleRenderIndices(Triangles.size());
        for (std::size_t I = 0; I < Triangles.size(); ++I)
            for (std::size_t C = 0; C < 3; ++C)
            {
                const auto R = Triangles[I].RenderVertexIndices[C];
                const auto Key = (std::uint64_t(Triangles[I].Surface) << 32U) | R;
                const auto [It, Added] = RenderIndices.try_emplace(Key, Result.Vertices.size());
                TriangleRenderIndices[I][C] = It->second;
                if (!Added)
                    continue;
                Result.Vertices.emplace_back();
                TopologyIDs.push_back(TriangleWelds[I][C]);
                const auto& Source = Vertices[R];
                const auto& Weld = Welds[TriangleWelds[I][C]];
                auto&       V = Result.Vertices.back();
                V.Position = glm::vec4(Source.Position, 1);
                V.Normal = glm::vec4(Source.Normal, 0);
                V.DisplacementNormal = glm::vec4(Weld.Normal, 0);
                V.UVSurface = {Source.UV.x, Source.UV.y, 0, float(Triangles[I].Surface)};
                V.Samples = Weld.Samples;
                for (std::size_t K = 0; K < 4; ++K)
                    if (V.Samples[K] != InvalidTexelIndex)
                        V.Weights[K] = 1.0F / std::max(Weld.Distances[K], 1e-12F);
                const float Sum = V.Weights.x + V.Weights.y + V.Weights.z + V.Weights.w;
                if (Sum > 0)
                    V.Weights /= Sum;
            }
        std::vector<std::vector<std::uint32_t>> SurfaceIndices(Geometry.GetSurfaces().size());
        for (std::size_t I = 0; I < Triangles.size(); ++I)
        {
            const auto&                Source = Triangles[I];
            std::vector<std::uint32_t> Points(TriangleRenderIndices[I].begin(), TriangleRenderIndices[I].end());
            // Barycentric coordinates avoid UV scale, atlas rotation and mirrored winding issues.
            std::vector<double> Coordinates{0, 0, 1, 0, 0, 1};
            for (std::size_t Corner = 0; Corner < 3; ++Corner)
            {
                auto A = Corner, B = (Corner + 1) % 3;
                if (TriangleWelds[I][A] > TriangleWelds[I][B])
                    std::swap(A, B);
                const auto Found = EdgePoints.find({TriangleWelds[I][A], TriangleWelds[I][B]});
                if (Found == EdgePoints.end())
                    continue;
                for (std::size_t PointIndex = 0; PointIndex < Found->second.size(); ++PointIndex)
                {
                    const auto&             Point = Found->second[PointIndex];
                    TSurfaceTexelMeshVertex V;
                    V.Position = Point.Position;
                    V.DisplacementNormal = Point.Direction;
                    V.Normal = glm::vec4(glm::normalize(glm::mix(Vertices[Source.RenderVertexIndices[A]].Normal,
                                                                 Vertices[Source.RenderVertexIndices[B]].Normal,
                                                                 Point.Parameter)),
                                         0);
                    const auto UV = glm::mix(Vertices[Source.RenderVertexIndices[A]].UV,
                                             Vertices[Source.RenderVertexIndices[B]].UV,
                                             Point.Parameter);
                    V.UVSurface = {UV.x, UV.y, 0, float(Source.Surface)};
                    V.Samples = Point.Samples;
                    V.Weights = Point.Weights;
                    Points.push_back(static_cast<std::uint32_t>(Result.Vertices.size()));
                    Result.Vertices.push_back(V);
                    TopologyIDs.push_back(EdgeTopologyIDs.at({TriangleWelds[I][A], TriangleWelds[I][B]})[PointIndex]);
                    glm::dvec3 Bary{0};
                    Bary[A] = 1.0 - double(Point.Parameter);
                    Bary[B] = double(Point.Parameter);
                    Coordinates.push_back(Bary.y);
                    Coordinates.push_back(Bary.z);
                }
            }
            for (const auto T : TriangleTexels[I])
            {
                const auto& Texel = Texels[T];
                // Boundary samples are represented by the common source edge vertices above.
                if (std::min({Texel.Barycentric.x, Texel.Barycentric.y, Texel.Barycentric.z}) <= 1e-6F)
                    continue;
                if (!IsSelectedRenderTexel(T))
                    continue;
                if (Result.Vertices.size() >= std::numeric_limits<std::uint32_t>::max())
                    throw std::overflow_error("Texel render vertex count exceeds index range.");
                Points.push_back(static_cast<std::uint32_t>(Result.Vertices.size()));
                TSurfaceTexelMeshVertex V;
                V.Position = glm::vec4(Texel.Position, 1);
                V.Normal = V.DisplacementNormal = glm::vec4(Texel.Normal, 0);
                glm::vec2 UV{0};
                for (std::size_t C = 0; C < 3; ++C)
                    UV += Texel.Barycentric[C] * Vertices[Source.RenderVertexIndices[C]].UV;
                V.UVSurface = {UV.x, UV.y, 0, float(Source.Surface)};
                V.Samples.x = T;
                V.Weights.x = 1;
                Result.Vertices.push_back(V);
                TopologyIDs.push_back(NextTopologyID++);
                Coordinates.push_back(Texel.Barycentric.y);
                Coordinates.push_back(Texel.Barycentric.z);
            }
            auto&      Indices = SurfaceIndices[Source.Surface];
            const auto Emit = [&](std::uint32_t A, std::uint32_t B, std::uint32_t C)
            {
                const glm::vec3 N = glm::cross(Vertices[Source.RenderVertexIndices[1]].Position -
                                                   Vertices[Source.RenderVertexIndices[0]].Position,
                                               Vertices[Source.RenderVertexIndices[2]].Position -
                                                   Vertices[Source.RenderVertexIndices[0]].Position);
                const auto      Cross = glm::cross(glm::vec3(Result.Vertices[B].Position - Result.Vertices[A].Position),
                                              glm::vec3(Result.Vertices[C].Position - Result.Vertices[A].Position));
                if (glm::dot(N, Cross) < 0)
                    std::swap(B, C);
                Indices.insert(Indices.end(), {A, B, C});
            };
            if (Points.size() == 3)
                Emit(Points[0], Points[1], Points[2]);
            else
            {
                const delaunator::Delaunator Refined(Coordinates);
                for (std::size_t K = 0; K < Refined.triangles.size(); K += 3)
                    Emit(Points[Refined.triangles[K]],
                         Points[Refined.triangles[K + 1]],
                         Points[Refined.triangles[K + 2]]);
            }
        }
        for (const auto& Indices : SurfaceIndices)
        {
            if (Result.Indices.size() + Indices.size() > std::numeric_limits<std::uint32_t>::max())
                throw std::overflow_error("Texel mesh exceeds index-count range.");
            Result.Surfaces.push_back(
                {static_cast<std::uint32_t>(Result.Indices.size()), static_cast<std::uint32_t>(Indices.size())});
            Result.Indices.insert(Result.Indices.end(), Indices.begin(), Indices.end());
        }
        BuildBoundaryEdges(Result, TopologyIDs);
        if (RenderResolution > 0 && !Geometry.GetSurfaces().empty())
        {
            std::uint32_t SourceResolution = Geometry.GetSurfaces().front().Resolution.Width;
            std::uint32_t Levels = 0;
            while (SourceResolution > 0 && SourceResolution <= RenderResolution / 2U)
            {
                SourceResolution *= 2U;
                ++Levels;
            }
            if (Levels > 0)
                Result = SubdivideMesh(Result, Levels);
        }
        return Result;
    }
#pragma endregion
}
