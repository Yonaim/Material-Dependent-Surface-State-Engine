/**
 * @file SurfaceProfileDistributionLoader.h
 * @brief Mesh Surface별 SRProfile 분포 sidecar 로더 인터페이스를 선언한다.
 */

#pragma once

#include "SurfaceState/Mapping/SurfaceMappingData.h"
#include "SurfaceState/Types/SurfaceStateTypes.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <vector>

namespace MDSS::Asset
{
    /** @brief 빈 경로는 해당 표면을 시뮬레이션 대상에서 제외한다. */
    using FSurfaceProfileOverrides = std::map<SurfaceState::FSurfaceLocalID, std::filesystem::path>;

    struct FSurfaceProfileDistribution
    {
        std::vector<std::filesystem::path> ProfilePaths;
        /** @brief Per-Surface Profile index; SurfaceState::InvalidSurfaceProfileIndex means render-only/no simulation.
         */
        std::vector<SurfaceState::FSurfaceProfileIndex> ProfileIndicesBySurface;

        /** @brief Expand one Profile assignment per Surface to one index per mapping texel. */
        [[nodiscard]] std::vector<SurfaceState::FSurfaceProfileIndex>
        BuildTexelProfileMap(const SurfaceState::FSurfaceMappingData &Mapping) const;
    };

    class FSurfaceProfileDistributionLoader final
    {
      public:
        /**
         * @brief Load a version 1 `.SurfaceProfileMap` JSON sidecar.
         * Paths in the file are relative to the sidecar's directory.
         */
        [[nodiscard]] static FSurfaceProfileDistribution Load(const std::filesystem::path &Path);
    };
} // namespace MDSS::Asset
