/**
 * @file SurfaceMappingTests.cpp
 * @brief OBJ topology 보존, UV rasterization, chart neighbor와 seam 연결 검증.
 */

#include "AssetManager/Loaders/OBJLoader.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingValidation.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

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

    std::filesystem::path GetMappingFixturePath(const char* FileName)
    {
        return std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / "Mapping" / FileName;
    }

    MDSS::TSurfaceMappingData BuildFixture(const char* FileName, std::uint32_t Resolution = 16)
    {
        const MDSS::TOBJLoadResult Mesh = MDSS::TOBJLoader::Load(GetMappingFixturePath(FileName));
        return MDSS::TSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, {{0, {Resolution, Resolution}}});
    }

    std::size_t CountValidTexels(const MDSS::TSurfaceMappingData& Mapping)
    {
        return static_cast<std::size_t>(std::ranges::count_if(Mapping.Texels, &MDSS::TSurfaceMappingTexel::IsValid));
    }

    bool HasCrossTriangleNeighbor(const MDSS::TSurfaceMappingData& Mapping, bool RequireDifferentChart)
    {
        for (const MDSS::TSurfaceMappingTexel& Texel : Mapping.Texels)
        {
            if (!Texel.IsValid())
            {
                continue;
            }
            for (const MDSS::TLocalTexelIndex Neighbor : Texel.Neighbors)
            {
                if (Neighbor == MDSS::InvalidTexelIndex)
                {
                    continue;
                }
                const MDSS::TSurfaceMappingTexel& Other = Mapping.Texels[Neighbor];
                if (Other.Triangle != Texel.Triangle && (!RequireDifferentChart || Other.Chart != Texel.Chart))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool HasCrossChartNeighbor(const MDSS::TSurfaceMappingData& Mapping)
    {
        for (const MDSS::TSurfaceMappingTexel& Texel : Mapping.Texels)
        {
            if (!Texel.IsValid())
            {
                continue;
            }
            for (const MDSS::TLocalTexelIndex Neighbor : Texel.Neighbors)
            {
                if (Neighbor != MDSS::InvalidTexelIndex && Mapping.Texels[Neighbor].Chart != Texel.Chart)
                {
                    return true;
                }
            }
        }
        return false;
    }

    void TestOBJTopologyPreservation()
    {
        const MDSS::TOBJLoadResult Mesh = MDSS::TOBJLoader::Load(GetMappingFixturePath("QuadSeam.obj"));
        Check(Mesh.Triangles.size() == 2, "seam fixture should retain two source triangles");

        const MDSS::TMeshTriangleSource& A = Mesh.Triangles[0];
        const MDSS::TMeshTriangleSource& B = Mesh.Triangles[1];
        Check(A.OriginalPositionIndices[0] == B.OriginalPositionIndices[0],
              "seam triangles should retain their shared original position");
        Check(A.RenderVertexIndices[0] != B.RenderVertexIndices[0],
              "different UVs should split the shared position into distinct render vertices");
        Check(A.Surface == 0 && B.Surface == 0, "OBJ triangles without materials should use Surface zero");
    }

    void TestSingleTriangleRasterization()
    {
        const MDSS::TSurfaceMappingData Mapping = BuildFixture("SingleTriangle.obj", 8);
        Check(CountValidTexels(Mapping) > 0, "single triangle should cover texel centers");
        Check(Mapping.Texels.size() == 64, "8 by 8 Surface should allocate 64 texels");

        for (const MDSS::TSurfaceMappingTexel& Texel : Mapping.Texels)
        {
            if (!Texel.IsValid())
            {
                continue;
            }
            const float Sum = Texel.Barycentric.x + Texel.Barycentric.y + Texel.Barycentric.z;
            Check(std::abs(Sum - 1.0F) < 1.0e-5F, "valid texel barycentric weights should sum to one");
            Check(Texel.Barycentric.x >= -1.0e-5F && Texel.Barycentric.y >= -1.0e-5F && Texel.Barycentric.z >= -1.0e-5F,
                  "valid texel barycentric weights should be non-negative");
        }
    }

    void TestRegularChartNeighbors()
    {
        const MDSS::TSurfaceMappingData Mapping = BuildFixture("QuadNoSeam.obj");
        Check(HasCrossTriangleNeighbor(Mapping, false),
              "triangles sharing a UV edge should connect through regular texel neighbors");
        Check(!HasCrossChartNeighbor(Mapping), "quad without a seam should remain one UV chart");
    }

    void TestSeamNeighbors()
    {
        const MDSS::TSurfaceMappingData Mapping = BuildFixture("QuadSeam.obj", 24);
        Check(HasCrossTriangleNeighbor(Mapping, true),
              "UV seam triangles should receive bidirectional cross-chart neighbors");
        Check(Mapping.Warnings.empty(), "prepared seam fixture should not produce mapping warnings");
    }

    void TestDisconnectedTopology()
    {
        const MDSS::TSurfaceMappingData Mapping = BuildFixture("DisconnectedQuads.obj", 24);
        Check(!HasCrossChartNeighbor(Mapping),
              "spatially close but topologically disconnected charts must not become neighbors");
    }

    void TestDeterministicResult()
    {
        const MDSS::TSurfaceMappingData First = BuildFixture("QuadSeam.obj", 24);
        const MDSS::TSurfaceMappingData Second = BuildFixture("QuadSeam.obj", 24);
        Check(First.Texels.size() == Second.Texels.size(), "repeated mapping should preserve texel count");
        for (std::size_t Index = 0; Index < First.Texels.size(); ++Index)
        {
            const MDSS::TSurfaceMappingTexel& A = First.Texels[Index];
            const MDSS::TSurfaceMappingTexel& B = Second.Texels[Index];
            Check(A.Surface == B.Surface && A.Triangle == B.Triangle && A.Chart == B.Chart &&
                      A.Barycentric == B.Barycentric && A.Neighbors == B.Neighbors,
                  "mapping output should be deterministic");
        }
    }

    void TestMultipleSurfaceRanges()
    {
        std::vector<MDSS::TVertex> Vertices(6);
        Vertices[0].UV = {0.0F, 0.0F};
        Vertices[1].UV = {1.0F, 0.0F};
        Vertices[2].UV = {0.0F, 1.0F};
        Vertices[3].UV = {0.0F, 0.0F};
        Vertices[4].UV = {1.0F, 0.0F};
        Vertices[5].UV = {0.0F, 1.0F};
        for (std::size_t Index = 0; Index < Vertices.size(); ++Index)
        {
            Vertices[Index].Position = {static_cast<float>(Index), 0.0F, 0.0F};
        }

        const std::vector<MDSS::TMeshTriangleSource> Triangles = {
            {{{0, 1, 2}}, {{0, 1, 2}}, {{0, 1, 2}}, 0},
            {{{3, 4, 5}}, {{3, 4, 5}}, {{3, 4, 5}}, 1},
        };
        const MDSS::TSurfaceMappingData Mapping =
            MDSS::TSurfaceMappingBuilder::Build(Vertices, Triangles, {{0, {8, 8}}, {1, {4, 4}}});

        Check(Mapping.Surfaces.size() == 2, "mapping should retain both Surface ranges");
        Check(Mapping.Surfaces[0].FirstTexel == 0 && Mapping.Surfaces[0].TexelCount == 64,
              "first Surface should occupy its 8 by 8 range");
        Check(Mapping.Surfaces[1].FirstTexel == 64 && Mapping.Surfaces[1].TexelCount == 16,
              "second Surface should follow with its 4 by 4 range");
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            if (Mapping.Texels[Index].IsValid())
            {
                const MDSS::TSurfaceLocalID ExpectedSurface = Index < 64 ? 0 : 1;
                Check(Mapping.Texels[Index].Surface == ExpectedSurface,
                      "valid texel should retain the Surface owning its range");
            }
        }
    }

    void TestInvalidFixtures()
    {
        CheckThrows([] { (void)BuildFixture("DegenerateUV.obj"); }, "UV area", "degenerate UV triangle");
        CheckThrows([] { (void)BuildFixture("Overlap.obj"); }, "UV overlap", "overlapping UV triangles");
        CheckThrows([] { (void)BuildFixture("NonManifold.obj"); }, "non-manifold", "non-manifold mesh edge");
        CheckThrows(
            [] { (void)BuildFixture("MissingUV.obj"); }, "missing a Simulation UV", "OBJ without UV coordinates");
    }

    void TestInvariantValidation()
    {
        MDSS::TSurfaceMappingData Mapping = BuildFixture("QuadNoSeam.obj");
        MDSS::TLocalTexelIndex    Source = MDSS::InvalidTexelIndex;
        MDSS::TLocalTexelIndex    Target = MDSS::InvalidTexelIndex;
        for (std::size_t Index = 0; Index < Mapping.Texels.size() && Source == MDSS::InvalidTexelIndex; ++Index)
        {
            for (const MDSS::TLocalTexelIndex Neighbor : Mapping.Texels[Index].Neighbors)
            {
                if (Neighbor != MDSS::InvalidTexelIndex)
                {
                    Source = static_cast<MDSS::TLocalTexelIndex>(Index);
                    Target = Neighbor;
                    break;
                }
            }
        }
        Check(Source != MDSS::InvalidTexelIndex, "fixture should contain a neighbor pair to corrupt");
        if (Source == MDSS::InvalidTexelIndex)
        {
            return;
        }

        for (MDSS::TLocalTexelIndex& Neighbor : Mapping.Texels[Target].Neighbors)
        {
            if (Neighbor == Source)
            {
                Neighbor = MDSS::InvalidTexelIndex;
                break;
            }
        }
        CheckThrows([&] { MDSS::ValidateSurfaceMapping(Mapping); }, "bidirectional", "one-way neighbor relationship");
    }
} // namespace

int main()
{
    TestOBJTopologyPreservation();
    TestSingleTriangleRasterization();
    TestRegularChartNeighbors();
    TestSeamNeighbors();
    TestDisconnectedTopology();
    TestDeterministicResult();
    TestMultipleSurfaceRanges();
    TestInvalidFixtures();
    TestInvariantValidation();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " surface mapping test(s) failed.\n";
        return 1;
    }

    std::cout << "All Surface Mapping tests passed.\n";
    return 0;
}
