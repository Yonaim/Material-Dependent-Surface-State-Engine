/**
 * @file SurfaceProfileDistributionLoader.h
 * @brief Load sidecar mapping from Mesh Surface IDs to ordered SRProfile assets.
 */

#pragma once

#include "SurfaceStateSystem/Mapping/SurfaceMappingData.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS
{
    struct SurfaceProfileDistribution
    {
        std::vector<std::filesystem::path> ProfilePaths;
        std::vector<SurfaceProfileIndex>    ProfileIndicesBySurface;

        /** @brief Expand one Profile assignment per Surface to one index per mapping texel. */
        [[nodiscard]] std::vector<SurfaceProfileIndex> BuildTexelProfileMap(const SurfaceMappingData& Mapping) const;
    };

    class SurfaceProfileDistributionLoader final
    {
    public:
        /**
         * @brief Load a version 1 `.SurfaceProfileMap` JSON sidecar.
         * Paths in the file are relative to the sidecar's directory.
         */
        [[nodiscard]] static SurfaceProfileDistribution Load(const std::filesystem::path& Path);
    };
} // namespace MDSS
