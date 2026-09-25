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
    struct TSurfaceProfileDistribution
    {
        std::vector<std::filesystem::path> ProfilePaths;
        std::vector<TSurfaceProfileIndex>    ProfileIndicesBySurface;

        /** @brief Expand one Profile assignment per Surface to one index per mapping texel. */
        [[nodiscard]] std::vector<TSurfaceProfileIndex> BuildTexelProfileMap(const TSurfaceMappingData& Mapping) const;
    };

    class TSurfaceProfileDistributionLoader final
    {
    public:
        /**
         * @brief Load a version 1 `.SurfaceProfileMap` JSON sidecar.
         * Paths in the file are relative to the sidecar's directory.
         */
        [[nodiscard]] static TSurfaceProfileDistribution Load(const std::filesystem::path& Path);
    };
} // namespace MDSS
