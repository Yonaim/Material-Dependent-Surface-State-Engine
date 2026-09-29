#include "SurfaceStateSystem/Geometry/SurfaceTexelMeshBuilder.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace MDSS
{
    TSurfaceTexelMesh BuildSurfaceTexelMesh(const TSharedSurfaceGeometryData& Geometry)
    {
        TSurfaceTexelMesh Result;
        const auto& Texels = Geometry.GetTexels();
        const auto Finite = [](glm::vec3 V) {
            return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z);
        };
        const auto Emit = [&](std::uint32_t A, std::uint32_t B, std::uint32_t C, TSurfaceLocalID Surface) {
            const auto& X = Texels[A];
            const auto& Y = Texels[B];
            const auto& Z = Texels[C];
            for (const auto* Texel : {&X, &Y, &Z})
                if (!Texel->IsValid() || Texel->Surface != Surface ||
                    Texel->Chart == std::numeric_limits<std::uint32_t>::max() ||
                    !Finite(Texel->Position) || !Finite(Texel->Normal)) return;
            if (X.Chart != Y.Chart || X.Chart != Z.Chart) return;
            const glm::vec3 AB = Y.Position - X.Position, AC = Z.Position - X.Position;
            const glm::vec3 Cross = glm::cross(AB, AC);
            const double AreaSquared = glm::dot(glm::dvec3(Cross), glm::dvec3(Cross));
            const double EdgeProduct = glm::dot(glm::dvec3(AB), glm::dvec3(AB)) *
                                       glm::dot(glm::dvec3(AC), glm::dvec3(AC));
            if (EdgeProduct <= 0.0 || AreaSquared <= 1e-12 * EdgeProduct) return;
            // A mirrored UV chart must retain the geometric front-face winding.
            if (glm::dot(Cross, X.Normal + Y.Normal + Z.Normal) < 0.0F) std::swap(B, C);
            if (Result.Indices.size() > std::numeric_limits<std::uint32_t>::max() - 3U)
                throw std::overflow_error("Texel mesh exceeds the Vulkan index-count range.");
            Result.Indices.insert(Result.Indices.end(), {A, B, C});
        };
        for (const auto& Range : Geometry.GetSurfaces())
        {
            const auto First = static_cast<std::uint32_t>(Result.Indices.size());
            for (std::uint32_t Y = 0; Y + 1 < Range.Resolution.Height; ++Y)
                for (std::uint32_t X = 0; X + 1 < Range.Resolution.Width; ++X)
                {
                    const std::uint32_t A = Range.FirstTexel + Y * Range.Resolution.Width + X;
                    const std::uint32_t B = A + 1, D = A + Range.Resolution.Width, C = D + 1;
                    // Prefer the diagonal that retains more valid, same-chart triangles.
                    const auto Compatible = [&](std::uint32_t I, std::uint32_t J, std::uint32_t K) {
                        return Texels[I].IsValid() && Texels[J].IsValid() && Texels[K].IsValid() &&
                               Texels[I].Chart == Texels[J].Chart && Texels[I].Chart == Texels[K].Chart;
                    };
                    const int ACCount = int(Compatible(A, B, C)) + int(Compatible(A, C, D));
                    const int BDCount = int(Compatible(A, B, D)) + int(Compatible(B, C, D));
                    const auto DistanceSquared = [&](std::uint32_t I, std::uint32_t J) {
                        const auto Delta = Texels[I].Position - Texels[J].Position;
                        return glm::dot(Delta, Delta);
                    };
                    if (BDCount > ACCount || (BDCount == ACCount && DistanceSquared(B, D) < DistanceSquared(A, C)))
                    {
                        Emit(A, B, D, Range.Surface);
                        Emit(B, C, D, Range.Surface);
                    }
                    else
                    {
                        Emit(A, B, C, Range.Surface);
                        Emit(A, C, D, Range.Surface);
                    }
                }
            Result.Surfaces.push_back({First, static_cast<std::uint32_t>(Result.Indices.size()) - First});
        }
        return Result;
    }
}
