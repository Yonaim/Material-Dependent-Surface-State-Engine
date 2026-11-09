/**
 * @file SharedSurfaceGeometryTests.cpp
 * @brief 공유 geometry 구성과 Runtime preprocessing을 검증한다.
 */

#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "SurfaceState/Geometry/MesoGeometryBuilder.h"
#include "SurfaceState/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceState/Geometry/SurfaceTexelMeshBuilder.h"
#include "SurfaceState/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceState/Runtime/SurfaceRuntimeData.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <glm/geometric.hpp>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// ============================================================
// 00. Fixtures and Test Support
// ============================================================
#pragma region 00_Fixtures_and_Test_Support

namespace
{
    int FailureCount = 0;

    void Check(bool bCondition, const std::string& Message)
    {
        if (!bCondition)
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

    MDSS::SurfaceState::FSurfaceMappingData BuildQuad()
    {
        const MDSS::Asset::FOBJLoadResult Mesh = MDSS::Asset::FOBJLoader::Load(Fixture("Mapping/QuadNoSeam.obj"));
        return MDSS::SurfaceState::FSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, {{0, {8, 8}}});
    }

#pragma endregion

// ============================================================
// 01. TestProfileDistribution
// ============================================================
#pragma region 01_TestProfileDistribution

    void TestProfileDistribution()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const Asset::FSurfaceProfileDistribution Distribution =
            Asset::FSurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        Check(Distribution.ProfilePaths.size() == 1, "distribution should retain its ordered Profile path table");
        Check(Distribution.ProfilePaths[0].filename() == "Valid.SRProfile",
              "relative Profile path should resolve against the sidecar directory");

        const SurfaceState::FSurfaceMappingData               Mapping = BuildQuad();
        const std::vector<SurfaceState::FSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
        Check(ProfileMap.size() == Mapping.Texels.size(), "distribution should output one entry per texel");
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            Check(ProfileMap[Index] ==
                      (Mapping.Texels[Index].IsValid() ? 0U : SurfaceState::InvalidSurfaceProfileIndex),
                  "valid texels should inherit their Surface Profile and invalid texels should keep sentinel");
        }

        CheckThrows(
            []
            {
                (void)Asset::FSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/OutOfRange.SurfaceProfileMap"));
            },
            "outside the profiles array",
            "out-of-range Profile index");
        CheckThrows(
            []
            {
                (void)Asset::FSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/MissingSurface.SurfaceProfileMap"));
            },
            "missing surfaceId 0",
            "missing Surface assignment");
        CheckThrows(
            []
            {
                (void)Asset::FSurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/InvalidIndexType.SurfaceProfileMap"));
            },
            "non-negative integer",
            "non-integer Surface ID");
    }

#pragma endregion

