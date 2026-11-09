/**
 * @file NormalMapTransferTests.cpp
 * @brief Normal Map sample, barycentric UV와 tangent frame 계약을 검증한다.
 */

#include "SurfaceState/Mapping/NormalMapTransferNormalBuilder.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
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

    bool Near(const glm::vec3& Actual, const glm::vec3& Expected, float Epsilon = 0.02F)
    {
        return glm::length(Actual - Expected) <= Epsilon;
    }

    MDSS::Asset::FTextureData
    MakeTexture(std::uint32_t Width, std::uint32_t Height, const std::vector<std::array<std::uint8_t, 4>>& Pixels)
    {
        MDSS::Asset::FTextureData Result;
        Result.Width = Width;
        Result.Height = Height;

        for (const auto& Pixel : Pixels)
        {
            Result.Pixels.insert(Result.Pixels.end(), Pixel.begin(), Pixel.end());
        }

        return Result;
    }

    std::vector<MDSS::Asset::FVertex> MakeVertices()
    {
        std::vector<MDSS::Asset::FVertex> Vertices(3);
        Vertices[0].UV = {0.25F, 0.5F};
        Vertices[1].UV = {0.75F, 0.5F};
        Vertices[2].UV = {0.25F, 0.75F};

        for (MDSS::Asset::FVertex& Vertex : Vertices)
        {
            Vertex.Normal = {0.0F, 0.0F, 1.0F};
            Vertex.Tangent = {1.0F, 0.0F, 0.0F, 1.0F};
        }

        return Vertices;
    }

    MDSS::Asset::FMeshTriangleSource MakeTriangle(std::uint32_t                       FirstVertex,
                                                  MDSS::SurfaceState::FSurfaceLocalID Surface = 0)
    {
        MDSS::Asset::FMeshTriangleSource Triangle;
        Triangle.RenderVertexIndices = {FirstVertex, FirstVertex + 1, FirstVertex + 2};
        Triangle.Surface = Surface;

        return Triangle;
    }

    MDSS::SurfaceState::FSurfaceTexelGeometry MakeTexel(std::uint32_t                       Triangle = 0,
                                                        MDSS::SurfaceState::FSurfaceLocalID Surface = 0,
                                                        glm::vec3 Barycentric = {1.0F, 0.0F, 0.0F})
    {
        MDSS::SurfaceState::FSurfaceTexelGeometry Texel;
        Texel.Surface = Surface;
        Texel.Triangle = Triangle;
        Texel.Barycentric = Barycentric;
        Texel.Normal = {0.0F, 0.0F, 1.0F};

        return Texel;
    }

#pragma endregion

// ============================================================
// 01. TestFlatAndTiltedNormals
// ============================================================
#pragma region 01_TestFlatAndTiltedNormals

    void TestFlatAndTiltedNormals()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const std::vector<FVertex>             Vertices = MakeVertices();
        const std::vector<FMeshTriangleSource> Triangles{MakeTriangle(0)};
        glm::vec3                              Result{};

        const FTextureData Flat = MakeTexture(1, 1, {{{128, 128, 255, 255}}});
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, Flat, Result, false),
              "flat normal map should produce a transfer normal");
        Check(Near(Result, {0.0F, 0.0F, 1.0F}), "flat normal map should match the geometric normal");

        const FTextureData Tilted = MakeTexture(1, 1, {{{255, 128, 128, 255}}});
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, Tilted, Result, false),
              "tilted normal map should produce a transfer normal");
        Check(Result.x > 0.99F && std::abs(Result.z) < 0.02F,
              "tangent-space +X normal should map to the mesh tangent direction");
    }

#pragma endregion

// ============================================================
// 02. TestBarycentricUVAndSeamCharts
// ============================================================
#pragma region 02_TestBarycentricUVAndSeamCharts

    void TestBarycentricUVAndSeamCharts()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        std::vector<FVertex>       Vertices = MakeVertices();
        const std::vector<FVertex> SecondChart = MakeVertices();
        Vertices.insert(Vertices.end(), SecondChart.begin(), SecondChart.end());

        for (FVertex& Vertex : Vertices)
        {
            Vertex.UV.x = 0.25F;
        }

        Vertices[1].UV.x = 0.75F;
        Vertices[3].UV.x = 0.75F;
        Vertices[4].UV.x = 0.75F;
        Vertices[5].UV.x = 0.75F;
        const std::vector<FMeshTriangleSource> Triangles{MakeTriangle(0), MakeTriangle(3)};
        const FTextureData Map = MakeTexture(2, 1, {{{128, 128, 255, 255}}, {{255, 128, 128, 255}}});

        glm::vec3 First{};
        glm::vec3 Second{};
        Check(BuildNormalMapTransferNormal(MakeTexel(0), Vertices, Triangles, Map, First, false),
              "first UV chart should sample successfully");
        Check(BuildNormalMapTransferNormal(MakeTexel(1), Vertices, Triangles, Map, Second, false),
              "second UV chart should sample successfully");
        Check(First.z > 0.99F && Second.x > 0.99F,
              "each texel should sample using its mapped triangle UV, including across a UV seam");

        const glm::vec3 MidBarycentric{0.5F, 0.5F, 0.0F};
        glm::vec3       Mid{};
        Check(BuildNormalMapTransferNormal(MakeTexel(0, 0, MidBarycentric), Vertices, Triangles, Map, Mid, false),
              "barycentrically interpolated UV should sample successfully");
        Check(Mid.x > 0.6F && Mid.z > 0.6F,
              "normal map sample should use barycentric UV interpolation (result: " + std::to_string(Mid.x) + ", " +
                  std::to_string(Mid.y) + ", " + std::to_string(Mid.z) + ")");
    }

