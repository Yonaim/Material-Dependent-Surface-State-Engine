/**
 * @file SurfaceMappingBuilder.cpp
 * @brief Mesh UV rasterization, chart neighbor 생성과 UV seam 연결.
 */

#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"

#include "SurfaceStateSystem/Mapping/SurfaceMappingValidation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace MDSS
{
    namespace
    {
        constexpr float UVEpsilon = 1.0e-6F;
        constexpr float RasterEdgeEpsilon = 1.0e-4F;
        constexpr float EdgeCandidateDistance = 0.8F;

        struct TEdgeKey
        {
            std::int32_t A = -1;
            std::int32_t B = -1;

            bool operator==(const TEdgeKey&) const noexcept = default;
        };

        struct TEdgeKeyHash
        {
            std::size_t operator()(const TEdgeKey& Key) const noexcept
            {
                return (static_cast<std::size_t>(static_cast<std::uint32_t>(Key.A)) << 32U) ^
                       static_cast<std::uint32_t>(Key.B);
            }
        };

        struct TEdgeIncident
        {
            std::uint32_t Triangle = InvalidTriangleID;
        };

        struct TEdgeRecord
        {
            TEdgeKey                   Key;
            std::vector<TEdgeIncident> Incidents;
        };

        class TDisjointSet
        {
        public:
            explicit TDisjointSet(std::size_t Count) : Parents(Count)
            {
                for (std::size_t Index = 0; Index < Count; ++Index)
                {
                    Parents[Index] = Index;
                }
            }

            std::size_t Find(std::size_t Value)
            {
                if (Parents[Value] != Value)
                {
                    Parents[Value] = Find(Parents[Value]);
                }
                return Parents[Value];
            }

            void Merge(std::size_t A, std::size_t B)
            {
                A = Find(A);
                B = Find(B);
                if (A != B)
                {
                    const std::size_t Root = std::min(A, B);
                    Parents[std::max(A, B)] = Root;
                }
            }

        private:
            std::vector<std::size_t> Parents;
        };

        float EdgeFunction(const glm::vec2& A, const glm::vec2& B, const glm::vec2& Point)
        {
            const glm::vec2 Edge = B - A;
            const glm::vec2 Offset = Point - A;
            return Edge.x * Offset.y - Edge.y * Offset.x;
        }

        bool IsTopLeft(const glm::vec2& A, const glm::vec2& B)
        {
            const glm::vec2 Edge = B - A;
            return Edge.y > RasterEdgeEpsilon || (std::abs(Edge.y) <= RasterEdgeEpsilon && Edge.x < 0.0F);
        }

        bool CoversPoint(const std::array<glm::vec2, 3>& InputUVs, const glm::vec2& Point)
        {
            std::array<glm::vec2, 3> UVs = InputUVs;
            if (EdgeFunction(UVs[0], UVs[1], UVs[2]) < 0.0F)
            {
                std::swap(UVs[1], UVs[2]);
            }

            for (std::size_t EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
            {
                const glm::vec2& A = UVs[EdgeIndex];
                const glm::vec2& B = UVs[(EdgeIndex + 1U) % 3U];
                const float      EdgeValue = EdgeFunction(A, B, Point);
                if (EdgeValue < -RasterEdgeEpsilon || (std::abs(EdgeValue) <= RasterEdgeEpsilon && !IsTopLeft(A, B)))
                {
                    return false;
                }
            }
            return true;
        }

        std::array<glm::vec2, 3> GetTriangleUVs(const std::vector<TVertex>& Vertices, const TMeshTriangleSource& Triangle)
        {
            return {
                Vertices[Triangle.RenderVertexIndices[0]].UV,
                Vertices[Triangle.RenderVertexIndices[1]].UV,
                Vertices[Triangle.RenderVertexIndices[2]].UV,
            };
        }

        glm::vec2 GetUVForOriginalPosition(const std::vector<TVertex>& Vertices,
                                           const TMeshTriangleSource&  Triangle,
                                           std::int32_t               OriginalPosition)
        {
            for (std::size_t Corner = 0; Corner < 3; ++Corner)
            {
                if (Triangle.OriginalPositionIndices[Corner] == OriginalPosition)
                {
                    return Vertices[Triangle.RenderVertexIndices[Corner]].UV;
                }
            }
            throw std::invalid_argument("Mesh edge endpoint is not present in its incident triangle.");
        }

        bool NearlyEqual(const glm::vec2& A, const glm::vec2& B)
        {
            return std::abs(A.x - B.x) <= UVEpsilon && std::abs(A.y - B.y) <= UVEpsilon;
        }

        bool HasMatchingUVEdge(const std::vector<TVertex>&             Vertices,
                               const std::vector<TMeshTriangleSource>& Triangles,
                               const TEdgeRecord&                      Edge)
        {
            const TMeshTriangleSource& First = Triangles[Edge.Incidents[0].Triangle];
            const TMeshTriangleSource& Second = Triangles[Edge.Incidents[1].Triangle];
            return NearlyEqual(GetUVForOriginalPosition(Vertices, First, Edge.Key.A),
                               GetUVForOriginalPosition(Vertices, Second, Edge.Key.A)) &&
                   NearlyEqual(GetUVForOriginalPosition(Vertices, First, Edge.Key.B),
                               GetUVForOriginalPosition(Vertices, Second, Edge.Key.B));
        }

        void AddNeighbor(TSurfaceMappingData& Mapping, TLocalTexelIndex Source, TLocalTexelIndex Target)
        {
            TSurfaceMappingTexel& Texel = Mapping.Texels[Source];
            for (const TLocalTexelIndex Existing : Texel.Neighbors)
            {
                if (Existing == Target)
                {
                    return;
                }
            }
            for (TLocalTexelIndex& Slot : Texel.Neighbors)
            {
                if (Slot == InvalidTexelIndex)
                {
                    Slot = Target;
                    return;
                }
            }
            throw std::runtime_error("Surface mapping texel exceeds the eight-neighbor limit.");
        }

        void AddBidirectionalNeighbor(TSurfaceMappingData& Mapping, TLocalTexelIndex A, TLocalTexelIndex B)
        {
            if (A == B)
            {
                return;
            }
            AddNeighbor(Mapping, A, B);
            AddNeighbor(Mapping, B, A);
        }

        const TSurfaceTexelRange& GetSurfaceRange(const TSurfaceMappingData& Mapping, TSurfaceLocalID Surface)
        {
            if (Surface >= Mapping.Surfaces.size() || Mapping.Surfaces[Surface].Surface != Surface)
            {
                throw std::invalid_argument("Triangle references an unknown Surface ID.");
            }
            return Mapping.Surfaces[Surface];
        }

        struct TSeamCandidate
        {
            TLocalTexelIndex Texel = InvalidTexelIndex;
            float           Parameter = 0.0F;
        };

        std::vector<TSeamCandidate> GatherSeamCandidates(const TSurfaceMappingData&  Mapping,
                                                        const std::vector<TVertex>& Vertices,
                                                        const TMeshTriangleSource&  Triangle,
                                                        std::uint32_t              TriangleIndex,
                                                        const TEdgeKey&             Edge)
        {
            const TSurfaceTexelRange& Range = GetSurfaceRange(Mapping, Triangle.Surface);
            const glm::vec2          EdgeStartUV = GetUVForOriginalPosition(Vertices, Triangle, Edge.A);
            const glm::vec2          EdgeEndUV = GetUVForOriginalPosition(Vertices, Triangle, Edge.B);
            const glm::vec2          Scale(static_cast<float>(Range.Resolution.Width),
                                  static_cast<float>(Range.Resolution.Height));
            const glm::vec2          EdgeStart = EdgeStartUV * Scale;
            const glm::vec2          EdgeEnd = EdgeEndUV * Scale;
            const glm::vec2          EdgeVector = EdgeEnd - EdgeStart;
            const float              EdgeLengthSquared = glm::dot(EdgeVector, EdgeVector);
            if (EdgeLengthSquared <= UVEpsilon)
            {
                throw std::invalid_argument("UV seam edge has zero length.");
            }

            std::vector<TSeamCandidate> Result;
            for (std::uint32_t Y = 0; Y < Range.Resolution.Height; ++Y)
            {
                for (std::uint32_t X = 0; X < Range.Resolution.Width; ++X)
                {
                    const TLocalTexelIndex Index = Range.FirstTexel + Y * Range.Resolution.Width + X;
                    if (Mapping.Texels[Index].Triangle != TriangleIndex)
                    {
                        continue;
                    }

                    const glm::vec2 Center(static_cast<float>(X) + 0.5F, static_cast<float>(Y) + 0.5F);
                    const float     Parameter =
                        std::clamp(glm::dot(Center - EdgeStart, EdgeVector) / EdgeLengthSquared, 0.0F, 1.0F);
                    const glm::vec2 Closest = EdgeStart + Parameter * EdgeVector;
                    if (glm::length(Center - Closest) <= EdgeCandidateDistance)
                    {
                        Result.push_back({Index, Parameter});
                    }
                }
            }
            std::ranges::sort(Result, {}, &TSeamCandidate::Parameter);
            return Result;
        }

        const TSeamCandidate& FindClosestCandidate(const std::vector<TSeamCandidate>& Candidates, float Parameter)
        {
            return *std::ranges::min_element(Candidates,
                                             {},
                                             [&](const TSeamCandidate& Candidate)
                                             { return std::abs(Candidate.Parameter - Parameter); });
        }
    } // namespace

    TSurfaceMappingData TSurfaceMappingBuilder::Build(const std::vector<TVertex>&             Vertices,
                                                    const std::vector<TMeshTriangleSource>& Triangles,
                                                    const std::vector<TSurfaceDefinition>&  Surfaces)
    {
        if (Vertices.empty() || Triangles.empty() || Surfaces.empty())
        {
            throw std::invalid_argument("Surface mapping requires vertices, triangles, and Surfaces.");
        }

        TSurfaceMappingData Mapping;
        std::uint64_t      FirstTexel = 0;
        for (std::size_t SurfaceIndex = 0; SurfaceIndex < Surfaces.size(); ++SurfaceIndex)
        {
            const TSurfaceDefinition& Surface = Surfaces[SurfaceIndex];
            if (Surface.ID != SurfaceIndex || Surface.ID == InvalidSurfaceID)
            {
                throw std::invalid_argument("Surface IDs must be dense and ordered from zero.");
            }
            const std::size_t TexelCount = Surface.Resolution.GetTexelCount();
            FirstTexel += TexelCount;
            if (FirstTexel > std::numeric_limits<TLocalTexelIndex>::max())
            {
                throw std::overflow_error("Surface mapping exceeds the local texel index range.");
            }
            Mapping.Surfaces.push_back({Surface.ID,
                                        Surface.Resolution,
                                        static_cast<TLocalTexelIndex>(FirstTexel - TexelCount),
                                        static_cast<TLocalTexelIndex>(TexelCount)});
        }
        Mapping.Texels.resize(static_cast<std::size_t>(FirstTexel));

        std::unordered_map<TEdgeKey, std::vector<TEdgeIncident>, TEdgeKeyHash> EdgeIncidents;
        for (std::size_t TriangleIndex = 0; TriangleIndex < Triangles.size(); ++TriangleIndex)
        {
            const TMeshTriangleSource& Triangle = Triangles[TriangleIndex];
            (void)GetSurfaceRange(Mapping, Triangle.Surface);

            for (std::size_t Corner = 0; Corner < 3; ++Corner)
            {
                if (Triangle.RenderVertexIndices[Corner] >= Vertices.size())
                {
                    throw std::invalid_argument("Triangle render vertex index is outside the vertex array.");
                }
                if (Triangle.OriginalPositionIndices[Corner] < 0)
                {
                    throw std::invalid_argument("Triangle is missing an original position index.");
                }
                if (Triangle.OriginalUVIndices[Corner] < 0)
                {
                    throw std::invalid_argument("Triangle is missing a Simulation UV coordinate.");
                }
            }
            const std::array<glm::vec2, 3> UVs = GetTriangleUVs(Vertices, Triangle);
            for (const glm::vec2& UV : UVs)
            {
                if (!std::isfinite(UV.x) || !std::isfinite(UV.y) || UV.x < 0.0F || UV.x > 1.0F || UV.y < 0.0F ||
                    UV.y > 1.0F)
                {
                    throw std::invalid_argument("Triangle UV must be finite and inside [0, 1].");
                }
            }
            if (std::abs(EdgeFunction(UVs[0], UVs[1], UVs[2])) <= UVEpsilon)
            {
                throw std::invalid_argument("Triangle UV area must be greater than epsilon.");
            }

            for (std::size_t Corner = 0; Corner < 3; ++Corner)
            {
                std::int32_t A = Triangle.OriginalPositionIndices[Corner];
                std::int32_t B = Triangle.OriginalPositionIndices[(Corner + 1U) % 3U];
                if (A == B)
                {
                    throw std::invalid_argument("Triangle contains a degenerate topology edge.");
                }
                if (A > B)
                {
                    std::swap(A, B);
                }
                std::vector<TEdgeIncident>& Incidents = EdgeIncidents[{A, B}];
                Incidents.push_back({static_cast<std::uint32_t>(TriangleIndex)});
                if (Incidents.size() > 2)
                {
                    throw std::invalid_argument("Mesh contains a non-manifold edge with more than two triangles.");
                }
            }
        }

        TDisjointSet             Charts(Triangles.size());
        std::vector<TEdgeRecord> SeamEdges;
        for (const auto& [Key, Incidents] : EdgeIncidents)
        {
            if (Incidents.size() != 2)
            {
                continue;
            }
            TEdgeRecord Edge{Key, Incidents};
            if (HasMatchingUVEdge(Vertices, Triangles, Edge))
            {
                Charts.Merge(Incidents[0].Triangle, Incidents[1].Triangle);
            }
            else
            {
                SeamEdges.push_back(std::move(Edge));
            }
        }
        std::ranges::sort(SeamEdges,
                          [](const TEdgeRecord& A, const TEdgeRecord& B)
                          { return A.Key.A != B.Key.A ? A.Key.A < B.Key.A : A.Key.B < B.Key.B; });

        std::unordered_map<std::size_t, std::uint32_t> ChartIDs;
        std::vector<std::uint32_t>                     TriangleCharts(Triangles.size(), InvalidChartID);
        for (std::size_t TriangleIndex = 0; TriangleIndex < Triangles.size(); ++TriangleIndex)
        {
            const std::size_t Root = Charts.Find(TriangleIndex);
            const auto [Found, Inserted] = ChartIDs.try_emplace(Root, static_cast<std::uint32_t>(ChartIDs.size()));
            (void)Inserted;
            TriangleCharts[TriangleIndex] = Found->second;
        }

        for (std::size_t TriangleIndex = 0; TriangleIndex < Triangles.size(); ++TriangleIndex)
        {
            const TMeshTriangleSource&      Triangle = Triangles[TriangleIndex];
            const TSurfaceTexelRange&       Range = GetSurfaceRange(Mapping, Triangle.Surface);
            const std::array<glm::vec2, 3> UVs = GetTriangleUVs(Vertices, Triangle);
            const glm::vec2                Scale(static_cast<float>(Range.Resolution.Width),
                                  static_cast<float>(Range.Resolution.Height));
            const std::array<glm::vec2, 3> TexelUVs = {UVs[0] * Scale, UVs[1] * Scale, UVs[2] * Scale};

            const float        MinX = std::min({TexelUVs[0].x, TexelUVs[1].x, TexelUVs[2].x});
            const float        MaxX = std::max({TexelUVs[0].x, TexelUVs[1].x, TexelUVs[2].x});
            const float        MinY = std::min({TexelUVs[0].y, TexelUVs[1].y, TexelUVs[2].y});
            const float        MaxY = std::max({TexelUVs[0].y, TexelUVs[1].y, TexelUVs[2].y});
            const std::int32_t StartX = std::max(0, static_cast<std::int32_t>(std::floor(MinX)));
            const std::int32_t EndX = std::min(static_cast<std::int32_t>(Range.Resolution.Width) - 1,
                                               static_cast<std::int32_t>(std::ceil(MaxX)));
            const std::int32_t StartY = std::max(0, static_cast<std::int32_t>(std::floor(MinY)));
            const std::int32_t EndY = std::min(static_cast<std::int32_t>(Range.Resolution.Height) - 1,
                                               static_cast<std::int32_t>(std::ceil(MaxY)));

            const float Denominator = EdgeFunction(UVs[1], UVs[2], UVs[0]);
            std::size_t CoveredTexels = 0;
            for (std::int32_t Y = StartY; Y <= EndY; ++Y)
            {
                for (std::int32_t X = StartX; X <= EndX; ++X)
                {
                    const glm::vec2 TexelCenter(static_cast<float>(X) + 0.5F, static_cast<float>(Y) + 0.5F);
                    if (!CoversPoint(TexelUVs, TexelCenter))
                    {
                        continue;
                    }

                    const TLocalTexelIndex MappingIndex = Range.FirstTexel +
                                                         static_cast<TLocalTexelIndex>(Y) * Range.Resolution.Width +
                                                         static_cast<TLocalTexelIndex>(X);
                    TSurfaceMappingTexel& Texel = Mapping.Texels[MappingIndex];
                    if (Texel.IsValid())
                    {
                        throw std::runtime_error("UV overlap at Surface " + std::to_string(Triangle.Surface) +
                                                 ", triangle " + std::to_string(TriangleIndex) + ", texel (" +
                                                 std::to_string(X) + ", " + std::to_string(Y) + "); already owned by " +
                                                 "triangle " + std::to_string(Texel.Triangle) + ".");
                    }

                    const glm::vec2 UVCenter = TexelCenter / Scale;
                    glm::vec3       Barycentric{
                        EdgeFunction(UVs[1], UVs[2], UVCenter) / Denominator,
                        EdgeFunction(UVs[2], UVs[0], UVCenter) / Denominator,
                        EdgeFunction(UVs[0], UVs[1], UVCenter) / Denominator,
                    };
                    const float BarycentricSum = Barycentric.x + Barycentric.y + Barycentric.z;
                    Barycentric /= BarycentricSum;

                    const TVertex&   V0 = Vertices[Triangle.RenderVertexIndices[0]];
                    const TVertex&   V1 = Vertices[Triangle.RenderVertexIndices[1]];
                    const TVertex&   V2 = Vertices[Triangle.RenderVertexIndices[2]];
                    const glm::vec3 Normal =
                        Barycentric.x * V0.Normal + Barycentric.y * V1.Normal + Barycentric.z * V2.Normal;

                    Texel.Surface = Triangle.Surface;
                    Texel.Triangle = static_cast<std::uint32_t>(TriangleIndex);
                    Texel.Chart = TriangleCharts[TriangleIndex];
                    Texel.Barycentric = Barycentric;
                    Texel.Position =
                        Barycentric.x * V0.Position + Barycentric.y * V1.Position + Barycentric.z * V2.Position;
                    Texel.Normal =
                        glm::dot(Normal, Normal) > UVEpsilon ? glm::normalize(Normal) : glm::vec3(0.0F, 0.0F, 1.0F);
                    ++CoveredTexels;
                }
            }

            if (CoveredTexels == 0)
            {
                Mapping.Warnings.push_back("Triangle " + std::to_string(TriangleIndex) + " on Surface " +
                                           std::to_string(Triangle.Surface) + " covers no texel centers.");
            }
        }

        constexpr std::array<std::array<std::int32_t, 2>, SurfaceNeighborCount> NeighborOffsets = {{
            {{-1, -1}},
            {{0, -1}},
            {{1, -1}},
            {{-1, 0}},
            {{1, 0}},
            {{-1, 1}},
            {{0, 1}},
            {{1, 1}},
        }};
        for (const TSurfaceTexelRange& Range : Mapping.Surfaces)
        {
            for (std::uint32_t Y = 0; Y < Range.Resolution.Height; ++Y)
            {
                for (std::uint32_t X = 0; X < Range.Resolution.Width; ++X)
                {
                    const TLocalTexelIndex Index = Range.FirstTexel + Y * Range.Resolution.Width + X;
                    TSurfaceMappingTexel&  Texel = Mapping.Texels[Index];
                    if (!Texel.IsValid())
                    {
                        continue;
                    }
                    for (const auto& Offset : NeighborOffsets)
                    {
                        const std::int32_t NeighborX = static_cast<std::int32_t>(X) + Offset[0];
                        const std::int32_t NeighborY = static_cast<std::int32_t>(Y) + Offset[1];
                        if (NeighborX < 0 || NeighborY < 0 ||
                            NeighborX >= static_cast<std::int32_t>(Range.Resolution.Width) ||
                            NeighborY >= static_cast<std::int32_t>(Range.Resolution.Height))
                        {
                            continue;
                        }
                        const TLocalTexelIndex Neighbor =
                            Range.FirstTexel + static_cast<TLocalTexelIndex>(NeighborY) * Range.Resolution.Width +
                            static_cast<TLocalTexelIndex>(NeighborX);
                        if (Mapping.Texels[Neighbor].IsValid() && Mapping.Texels[Neighbor].Chart == Texel.Chart)
                        {
                            AddNeighbor(Mapping, Index, Neighbor);
                        }
                    }
                }
            }
        }

        for (const TEdgeRecord& Seam : SeamEdges)
        {
            const std::uint32_t              FirstTriangle = Seam.Incidents[0].Triangle;
            const std::uint32_t              SecondTriangle = Seam.Incidents[1].Triangle;
            const std::vector<TSeamCandidate> FirstCandidates =
                GatherSeamCandidates(Mapping, Vertices, Triangles[FirstTriangle], FirstTriangle, Seam.Key);
            const std::vector<TSeamCandidate> SecondCandidates =
                GatherSeamCandidates(Mapping, Vertices, Triangles[SecondTriangle], SecondTriangle, Seam.Key);
            if (FirstCandidates.empty() || SecondCandidates.empty())
            {
                Mapping.Warnings.push_back("UV seam between triangles " + std::to_string(FirstTriangle) + " and " +
                                           std::to_string(SecondTriangle) + " has no texel candidates.");
                continue;
            }

            for (const TSeamCandidate& Candidate : FirstCandidates)
            {
                AddBidirectionalNeighbor(
                    Mapping, Candidate.Texel, FindClosestCandidate(SecondCandidates, Candidate.Parameter).Texel);
            }
            for (const TSeamCandidate& Candidate : SecondCandidates)
            {
                AddBidirectionalNeighbor(
                    Mapping, Candidate.Texel, FindClosestCandidate(FirstCandidates, Candidate.Parameter).Texel);
            }
        }

        ValidateSurfaceMapping(Mapping);
        return Mapping;
    }
} // namespace MDSS
