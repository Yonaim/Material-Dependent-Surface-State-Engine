/**
 * @file SharedSurfaceGeometryTests.cpp
 * @brief Shared geometry construction and Runtime preprocessing tests.
 */

#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    int FailureCount = 0;

    void Check(bool Condition, const std::string& Message)
    {
        if (!Condition)
        {
            ++FailureCount;
            std::cerr << "FAIL: " << Message << '\n';
        }
    }

    template <typename TFunction>
    void CheckThrows(TFunction&& Function, const std::string& ExpectedMessage, const std::string& TestName)
    {
        try
        {
            std::invoke(std::forward<TFunction>(Function));
            Check(false, TestName + " did not throw");
        }
        catch (const std::exception& Exception)
        {
            Check(std::string(Exception.what()).find(ExpectedMessage) != std::string::npos,
                  TestName + " did not include expected diagnostic '" + ExpectedMessage + "'");
        }
    }

    std::filesystem::path Fixture(const char* RelativePath)
    {
        return std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / RelativePath;
    }

    MDSS::TSurfaceMappingData BuildQuad()
    {
        const MDSS::TOBJLoadResult Mesh = MDSS::TOBJLoader::Load(Fixture("Mapping/QuadNoSeam.obj"));
        return MDSS::TSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, {{0, {8, 8}}});
    }

    void TestProfileDistribution()
    {
        using namespace MDSS;
        const TSurfaceProfileDistribution Distribution =
            TSurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        Check(Distribution.ProfilePaths.size() == 1, "distribution should retain its ordered Profile path table");
        Check(Distribution.ProfilePaths[0].filename() == "Valid.SRProfile",
              "relative Profile path should resolve against the sidecar directory");

        const TSurfaceMappingData               Mapping = BuildQuad();
        const std::vector<TSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
        Check(ProfileMap.size() == Mapping.Texels.size(), "distribution should output one entry per texel");
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            Check(ProfileMap[Index] == (Mapping.Texels[Index].IsValid() ? 0U : InvalidSurfaceProfileIndex),
                  "valid texels should inherit their Surface Profile and invalid texels should keep sentinel");
        }

        CheckThrows(
            []
            {
                (void)TSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/OutOfRange.SurfaceProfileMap"));
            },
            "outside the profiles array",
            "out-of-range Profile index");
        CheckThrows(
            []
            {
                (void)TSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/MissingSurface.SurfaceProfileMap"));
            },
            "missing surfaceId 0",
            "missing Surface assignment");
        CheckThrows(
            []
            {
                (void)TSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/InvalidIndexType.SurfaceProfileMap"));
            },
            "non-negative integer",
            "non-integer Surface ID");
    }

    void TestGeometryBuild()
    {
        using namespace MDSS;
        const TSurfaceMappingData         Mapping = BuildQuad();
        std::vector<TSurfaceProfileIndex> ProfileMap(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            if (Mapping.Texels[Index].IsValid())
            {
                ProfileMap[Index] = 0;
            }
        }

        TSharedSurfaceGeometryData Geometry = TSurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1);
        Check(Geometry.GetTexelCount() == Mapping.Texels.size(), "shared geometry should preserve mapping texel count");
        Check(Geometry.GetSurface(0).Resolution == TSurfaceResolution{8, 8},
              "shared geometry should preserve Surface grid resolution");

        bool FoundValid = false;
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const TSurfaceTexelGeometry& GeometryTexel = Geometry.GetTexels()[Index];
            if (!Mapping.Texels[Index].IsValid())
            {
                Check(!GeometryTexel.IsValid(), "invalid mapping texels should remain invalid in shared geometry");
                Check(Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(Index)) == InvalidSurfaceProfileIndex,
                      "invalid geometry texels should retain the Profile sentinel");
                continue;
            }

            FoundValid = true;
            Check(GeometryTexel.Position == Mapping.Texels[Index].Position,
                  "builder should copy barycentric surface position");
            Check(GeometryTexel.Normal == Mapping.Texels[Index].Normal, "builder should copy surface normal");
            Check(GeometryTexel.NeighborIndices == Mapping.Texels[Index].Neighbors,
                  "builder should copy the seam-aware neighbor graph");
            Check(GeometryTexel.Geometry.MesoVirtualHeight == 0.0F && GeometryTexel.Geometry.ConcavityWeight == 0.0F,
                  "not-yet-built geometry scalars should have zero defaults");
            Check(Geometry.GetProfileIndex(static_cast<TLocalTexelIndex>(Index)) == 0,
                  "valid texels should retain the Profile assigned to their Surface");
        }
        Check(FoundValid, "fixture should include valid mapping texels");

        std::size_t CheckedNeighborDistances = 0;
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            const TSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            if (!Texel.IsValid())
            {
                continue;
            }
            for (const TLocalTexelIndex Neighbor : Texel.NeighborIndices)
            {
                if (Neighbor == InvalidTexelIndex)
                {
                    continue;
                }
                const glm::vec3 Delta = Geometry.GetTexels()[Neighbor].Position - Texel.Position;
                const float     Distance = glm::length(Delta);
                Check(std::isfinite(Distance) && Distance > 0.0F,
                      "neighbor distance should be finite and computed on demand from positions");
                const auto& ReverseNeighbors = Geometry.GetTexels()[Neighbor].NeighborIndices;
                Check(std::ranges::find(ReverseNeighbors, static_cast<TLocalTexelIndex>(Index)) !=
                          ReverseNeighbors.end(),
                      "geometry neighbor links should remain bidirectional");
                ++CheckedNeighborDistances;
            }
        }
        Check(CheckedNeighborDistances != 0, "fixture should include neighbors for on-demand distance checks");

        const auto        ValidMappingTexel = std::ranges::find_if(Mapping.Texels, &TSurfaceMappingTexel::IsValid);
        const std::size_t ValidTexelIndex = static_cast<std::size_t>(ValidMappingTexel - Mapping.Texels.begin());
        ProfileMap[ValidTexelIndex] = InvalidSurfaceProfileIndex;
        const TSharedSurfaceGeometryData NoSimulationGeometry =
            TSurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1);
        Check(NoSimulationGeometry.GetProfileIndex(static_cast<TLocalTexelIndex>(ValidTexelIndex)) ==
                  InvalidSurfaceProfileIndex,
              "valid geometry should preserve the sentinel that disables simulation for a texel");
    }

    void TestTexelMesh()
    {
        using namespace MDSS;
        TSharedSurfaceGeometryData Geometry({{0, {3, 3}}, {1, {2, 2}}});
        for (const auto& Range : Geometry.GetSurfaces())
            for (std::uint32_t Y = 0; Y < Range.Resolution.Height; ++Y)
                for (std::uint32_t X = 0; X < Range.Resolution.Width; ++X)
                {
                    auto& T = Geometry.GetTexels()[Range.FirstTexel + Y * Range.Resolution.Width + X];
                    T.Surface = Range.Surface;
                    T.Triangle = 0;
                    T.Chart = Range.Surface;
                    // Second chart has mirrored UV-to-position orientation.
                    T.Position = {Range.Surface == 0 ? float(X) : -float(X), float(Y), 0};
                }
        const auto Mesh = BuildSurfaceTexelMesh(Geometry);
        Check(Mesh.Surfaces.size() == 2 && Mesh.Surfaces[0].IndexCount == 24 && Mesh.Surfaces[1].IndexCount == 6,
              "texel grids should produce two triangles per complete cell with separate Surface ranges");
        Check(std::ranges::find(Mesh.Indices, 4U) != Mesh.Indices.end(),
              "interior texels must participate even when the source mesh only has corner vertices");
        for (std::size_t I = 0; I < Mesh.Indices.size(); I += 3)
        {
            const auto& A = Geometry.GetTexels()[Mesh.Indices[I]];
            const auto& B = Geometry.GetTexels()[Mesh.Indices[I + 1]];
            const auto& C = Geometry.GetTexels()[Mesh.Indices[I + 2]];
            Check(A.Surface == B.Surface && A.Surface == C.Surface && A.Chart == B.Chart && A.Chart == C.Chart,
                  "triangles must not connect different Surfaces or charts");
            Check(glm::dot(glm::cross(B.Position - A.Position, C.Position - A.Position), A.Normal) > 0,
                  "mirrored UV charts must retain front-facing geometric winding");
        }
        Geometry.GetTexels()[4].Surface = InvalidSurfaceID;
        const auto Hole = BuildSurfaceTexelMesh(Geometry);
        Check(std::ranges::find(Hole.Indices, 4U) == Hole.Indices.end() && Hole.Surfaces[0].IndexCount == 12,
              "invalid texels must be omitted while retaining valid three-corner cell triangles");
        Geometry.GetTexels()[10].Chart = 42;
        Check(BuildSurfaceTexelMesh(Geometry).Surfaces[1].IndexCount == 3,
              "chart boundaries must not be bridged by the regular UV grid");
        Geometry.GetTexels()[9].Position.x = std::numeric_limits<float>::quiet_NaN();
        Check(BuildSurfaceTexelMesh(Geometry).Surfaces[1].IndexCount == 0,
              "nonfinite geometry must not produce render indices");
        Geometry.GetTexels()[9].Position = Geometry.GetTexels()[11].Position;
        Check(BuildSurfaceTexelMesh(Geometry).Surfaces[1].IndexCount == 0,
              "degenerate triangles must not produce render indices");
    }

    void TestStitchedSourceMesh()
    {
        using namespace MDSS;
        for (const auto* Name : {"Mapping/QuadSeam.obj", "Mapping/QuadNoSeam.obj", "Mapping/DisconnectedQuads.obj"})
        {
            const auto Source = TOBJLoader::Load(Fixture(Name));
            const auto Mapping = TSurfaceMappingBuilder::Build(Source.Vertices, Source.Triangles, {{0, {24, 24}}});
            std::vector<TSurfaceProfileIndex> Profiles(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
            for (std::size_t T = 0; T < Profiles.size(); ++T)
                if (Mapping.Texels[T].IsValid()) Profiles[T] = 0;
            const auto Geometry = TSurfaceGeometryBuilder::Build(Mapping, Profiles, 1);
            const auto Mesh = BuildSurfaceTexelMesh(Geometry, Source.Vertices, Source.Triangles);
            using TPoint = std::tuple<float, float, float>;
            const auto Point = [](glm::vec4 P) { return TPoint{P.x, P.y, P.z}; };
            std::map<std::pair<TPoint, TPoint>, unsigned> Edges;
            double Area = 0;
            for (std::size_t I = 0; I < Mesh.Indices.size(); I += 3)
            {
                const auto A = Mesh.Indices[I], B = Mesh.Indices[I + 1], C = Mesh.Indices[I + 2];
                const auto Cross = glm::cross(glm::vec3(Mesh.Vertices[B].Position - Mesh.Vertices[A].Position),
                                              glm::vec3(Mesh.Vertices[C].Position - Mesh.Vertices[A].Position));
                Check(Cross.z > 0, "refined source triangles must retain front-facing winding");
                Area += glm::length(Cross) * 0.5;
                for (unsigned K = 0; K < 3; ++K)
                {
                    auto X = Point(Mesh.Vertices[Mesh.Indices[I + K]].Position);
                    auto Y = Point(Mesh.Vertices[Mesh.Indices[I + (K + 1) % 3]].Position);
                    if (Y < X) std::swap(X, Y);
                    ++Edges[{X, Y}];
                }
            }
            const bool Disconnected = std::string(Name).find("Disconnected") != std::string::npos;
            const auto Boundary = std::count_if(Edges.begin(), Edges.end(), [](const auto& E) { return E.second == 1; });
            Check(Boundary == (Disconnected ? 8 : 4), "texel refinement must add no open edges or seam T-junctions");
            Check(std::all_of(Edges.begin(), Edges.end(), [](const auto& E) { return E.second <= 2; }),
                  "refinement must not create overlapping/non-manifold faces");
            Check(std::abs(Area - (Disconnected ? 2.0 : 1.0)) < 1e-5,
                  "refinement must cover the entire source surface, including chart border bands");
            // Different chart heights must still produce bit-identical positions at UV seam copies.
            std::map<TPoint, glm::vec3> Displaced;
            for (const auto& V : Mesh.Vertices)
            {
                float Height = 0;
                for (unsigned K = 0; K < 4; ++K)
                    if (V.Weights[K] > 0) Height += V.Weights[K] * float(1 + Geometry.GetTexels()[V.Samples[K]].Chart);
                const auto P = glm::vec3(V.Position) + glm::vec3(V.DisplacementNormal) * Height;
                const auto [It, Added] = Displaced.try_emplace(Point(V.Position), P);
                Check(Added || It->second == P, "seam copies must remain welded with unequal chart heights");
            }
            Check(Mesh.Vertices.size() > Source.Vertices.size(), "interior texels must participate in source refinement");
        }
        // Tiny triangles without any simulation centers must retain their full source geometry.
        const auto Tiny = TOBJLoader::Load(Fixture("Mapping/SingleTriangle.obj"));
        TSharedSurfaceGeometryData Empty({{0, {1, 1}}});
        const auto Mesh = BuildSurfaceTexelMesh(Empty, Tiny.Vertices, Tiny.Triangles);
        Check(Mesh.Indices.size() == 3, "triangles with no texel samples must not disappear");
        Check(std::all_of(Mesh.Vertices.begin(), Mesh.Vertices.end(), [](const auto& V) {
            return V.Weights == glm::vec4(0); }), "unsampled connected components must retain zero displacement");
    }

    void TestRuntimePreprocessing()
    {
        using namespace MDSS;
        const TSurfaceProfileDistribution Distribution =
            TSurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        const TSurfaceMappingData               Mapping = BuildQuad();
        const std::vector<TSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);

        const TSurfaceRuntimeData First = TSurfacePreprocessor::Build(Mapping, ProfileMap, 1);
        const TSurfaceRuntimeData Second = TSurfacePreprocessor::Build(Mapping, ProfileMap, 1);
        Check(First.Geometry->GetTexelCount() == Mapping.Texels.size(),
              "Runtime preprocessing should produce one geometry texel per mapping texel");
        Check(First.Geometry->GetSurfaces().size() == Second.Geometry->GetSurfaces().size() &&
                  First.Geometry->GetSurfaces()[0].Surface == Second.Geometry->GetSurfaces()[0].Surface &&
                  First.Geometry->GetSurfaces()[0].Resolution == Second.Geometry->GetSurfaces()[0].Resolution &&
                  First.Geometry->GetSurfaces()[0].FirstTexel == Second.Geometry->GetSurfaces()[0].FirstTexel &&
                  First.Geometry->GetSurfaces()[0].TexelCount == Second.Geometry->GetSurfaces()[0].TexelCount,
              "identical Runtime inputs should deterministically reproduce Surface ranges");
        Check(First.Geometry->GetProfileMap() == Second.Geometry->GetProfileMap(),
              "identical Runtime inputs should deterministically reproduce the texel Profile map");
    }
} // namespace

int main()
{
    TestProfileDistribution();
    TestGeometryBuild();
    TestTexelMesh();
    TestStitchedSourceMesh();
    TestRuntimePreprocessing();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " shared geometry test(s) failed.\n";
        return 1;
    }
    std::cout << "All shared geometry tests passed.\n";
    return 0;
}