#pragma endregion

// ============================================================
// 03. TestTangentHandednessAndRepeat
// ============================================================
#pragma region 03_TestTangentHandednessAndRepeat

    void TestTangentHandednessAndRepeat()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        std::vector<FVertex>                   Vertices = MakeVertices();
        const std::vector<FMeshTriangleSource> Triangles{MakeTriangle(0)};
        const FTextureData                     PositiveY = MakeTexture(1, 1, {{{128, 255, 128, 255}}});
        glm::vec3                              Positive{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Positive, false),
              "positive-handed tangent frame should produce a normal");
        Check(Positive.y > 0.99F, "positive tangent handedness should preserve tangent-space +Y");

        glm::vec3 Flipped{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Flipped, true),
              "flipped normal-map Y should produce a normal");
        Check(Flipped.y < -0.99F, "Y flip should match the rendered normal-map convention");

        for (FVertex& Vertex : Vertices)
        {
            Vertex.Tangent.w = -1.0F;
            Vertex.UV.x = -0.25F; // repeat addressing wraps this coordinate into the one-pixel texture
        }

        glm::vec3 Mirrored{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Mirrored, false),
              "mirrored tangent frame and repeated UV should produce a normal");
        Check(Mirrored.y < -0.99F, "negative tangent handedness should flip the bitangent direction");
    }

#pragma endregion

// ============================================================
// 04. TestInvalidInputsUseFallbackSignal
// ============================================================
#pragma region 04_TestInvalidInputsUseFallbackSignal

    void TestInvalidInputsUseFallbackSignal()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        const std::vector<FVertex>             Vertices = MakeVertices();
        const std::vector<FMeshTriangleSource> Triangles{MakeTriangle(0)};
        const FTextureData                     Flat = MakeTexture(1, 1, {{{128, 128, 255, 255}}});
        glm::vec3                              Result{};

        std::vector<FVertex> DegenerateTangent = Vertices;

        for (FVertex& Vertex : DegenerateTangent)
        {
            Vertex.Tangent = {0.0F, 0.0F, 1.0F, 1.0F};
        }

        Check(!BuildNormalMapTransferNormal(MakeTexel(), DegenerateTangent, Triangles, Flat, Result, false),
              "degenerate tangent should request geometric-normal fallback");

        const FTextureData InvalidTexture{};
        Check(!BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, InvalidTexture, Result, false),
              "missing or malformed texture data should request geometric-normal fallback");
        std::vector<FVertex> MidUVVertices = Vertices;

        for (FVertex& Vertex : MidUVVertices)
        {
            Vertex.UV.x = 0.5F;
        }

        const FTextureData ZeroLengthSample = MakeTexture(2, 1, {{{127, 127, 127, 255}}, {{128, 128, 128, 255}}});
        Check(!BuildNormalMapTransferNormal(MakeTexel(), MidUVVertices, Triangles, ZeroLengthSample, Result, false),
              "zero-length filtered normal sample should request geometric-normal fallback");
        Check(!BuildNormalMapTransferNormal(MakeTexel(1), Vertices, Triangles, Flat, Result, false),
              "missing triangle mapping should request geometric-normal fallback");
        Check(!BuildNormalMapTransferNormal(MakeTexel(0, 1), Vertices, Triangles, Flat, Result, false),
              "surface mismatch should request geometric-normal fallback");
    }
} // namespace

#pragma endregion

// ============================================================
// 05. Test Entry Point
// ============================================================
#pragma region 05_Test_Entry_Point

int main()
{
    TestFlatAndTiltedNormals();
    TestBarycentricUVAndSeamCharts();
    TestTangentHandednessAndRepeat();
    TestInvalidInputsUseFallbackSignal();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " Normal Map transfer test(s) failed.\n";
        return 1;
    }

    std::cout << "All Normal Map transfer tests passed.\n";
    return 0;
}

#pragma endregion
