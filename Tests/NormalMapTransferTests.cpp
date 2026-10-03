/**
 * @file NormalMapTransferTests.cpp
 * @brief Normal Map sample, barycentric UV와 tangent frame 계약을 검증한다.
 */

#include "SurfaceStateSystem/Mapping/NormalMapTransferNormalBuilder.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
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

    bool Near(const glm::vec3& Actual, const glm::vec3& Expected, float Epsilon = 0.02F)
    {
        return glm::length(Actual - Expected) <= Epsilon;
    }

    MDSS::TextureData
    MakeTexture(std::uint32_t Width, std::uint32_t Height, const std::vector<std::array<std::uint8_t, 4>>& Pixels)
    {
        MDSS::TextureData Result;
        Result.Width = Width;
        Result.Height = Height;
        for (const auto& Pixel : Pixels)
        {
            Result.Pixels.insert(Result.Pixels.end(), Pixel.begin(), Pixel.end());
        }
        return Result;
    }

    std::vector<MDSS::TVertex> MakeVertices()
    {
        std::vector<MDSS::TVertex> Vertices(3);
        Vertices[0].UV = {0.25F, 0.5F};
        Vertices[1].UV = {0.75F, 0.5F};
        Vertices[2].UV = {0.25F, 0.75F};
        for (MDSS::TVertex& Vertex : Vertices)
        {
            Vertex.Normal = {0.0F, 0.0F, 1.0F};
            Vertex.Tangent = {1.0F, 0.0F, 0.0F, 1.0F};
        }
        return Vertices;
    }

    MDSS::TMeshTriangleSource MakeTriangle(std::uint32_t FirstVertex, MDSS::TSurfaceLocalID Surface = 0)
    {
        MDSS::TMeshTriangleSource Triangle;
        Triangle.RenderVertexIndices = {FirstVertex, FirstVertex + 1, FirstVertex + 2};
        Triangle.Surface = Surface;
        return Triangle;
    }

    MDSS::TSurfaceTexelGeometry
    MakeTexel(std::uint32_t Triangle = 0, MDSS::TSurfaceLocalID Surface = 0, glm::vec3 Barycentric = {1.0F, 0.0F, 0.0F})
    {
        MDSS::TSurfaceTexelGeometry Texel;
        Texel.Surface = Surface;
        Texel.Triangle = Triangle;
        Texel.Barycentric = Barycentric;
        Texel.Normal = {0.0F, 0.0F, 1.0F};
        return Texel;
    }

    void TestFlatAndTiltedNormals()
    {
        using namespace MDSS;
        const std::vector<TVertex>             Vertices = MakeVertices();
        const std::vector<TMeshTriangleSource> Triangles{MakeTriangle(0)};
        glm::vec3                              Result{};

        const TextureData Flat = MakeTexture(1, 1, {{{128, 128, 255, 255}}});
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, Flat, Result, false),
              "flat normal map should produce a transfer normal");
        Check(Near(Result, {0.0F, 0.0F, 1.0F}), "flat normal map should match the geometric normal");

        const TextureData Tilted = MakeTexture(1, 1, {{{255, 128, 128, 255}}});
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, Tilted, Result, false),
              "tilted normal map should produce a transfer normal");
        Check(Result.x > 0.99F && std::abs(Result.z) < 0.02F,
              "tangent-space +X normal should map to the mesh tangent direction");
    }

    void TestBarycentricUVAndSeamCharts()
    {
        using namespace MDSS;
        std::vector<TVertex>       Vertices = MakeVertices();
        const std::vector<TVertex> SecondChart = MakeVertices();
        Vertices.insert(Vertices.end(), SecondChart.begin(), SecondChart.end());
        for (TVertex& Vertex : Vertices)
        {
            Vertex.UV.x = 0.25F;
        }
        Vertices[1].UV.x = 0.75F;
        Vertices[3].UV.x = 0.75F;
        Vertices[4].UV.x = 0.75F;
        Vertices[5].UV.x = 0.75F;
        const std::vector<TMeshTriangleSource> Triangles{MakeTriangle(0), MakeTriangle(3)};
        const TextureData Map = MakeTexture(2, 1, {{{128, 128, 255, 255}}, {{255, 128, 128, 255}}});

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

    void TestTangentHandednessAndRepeat()
    {
        using namespace MDSS;
        std::vector<TVertex>                   Vertices = MakeVertices();
        const std::vector<TMeshTriangleSource> Triangles{MakeTriangle(0)};
        const TextureData                      PositiveY = MakeTexture(1, 1, {{{128, 255, 128, 255}}});
        glm::vec3                              Positive{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Positive, false),
              "positive-handed tangent frame should produce a normal");
        Check(Positive.y > 0.99F, "positive tangent handedness should preserve tangent-space +Y");

        glm::vec3 Flipped{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Flipped, true),
              "flipped normal-map Y should produce a normal");
        Check(Flipped.y < -0.99F, "Y flip should match the rendered normal-map convention");

        for (TVertex& Vertex : Vertices)
        {
            Vertex.Tangent.w = -1.0F;
            Vertex.UV.x = -0.25F; // repeat addressing wraps this coordinate into the one-pixel texture
        }
        glm::vec3 Mirrored{};
        Check(BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, PositiveY, Mirrored, false),
              "mirrored tangent frame and repeated UV should produce a normal");
        Check(Mirrored.y < -0.99F, "negative tangent handedness should flip the bitangent direction");
    }

    void TestInvalidInputsUseFallbackSignal()
    {
        using namespace MDSS;
        const std::vector<TVertex>             Vertices = MakeVertices();
        const std::vector<TMeshTriangleSource> Triangles{MakeTriangle(0)};
        const TextureData                      Flat = MakeTexture(1, 1, {{{128, 128, 255, 255}}});
        glm::vec3                              Result{};

        std::vector<TVertex> DegenerateTangent = Vertices;
        for (TVertex& Vertex : DegenerateTangent)
        {
            Vertex.Tangent = {0.0F, 0.0F, 1.0F, 1.0F};
        }
        Check(!BuildNormalMapTransferNormal(MakeTexel(), DegenerateTangent, Triangles, Flat, Result, false),
              "degenerate tangent should request geometric-normal fallback");

        const TextureData InvalidTexture{};
        Check(!BuildNormalMapTransferNormal(MakeTexel(), Vertices, Triangles, InvalidTexture, Result, false),
              "missing or malformed texture data should request geometric-normal fallback");
        std::vector<TVertex> MidUVVertices = Vertices;
        for (TVertex& Vertex : MidUVVertices)
        {
            Vertex.UV.x = 0.5F;
        }
        const TextureData ZeroLengthSample = MakeTexture(2, 1, {{{127, 127, 127, 255}}, {{128, 128, 128, 255}}});
        Check(!BuildNormalMapTransferNormal(MakeTexel(), MidUVVertices, Triangles, ZeroLengthSample, Result, false),
              "zero-length filtered normal sample should request geometric-normal fallback");
        Check(!BuildNormalMapTransferNormal(MakeTexel(1), Vertices, Triangles, Flat, Result, false),
              "missing triangle mapping should request geometric-normal fallback");
        Check(!BuildNormalMapTransferNormal(MakeTexel(0, 1), Vertices, Triangles, Flat, Result, false),
              "surface mismatch should request geometric-normal fallback");
    }
} // namespace

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