// ============================================================
// 02. TestGeometryBuild
// ============================================================
#pragma region 02_TestGeometryBuild

    void TestGeometryBuild()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const SurfaceState::FSurfaceMappingData         Mapping = BuildQuad();
        std::vector<SurfaceState::FSurfaceProfileIndex> ProfileMap(Mapping.Texels.size(),
                                                                   SurfaceState::InvalidSurfaceProfileIndex);
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            if (Mapping.Texels[Index].IsValid())
            {
                ProfileMap[Index] = 0;
            }
        }

        FSharedSurfaceGeometryData Geometry = SurfaceState::FSurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1);
        Check(Geometry.GetTexelCount() == Mapping.Texels.size(), "shared geometry should preserve mapping texel count");
        Check(Geometry.GetSurface(0).Resolution == SurfaceState::FSurfaceResolution{8, 8},
              "shared geometry should preserve Surface grid resolution");

        bool bFoundValid = false;
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const SurfaceState::FSurfaceTexelGeometry& GeometryTexel = Geometry.GetTexels()[Index];
            if (!Mapping.Texels[Index].IsValid())
            {
                Check(!GeometryTexel.IsValid(), "invalid mapping texels should remain invalid in shared geometry");
                Check(Geometry.GetProfileIndex(static_cast<FLocalTexelIndex>(Index)) ==
                          SurfaceState::InvalidSurfaceProfileIndex,
                      "invalid geometry texels should retain the Profile sentinel");
                continue;
            }

            bFoundValid = true;
            Check(GeometryTexel.Position == Mapping.Texels[Index].Position,
                  "builder should copy barycentric surface position");
            Check(GeometryTexel.Normal == Mapping.Texels[Index].Normal, "builder should copy surface normal");
            Check(GeometryTexel.NeighborIndices == Mapping.Texels[Index].Neighbors,
                  "builder should copy the seam-aware neighbor graph");
            Check(GeometryTexel.Geometry.MesoVirtualHeight == 0.0F && GeometryTexel.Geometry.ConcavityWeight == 0.0F,
                  "not-yet-built geometry scalars should have zero defaults");
            Check(Geometry.GetProfileIndex(static_cast<FLocalTexelIndex>(Index)) == 0,
                  "valid texels should retain the Profile assigned to their Surface");
        }
        Check(bFoundValid, "fixture should include valid mapping texels");

        std::size_t CheckedNeighborDistances = 0;
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            const SurfaceState::FSurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            if (!Texel.IsValid())
            {
                continue;
            }
            for (const FLocalTexelIndex Neighbor : Texel.NeighborIndices)
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
                Check(std::ranges::find(ReverseNeighbors, static_cast<FLocalTexelIndex>(Index)) !=
                          ReverseNeighbors.end(),
                      "geometry neighbor links should remain bidirectional");
                ++CheckedNeighborDistances;
            }
        }
        Check(CheckedNeighborDistances != 0, "fixture should include neighbors for on-demand distance checks");

        const auto ValidMappingTexel =
            std::ranges::find_if(Mapping.Texels, &SurfaceState::FSurfaceMappingTexel::IsValid);
        const std::size_t ValidTexelIndex = static_cast<std::size_t>(ValidMappingTexel - Mapping.Texels.begin());
        ProfileMap[ValidTexelIndex] = SurfaceState::InvalidSurfaceProfileIndex;
        const FSharedSurfaceGeometryData NoSimulationGeometry =
            SurfaceState::FSurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1);
        Check(NoSimulationGeometry.GetProfileIndex(static_cast<FLocalTexelIndex>(ValidTexelIndex)) ==
                  SurfaceState::InvalidSurfaceProfileIndex,
              "valid geometry should preserve the sentinel that disables simulation for a texel");
    }

#pragma endregion

// ============================================================
// 03. TestTexelMesh
// ============================================================
#pragma region 03_TestTexelMesh

    void TestTexelMesh()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        FSharedSurfaceGeometryData Geometry({{0, {3, 3}}, {1, {2, 2}}});
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
        Geometry.GetTexels()[4].Surface = SurfaceState::InvalidSurfaceID;
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

#pragma endregion

