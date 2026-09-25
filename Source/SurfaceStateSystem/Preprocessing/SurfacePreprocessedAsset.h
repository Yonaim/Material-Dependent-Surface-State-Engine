/**
 * @file SurfacePreprocessedAsset.h
 * @brief Static Surface mapping/profile data and versioned binary cache API.
 */

#pragma once

#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingData.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace MDSS
{
    struct SurfaceCacheMetadata
    {
        std::string                    MeshHash;
        std::string                    NormalMapHash;
        std::string                    ProfileMapHash;
        std::uint32_t                  UVSet = 0;
        std::uint32_t                  PreprocessVersion = 1;
        std::uint32_t                  ProfileCount = 0;
        std::vector<SurfaceResolution> GridResolutions;

        [[nodiscard]] bool operator==(const SurfaceCacheMetadata&) const = default;
    };

    /** @brief A `.Surface` cache payload; it contains static data only. */
    struct SurfacePreprocessedAsset
    {
        SurfaceCacheMetadata      Metadata;
        SharedSurfaceGeometryData Geometry;

        SurfacePreprocessedAsset(SurfaceCacheMetadata Metadata, SharedSurfaceGeometryData Geometry);
    };

    class SurfacePreprocessor final
    {
    public:
        /** @brief Create source/input fingerprints used for cache hit validation. */
        [[nodiscard]] static SurfaceCacheMetadata CreateMetadata(const std::filesystem::path&            MeshPath,
                                                                 const std::filesystem::path&            NormalMapPath,
                                                                 const std::vector<SurfaceProfileIndex>& ProfileMap,
                                                                 const std::vector<SurfaceResolution>& GridResolutions,
                                                                 std::uint32_t                         UVSet,
                                                                 std::uint32_t PreprocessVersion,
                                                                 std::uint32_t ProfileCount);

        /** @brief Convert the validated CPU mapping and texel Profile map into static shared data. */
        [[nodiscard]] static SurfacePreprocessedAsset Build(const SurfaceMappingData&        Mapping,
                                                            std::vector<SurfaceProfileIndex> ProfileMap,
                                                            std::uint32_t                    ProfileCount,
                                                            SurfaceCacheMetadata             Metadata);

        /** @brief Hash a file's bytes with deterministic 64-bit FNV-1a (hex encoded). */
        [[nodiscard]] static std::string HashFile(const std::filesystem::path& Path);
        /** @brief Hash the ordered texel Profile IDs, including invalid sentinels. */
        [[nodiscard]] static std::string HashProfileMap(const std::vector<SurfaceProfileIndex>& ProfileMap);
    };

    class SurfaceCache final
    {
    public:
        static constexpr std::uint32_t FormatVersion = 1;

        /** @throws std::runtime_error if the path cannot be created or written. */
        static void Save(const std::filesystem::path& Path, const SurfacePreprocessedAsset& Asset);
        /** @throws std::runtime_error on corruption, unsupported version, or stale metadata. */
        [[nodiscard]] static SurfacePreprocessedAsset Load(const std::filesystem::path& Path,
                                                           const SurfaceCacheMetadata&  ExpectedMetadata);
    };
} // namespace MDSS
