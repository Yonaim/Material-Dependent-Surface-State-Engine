/**
 * @file SurfaceMappingTests.cpp
 * @brief OBJ topology 보존, UV rasterization, chart neighbor와 seam 연결 검증.
 */

#include "AssetManager/Loaders/OBJLoader.h"
#include "SurfaceState/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceState/Mapping/SurfaceMappingValidation.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

// ============================================================
// 00. Fixtures and Test Support
// ============================================================
#pragma region "00_Fixtures_and_Test_Support"
// 회귀 실행에 사용할 입력 자산과 공통 검사·GPU 준비 코드를 제공한다.

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

    std::filesystem::path GetMappingFixturePath(const char* FileName)
    {
        return std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / "Mapping" / FileName;
    }

    MDSS::SurfaceState::FSurfaceMappingData BuildFixture(const char* FileName, std::uint32_t Resolution = 16)
    {
        const MDSS::Asset::FOBJLoadResult Mesh = MDSS::Asset::FOBJLoader::Load(GetMappingFixturePath(FileName));
        return MDSS::SurfaceState::FSurfaceMappingBuilder::Build(
            Mesh.Vertices, Mesh.Triangles, {{0, {Resolution, Resolution}}});
    }

    std::size_t CountValidTexels(const MDSS::SurfaceState::FSurfaceMappingData& Mapping)
    {
        return static_cast<std::size_t>(
            std::ranges::count_if(Mapping.Texels, &MDSS::SurfaceState::FSurfaceMappingTexel::IsValid));
    }

    bool HasCrossTriangleNeighbor(const MDSS::SurfaceState::FSurfaceMappingData& Mapping, bool bRequireDifferentChart)
    {
        for (const MDSS::SurfaceState::FSurfaceMappingTexel& Texel : Mapping.Texels)
        {
            if (!Texel.IsValid())
            {
                continue;
            }

            for (const MDSS::SurfaceState::FLocalTexelIndex Neighbor : Texel.Neighbors)
            {
                if (Neighbor == MDSS::SurfaceState::InvalidTexelIndex)
                {
                    continue;
                }

                const MDSS::SurfaceState::FSurfaceMappingTexel& Other = Mapping.Texels[Neighbor];

                if (Other.Triangle != Texel.Triangle && (!bRequireDifferentChart || Other.Chart != Texel.Chart))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool HasCrossChartNeighbor(const MDSS::SurfaceState::FSurfaceMappingData& Mapping)
    {
        for (const MDSS::SurfaceState::FSurfaceMappingTexel& Texel : Mapping.Texels)
        {
            if (!Texel.IsValid())
            {
                continue;
            }

            for (const MDSS::SurfaceState::FLocalTexelIndex Neighbor : Texel.Neighbors)
            {
                if (Neighbor != MDSS::SurfaceState::InvalidTexelIndex && Mapping.Texels[Neighbor].Chart != Texel.Chart)
                {
                    return true;
                }
            }
        }

        return false;
    }

#pragma endregion

// ============================================================
// 01. TestOBJTopologyPreservation
// ============================================================
#pragma region "01_TestOBJTopologyPreservation"
// 기대 조건: seam fixture should retain two source triangles

    void TestOBJTopologyPreservation()
    {
        const MDSS::Asset::FOBJLoadResult Mesh = MDSS::Asset::FOBJLoader::Load(GetMappingFixturePath("QuadSeam.obj"));
        Check(Mesh.Triangles.size() == 2, "seam fixture should retain two source triangles");

        const MDSS::Asset::FMeshTriangleSource& A = Mesh.Triangles[0];
        const MDSS::Asset::FMeshTriangleSource& B = Mesh.Triangles[1];
        Check(A.OriginalPositionIndices[0] == B.OriginalPositionIndices[0],
              "seam triangles should retain their shared original position");
        Check(A.RenderVertexIndices[0] != B.RenderVertexIndices[0],
              "different UVs should split the shared position into distinct render vertices");
        Check(A.Surface == 0 && B.Surface == 0, "OBJ triangles without materials should use Surface zero");
    }

#pragma endregion

// ============================================================
// 02. TestSingleTriangleRasterization
// ============================================================
#pragma region "02_TestSingleTriangleRasterization"
// 기대 조건: single triangle should cover texel centers

    void TestSingleTriangleRasterization()
    {
        const MDSS::SurfaceState::FSurfaceMappingData Mapping = BuildFixture("SingleTriangle.obj", 8);
        Check(CountValidTexels(Mapping) > 0, "single triangle should cover texel centers");
        Check(Mapping.Texels.size() == 64, "8 by 8 Surface should allocate 64 texels");

        for (const MDSS::SurfaceState::FSurfaceMappingTexel& Texel : Mapping.Texels)
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

#pragma endregion

// ============================================================
// 03. TestRegularChartNeighbors
// ============================================================
#pragma region "03_TestRegularChartNeighbors"
// 기대 조건: triangles sharing a UV edge should connect through regular texel neighbors

    void TestRegularChartNeighbors()
    {
        const MDSS::SurfaceState::FSurfaceMappingData Mapping = BuildFixture("QuadNoSeam.obj");
        Check(HasCrossTriangleNeighbor(Mapping, false),
              "triangles sharing a UV edge should connect through regular texel neighbors");
        Check(!HasCrossChartNeighbor(Mapping), "quad without a seam should remain one UV chart");
    }

#pragma endregion

// ============================================================
// 04. TestSeamNeighbors
// ============================================================
#pragma region "04_TestSeamNeighbors"
// 기대 조건: UV seam triangles should receive bidirectional cross-chart neighbors

    void TestSeamNeighbors()
    {
        const MDSS::SurfaceState::FSurfaceMappingData Mapping = BuildFixture("QuadSeam.obj", 24);
        Check(HasCrossTriangleNeighbor(Mapping, true),
              "UV seam triangles should receive bidirectional cross-chart neighbors");
        Check(Mapping.Warnings.empty(), "prepared seam fixture should not produce mapping warnings");
    }

#pragma endregion

// ============================================================
// 05. TestDisconnectedTopology
// ============================================================
#pragma region "05_TestDisconnectedTopology"
// 기대 조건: spatially close but topologically disconnected charts must not become neighbors

    void TestDisconnectedTopology()
    {
        const MDSS::SurfaceState::FSurfaceMappingData Mapping = BuildFixture("DisconnectedQuads.obj", 24);
        Check(!HasCrossChartNeighbor(Mapping),
              "spatially close but topologically disconnected charts must not become neighbors");
    }

#pragma endregion

// ============================================================
// 06. TestDeterministicResult
// ============================================================
#pragma region "06_TestDeterministicResult"
// 기대 조건: repeated mapping should preserve texel count

    void TestDeterministicResult()
    {
        const MDSS::SurfaceState::FSurfaceMappingData First = BuildFixture("QuadSeam.obj", 24);
        const MDSS::SurfaceState::FSurfaceMappingData Second = BuildFixture("QuadSeam.obj", 24);
        Check(First.Texels.size() == Second.Texels.size(), "repeated mapping should preserve texel count");

        for (std::size_t Index = 0; Index < First.Texels.size(); ++Index)
        {
            const MDSS::SurfaceState::FSurfaceMappingTexel& A = First.Texels[Index];
            const MDSS::SurfaceState::FSurfaceMappingTexel& B = Second.Texels[Index];
            Check(A.Surface == B.Surface && A.Triangle == B.Triangle && A.Chart == B.Chart &&
                      A.Barycentric == B.Barycentric && A.Neighbors == B.Neighbors,
                  "mapping output should be deterministic");
        }
    }

#pragma endregion

// ============================================================
// 07. TestMultipleSurfaceRanges
// ============================================================
#pragma region "07_TestMultipleSurfaceRanges"
// 기대 조건: mapping should retain both Surface ranges

    void TestMultipleSurfaceRanges()
    {
        std::vector<MDSS::Asset::FVertex> Vertices(6);
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

        const std::vector<MDSS::Asset::FMeshTriangleSource> Triangles = {
            {{{0, 1, 2}}, {{0, 1, 2}}, {{0, 1, 2}}, 0},
            {{{3, 4, 5}}, {{3, 4, 5}}, {{3, 4, 5}}, 1},
        };
        const MDSS::SurfaceState::FSurfaceMappingData Mapping =
            MDSS::SurfaceState::FSurfaceMappingBuilder::Build(Vertices, Triangles, {{0, {8, 8}}, {1, {4, 4}}});

        Check(Mapping.Surfaces.size() == 2, "mapping should retain both Surface ranges");
        Check(Mapping.Surfaces[0].FirstTexel == 0 && Mapping.Surfaces[0].TexelCount == 64,
              "first Surface should occupy its 8 by 8 range");
        Check(Mapping.Surfaces[1].FirstTexel == 64 && Mapping.Surfaces[1].TexelCount == 16,
              "second Surface should follow with its 4 by 4 range");
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            if (Mapping.Texels[Index].IsValid())
            {
                const MDSS::SurfaceState::FSurfaceLocalID ExpectedSurface = Index < 64 ? 0 : 1;
                Check(Mapping.Texels[Index].Surface == ExpectedSurface,
                      "valid texel should retain the Surface owning its range");
            }
        }
    }

#pragma endregion

// ============================================================
// 08. TestInvalidFixtures
// ============================================================
#pragma region "08_TestInvalidFixtures"
// 누락·퇴화·비정상 topology 입력의 처리와 mapping 진단을 검사한다.

    void TestInvalidFixtures()
    {
        CheckThrows([] { (void)BuildFixture("DegenerateUV.obj"); }, "UV area", "degenerate UV triangle");
        CheckThrows([] { (void)BuildFixture("Overlap.obj"); }, "UV overlap", "overlapping UV triangles");
        CheckThrows([] { (void)BuildFixture("NonManifold.obj"); }, "non-manifold", "non-manifold mesh edge");
        CheckThrows(
            [] { (void)BuildFixture("MissingUV.obj"); }, "missing a Simulation UV", "OBJ without UV coordinates");
    }

#pragma endregion

// ============================================================
// 09. TestInvariantValidation
// ============================================================
#pragma region "09_TestInvariantValidation"
// 기대 조건: fixture should contain a neighbor pair to corrupt

    void TestInvariantValidation()
    {
        MDSS::SurfaceState::FSurfaceMappingData Mapping = BuildFixture("QuadNoSeam.obj");
        MDSS::SurfaceState::FLocalTexelIndex    Source = MDSS::SurfaceState::InvalidTexelIndex;
        MDSS::SurfaceState::FLocalTexelIndex    Target = MDSS::SurfaceState::InvalidTexelIndex;

        for (std::size_t Index = 0; Index < Mapping.Texels.size() && Source == MDSS::SurfaceState::InvalidTexelIndex;
             ++Index)
        {
            for (const MDSS::SurfaceState::FLocalTexelIndex Neighbor : Mapping.Texels[Index].Neighbors)
            {
                if (Neighbor != MDSS::SurfaceState::InvalidTexelIndex)
                {
                    Source = static_cast<MDSS::SurfaceState::FLocalTexelIndex>(Index);
                    Target = Neighbor;
                    break;
                }
            }
        }

        Check(Source != MDSS::SurfaceState::InvalidTexelIndex, "fixture should contain a neighbor pair to corrupt");

        if (Source == MDSS::SurfaceState::InvalidTexelIndex)
        {
            return;
        }

        for (MDSS::SurfaceState::FLocalTexelIndex& Neighbor : Mapping.Texels[Target].Neighbors)
        {
            if (Neighbor == Source)
            {
                Neighbor = MDSS::SurfaceState::InvalidTexelIndex;
                break;
            }
        }

        CheckThrows([&] { MDSS::SurfaceState::ValidateSurfaceMapping(Mapping); },
                    "bidirectional",
                    "one-way neighbor relationship");
    }
} // namespace

#pragma endregion

// ============================================================
// 10. Test Entry Point
// ============================================================
#pragma region "10_Test_Entry_Point"
// 선택한 회귀 사례를 실행하고 실패를 프로세스 종료 코드로 전달한다.

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

#pragma endregion