// ============================================================
// 04. TestStitchedSourceMesh
// ============================================================
#pragma region 04_TestStitchedSourceMesh

    void TestStitchedSourceMesh()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        for (const auto* Name : {"Mapping/QuadSeam.obj", "Mapping/QuadNoSeam.obj", "Mapping/DisconnectedQuads.obj"})
        {
            const auto Source = FOBJLoader::Load(Fixture(Name));
            const auto Mapping =
                SurfaceState::FSurfaceMappingBuilder::Build(Source.Vertices, Source.Triangles, {{0, {24, 24}}});
            std::vector<SurfaceState::FSurfaceProfileIndex> Profiles(Mapping.Texels.size(),
                                                                     SurfaceState::InvalidSurfaceProfileIndex);
            for (std::size_t T = 0; T < Profiles.size(); ++T)
                if (Mapping.Texels[T].IsValid())
                    Profiles[T] = 0;
            const auto Geometry = SurfaceState::FSurfaceGeometryBuilder::Build(Mapping, Profiles, 1);
            const auto Mesh = BuildSurfaceTexelMesh(Geometry, Source.Vertices, Source.Triangles);
            using FPoint = std::tuple<float, float, float>;
            const auto                                    Point = [](glm::vec4 P) { return FPoint{P.x, P.y, P.z}; };
            std::map<std::pair<FPoint, FPoint>, unsigned> Edges;
            double                                        Area = 0;
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
                    if (Y < X)
                        std::swap(X, Y);
                    ++Edges[{X, Y}];
                }
            }
            const bool bDisconnected = std::string(Name).find("Disconnected") != std::string::npos;
            const auto Boundary =
                std::count_if(Edges.begin(), Edges.end(), [](const auto& E) { return E.second == 1; });
            Check(Boundary == (bDisconnected ? 8 : 4), "texel refinement must add no open edges or seam T-junctions");
            Check(std::all_of(Edges.begin(), Edges.end(), [](const auto& E) { return E.second <= 2; }),
                  "refinement must not create overlapping/non-manifold faces");
            Check(std::abs(Area - (bDisconnected ? 2.0 : 1.0)) < 1e-5,
                  "refinement must cover the entire source surface, including chart border bands");
            // Different chart heights must still produce bit-identical positions at UV seam copies.
            std::map<FPoint, glm::vec3> Displaced;
            for (const auto& V : Mesh.Vertices)
            {
                float Height = 0;
                for (unsigned K = 0; K < 4; ++K)
                    if (V.Weights[K] > 0)
                        Height += V.Weights[K] * float(1 + Geometry.GetTexels()[V.Samples[K]].Chart);
                const auto P = glm::vec3(V.Position) + glm::vec3(V.DisplacementNormal) * Height;
                const auto [It, Added] = Displaced.try_emplace(Point(V.Position), P);
                Check(Added || It->second == P, "seam copies must remain welded with unequal chart heights");
            }
            Check(Mesh.Vertices.size() > Source.Vertices.size(),
                  "interior texels must participate in source refinement");
        }
        // Tiny triangles without any simulation centers must retain their full source geometry.
        const auto                 Tiny = FOBJLoader::Load(Fixture("Mapping/SingleTriangle.obj"));
        FSharedSurfaceGeometryData Empty({{0, {1, 1}}});
        const auto                 Mesh = BuildSurfaceTexelMesh(Empty, Tiny.Vertices, Tiny.Triangles);
        Check(Mesh.Indices.size() == 3, "triangles with no texel samples must not disappear");
        Check(std::all_of(
                  Mesh.Vertices.begin(), Mesh.Vertices.end(), [](const auto& V) { return V.Weights == glm::vec4(0); }),
              "unsampled connected components must retain zero displacement");
    }

#pragma endregion

// ============================================================
// 05. TestCubeTopDiagonalDisplacement
// ============================================================
#pragma region 05_TestCubeTopDiagonalDisplacement

    void TestCubeTopDiagonalDisplacement()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const auto Source = FOBJLoader::Load(Fixture("../../Assets/Meshes/MarbleCube/MarbleCube.obj"));
        std::vector<FSurfaceDefinition> Surfaces;
        for (std::uint32_t Surface = 0; Surface < Source.Sections.size(); ++Surface)
            Surfaces.push_back({Surface, {16, 16}});
        const auto Mapping = FSurfaceMappingBuilder::Build(Source.Vertices, Source.Triangles, Surfaces);
        std::vector<FSurfaceProfileIndex> Profiles(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t T = 0; T < Profiles.size(); ++T)
            if (Mapping.Texels[T].IsValid())
                Profiles[T] = 0;
        const auto  Geometry = FSurfaceGeometryBuilder::Build(Mapping, Profiles, 1);
        const auto  Mesh = BuildSurfaceTexelMesh(Geometry, Source.Vertices, Source.Triangles);
        std::size_t TopDiagonalVertices = 0;
        for (const auto& Vertex : Mesh.Vertices)
        {
            const auto Position = glm::vec3(Vertex.Position);
            if (std::abs(Position.z - 0.5F) > 1e-5F || std::abs(Position.x - Position.y) > 1e-5F ||
                std::abs(Position.x) >= 0.45F || Vertex.Normal.z < 0.99F)
                continue;
            ++TopDiagonalVertices;
            const auto Direction = glm::vec3(Vertex.DisplacementNormal);
            Check(std::abs(Direction.x) < 1e-5F && std::abs(Direction.y) < 1e-5F && Direction.z > 0.99F,
                  "cube top diagonal displacement must use its incident top-face normals");
        }
        Check(TopDiagonalVertices > 0, "cube top diagonal must contain refined render vertices");
    }

