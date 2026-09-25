/**
 * @file SurfaceGeometryBuilder.cpp
 * @brief Build shared CPU geometry and texel Profile mapping from Surface mapping.
 */

#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"

#include "SurfaceStateSystem/Mapping/SurfaceMappingValidation.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    namespace
    {
        bool IsFinite(const glm::vec3& Value)
        {
            return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
        }
    } // namespace

    TSharedSurfaceGeometryData TSurfaceGeometryBuilder::Build(const TSurfaceMappingData&        Mapping,
                                                            std::vector<TSurfaceProfileIndex> ProfileMap,
                                                            std::uint32_t                    ProfileCount)
    {
        ValidateSurfaceMapping(Mapping);
        if (Mapping.Surfaces.empty())
        {
            throw std::invalid_argument("TSurfaceGeometryBuilder requires at least one Surface.");
        }
        if (ProfileCount == 0)
        {
            throw std::invalid_argument("TSurfaceGeometryBuilder requires at least one Profile.");
        }
        if (ProfileMap.size() != Mapping.Texels.size())
        {
            throw std::invalid_argument("ProfileMap must provide exactly one entry per mapping texel.");
        }

        std::vector<TSurfaceDefinition> Definitions;
        Definitions.reserve(Mapping.Surfaces.size());
        for (const TSurfaceTexelRange& Surface : Mapping.Surfaces)
        {
            Definitions.push_back({Surface.Surface, Surface.Resolution});
        }

        TSharedSurfaceGeometryData Geometry(std::move(Definitions));
        std::vector<TSurfaceTexelGeometry>& GeometryTexels = Geometry.GetTexels();
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const TSurfaceMappingTexel& Source = Mapping.Texels[Index];
            TSurfaceTexelGeometry&      Target = GeometryTexels[Index];
            if (!Source.IsValid())
            {
                if (ProfileMap[Index] != InvalidSurfaceProfileIndex)
                {
                    throw std::invalid_argument("Invalid texels must use InvalidSurfaceProfileIndex.");
                }
                continue;
            }

            if (ProfileMap[Index] == InvalidSurfaceProfileIndex || ProfileMap[Index] >= ProfileCount)
            {
                throw std::invalid_argument("Valid texel Profile index is outside the loaded Profile range.");
            }
            if (!IsFinite(Source.Position) || !IsFinite(Source.Normal) || !IsFinite(Source.Barycentric))
            {
                throw std::invalid_argument("Surface mapping contains non-finite texel geometry.");
            }
            const float NormalLength = glm::length(Source.Normal);
            if (!std::isfinite(NormalLength) || std::abs(NormalLength - 1.0F) > 1.0e-3F)
            {
                throw std::invalid_argument("Valid texel Normal must be normalized.");
            }

            Target.Surface = Source.Surface;
            Target.Triangle = Source.Triangle;
            Target.Barycentric = Source.Barycentric;
            Target.Position = Source.Position;
            Target.Normal = Source.Normal;
            Target.NeighborIndices = Source.Neighbors;
            // Geometry fields retain their declared zero defaults until their preprocessing is implemented.
        }

        Geometry.SetProfileMap(std::move(ProfileMap));
        return Geometry;
    }
} // namespace MDSS
