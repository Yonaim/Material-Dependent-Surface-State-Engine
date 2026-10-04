/**
 * @file SurfaceProfileDistributionLoader.h
 * @brief Mesh Surface별 SRProfile 분포 sidecar 로더 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/Mapping/SurfaceMappingData.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace MDSS::Asset
{
    struct TSurfaceProfileDistribution
    {
        std::vector<std::filesystem::path> ProfilePaths;
        /** @brief Per-Surface Profile index; SurfaceState::InvalidSurfaceProfileIndex means render-only/no simulation. */
        std::vector<SurfaceState::TSurfaceProfileIndex> ProfileIndicesBySurface;

        /** @brief Expand one Profile assignment per Surface to one index per mapping texel. */
        [[nodiscard]] std::vector<SurfaceState::TSurfaceProfileIndex> BuildTexelProfileMap(const SurfaceState::TSurfaceMappingData& Mapping) const;
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
} // namespace MDSS::Asset