#pragma endregion

// ============================================================
// 06. TestRuntimePreprocessing
// ============================================================
#pragma region 06_TestRuntimePreprocessing

    void TestRuntimePreprocessing()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const Asset::FSurfaceProfileDistribution Distribution =
            Asset::FSurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        const SurfaceState::FSurfaceMappingData               Mapping = BuildQuad();
        const std::vector<SurfaceState::FSurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);

        const SurfaceState::FSurfaceRuntimeData First =
            SurfaceState::FSurfacePreprocessor::Build(Mapping, ProfileMap, 1);
        const SurfaceState::FSurfaceRuntimeData Second =
            SurfaceState::FSurfacePreprocessor::Build(Mapping, ProfileMap, 1);
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

#pragma endregion

// ============================================================
// 07. TestMacroConcavityWithoutNormalMap
// ============================================================
#pragma region 07_TestMacroConcavityWithoutNormalMap

    void TestMacroConcavityWithoutNormalMap()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const auto CenterWeight = [](float CurvatureX, float CurvatureY)
        {
            FSharedSurfaceGeometryData Geometry({{0, {5, 5}}});
            auto&                      Texels = Geometry.GetTexels();
            for (int Y = 0; Y < 5; ++Y)
                for (int X = 0; X < 5; ++X)
                {
                    const int   Index = Y * 5 + X;
                    auto&       Texel = Texels[Index];
                    const float DX = float(X - 2), DY = float(Y - 2);
                    Texel.Surface = Texel.Triangle = Texel.Chart = 0;
                    Texel.Position = {DX, DY, CurvatureX * DX * DX + CurvatureY * DY * DY};
                    Texel.Normal = glm::normalize(glm::vec3(-2.0F * CurvatureX * DX, -2.0F * CurvatureY * DY, 1.0F));
                    std::size_t DirectionIndex = 0;
                    for (int NY = std::max(0, Y - 1); NY <= std::min(4, Y + 1); ++NY)
                        for (int NX = std::max(0, X - 1); NX <= std::min(4, X + 1); ++NX)
                            if (NX != X || NY != Y)
                                Texel.NeighborIndices[DirectionIndex++] = NY * 5 + NX;
                }
            (void)BuildMesoGeometry(Geometry);
            return Texels[12].Geometry.ConcavityWeight;
        };
        Check(CenterWeight(0.0F, 0.0F) < 1.0e-5F, "flat Macro surface should have no concavity");
        Check(CenterWeight(0.12F, 0.12F) > 0.1F, "Macro bowl should have concavity without a normal map");
        Check(CenterWeight(0.12F, 0.0F) > 0.1F, "Macro trough should have concavity without a normal map");
        Check(CenterWeight(-0.12F, -0.12F) < 1.0e-5F, "convex Macro dome should not retain material");
        Check(CenterWeight(0.12F, -0.12F) < 1.0e-5F, "saddle should not be classified as a cavity");
    }
} // namespace

#pragma endregion

// ============================================================
// 08. Test Entry Point
// ============================================================
#pragma region 08_Test_Entry_Point

int main()
{
    TestProfileDistribution();
    TestGeometryBuild();
    TestTexelMesh();
    TestStitchedSourceMesh();
    TestCubeTopDiagonalDisplacement();
    TestRuntimePreprocessing();
    TestMacroConcavityWithoutNormalMap();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " shared geometry test(s) failed.\n";
        return 1;
    }
    std::cout << "All shared geometry tests passed.\n";
    return 0;
}

#pragma endregion
