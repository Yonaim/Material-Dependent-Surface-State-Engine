/**
 * @file NormalMapTransferNormalBuilder.cpp
 * @brief Normal Map sample과 tangent frame 변환으로 solver 전달 normal을 계산한다.
 */

#include "SurfaceState/Mapping/NormalMapTransferNormalBuilder.h"

#include <cmath>
#include <cstdint>

namespace MDSS::SurfaceState
{
#pragma region Normal_Map_Sampling_Helpers

    namespace
    {
        constexpr float NormalMapEpsilon = 1.0e-8F;

        std::int64_t WrapTextureCoordinate(std::int64_t Coordinate, std::uint32_t Extent)
        {
            const std::int64_t SignedExtent = static_cast<std::int64_t>(Extent);
            const std::int64_t Remainder = Coordinate % SignedExtent;
            return Remainder < 0 ? Remainder + SignedExtent : Remainder;
        }

        bool SampleNormalMapLinearRepeat(const Asset::TextureData& Texture, const glm::vec2& UV, glm::vec3& OutNormal)
        {
            if (Texture.Width == 0 || Texture.Height == 0 ||
                Texture.Pixels.size() != static_cast<std::size_t>(Texture.Width) * Texture.Height * 4U ||
                !std::isfinite(UV.x) || !std::isfinite(UV.y))
            {
                return false;
            }

            const float X = UV.x * static_cast<float>(Texture.Width) - 0.5F;
            const float Y = UV.y * static_cast<float>(Texture.Height) - 0.5F;
            const float FloorX = std::floor(X);
            const float FloorY = std::floor(Y);
            const auto  X0 = static_cast<std::int64_t>(FloorX);
            const auto  Y0 = static_cast<std::int64_t>(FloorY);
            const float TX = X - FloorX;
            const float TY = Y - FloorY;

            const auto ReadNormal = [&](std::int64_t PixelX, std::int64_t PixelY)
            {
                const std::size_t WrappedX = static_cast<std::size_t>(WrapTextureCoordinate(PixelX, Texture.Width));
                const std::size_t WrappedY = static_cast<std::size_t>(WrapTextureCoordinate(PixelY, Texture.Height));
                const std::size_t Offset = (WrappedY * Texture.Width + WrappedX) * 4U;
                return glm::vec3(Texture.Pixels[Offset], Texture.Pixels[Offset + 1U], Texture.Pixels[Offset + 2U]) /
                           127.5F -
                       glm::vec3(1.0F);
            };

            const glm::vec3 Top = glm::mix(ReadNormal(X0, Y0), ReadNormal(X0 + 1, Y0), TX);
            const glm::vec3 Bottom = glm::mix(ReadNormal(X0, Y0 + 1), ReadNormal(X0 + 1, Y0 + 1), TX);
            const glm::vec3 Sample = glm::mix(Top, Bottom, TY);
            const float     SampleLengthSquared = glm::dot(Sample, Sample);
            if (!std::isfinite(SampleLengthSquared) || SampleLengthSquared <= NormalMapEpsilon)
            {
                return false;
            }
            OutNormal = Sample / std::sqrt(SampleLengthSquared);
            return true;
        }
    } // namespace
#pragma endregion

#pragma region Transfer_Normal_Build

    bool BuildNormalMapTransferNormal(const TSurfaceTexelGeometry&                   Texel,
                                      const std::vector<Asset::TVertex>&             Vertices,
                                      const std::vector<Asset::TMeshTriangleSource>& Triangles,
                                      const Asset::TextureData&                      NormalMap,
                                      glm::vec3&                                     OutTransferNormal,
                                      bool                                           bFlipNormalY)
    {
        if (!Texel.IsValid() || Texel.Triangle >= Triangles.size())
        {
            return false;
        }

        const Asset::TMeshTriangleSource& Triangle = Triangles[Texel.Triangle];
        const auto&                       VertexIndices = Triangle.RenderVertexIndices;
        if (Triangle.Surface != Texel.Surface || VertexIndices[0] >= Vertices.size() ||
            VertexIndices[1] >= Vertices.size() || VertexIndices[2] >= Vertices.size())
        {
            return false;
        }

        const Asset::TVertex& V0 = Vertices[VertexIndices[0]];
        const Asset::TVertex& V1 = Vertices[VertexIndices[1]];
        const Asset::TVertex& V2 = Vertices[VertexIndices[2]];
        const glm::vec2 UV = Texel.Barycentric.x * V0.UV + Texel.Barycentric.y * V1.UV + Texel.Barycentric.z * V2.UV;
        if (!std::isfinite(UV.x) || !std::isfinite(UV.y))
        {
            return false;
        }

        const glm::vec3 Normal = Texel.Normal;
        glm::vec3 Tangent = Texel.Barycentric.x * glm::vec3(V0.Tangent) + Texel.Barycentric.y * glm::vec3(V1.Tangent) +
                            Texel.Barycentric.z * glm::vec3(V2.Tangent);
        Tangent -= Normal * glm::dot(Normal, Tangent);
        const float TangentLengthSquared = glm::dot(Tangent, Tangent);
        const float Handedness = Texel.Barycentric.x * V0.Tangent.w + Texel.Barycentric.y * V1.Tangent.w +
                                 Texel.Barycentric.z * V2.Tangent.w;
        if (!std::isfinite(TangentLengthSquared) || TangentLengthSquared <= NormalMapEpsilon ||
            !std::isfinite(Handedness) || std::abs(Handedness) <= NormalMapEpsilon)
        {
            return false;
        }

        Tangent /= std::sqrt(TangentLengthSquared);
        const glm::vec3 Bitangent = glm::normalize(glm::cross(Normal, Tangent)) * (Handedness < 0.0F ? -1.0F : 1.0F);
        glm::vec3       NormalTS{};
        if (!SampleNormalMapLinearRepeat(NormalMap, UV, NormalTS))
        {
            return false;
        }
        if (bFlipNormalY)
        {
            NormalTS.y = -NormalTS.y;
        }

        const glm::vec3 NormalLocal = Tangent * NormalTS.x + Bitangent * NormalTS.y + Normal * NormalTS.z;
        const float     NormalLengthSquared = glm::dot(NormalLocal, NormalLocal);
        if (!std::isfinite(NormalLengthSquared) || NormalLengthSquared <= NormalMapEpsilon)
        {
            return false;
        }
        OutTransferNormal = NormalLocal / std::sqrt(NormalLengthSquared);
        return true;
    }
#pragma endregion
} // namespace MDSS::SurfaceState
