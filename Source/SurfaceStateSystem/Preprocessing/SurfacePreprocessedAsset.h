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

        /** @brief Create a fingerprint from the ordered per-Surface normal-map inputs. */
        [[nodiscard]] static SurfaceCacheMetadata CreateMetadataForNormalMaps(
            const std::filesystem::path&                  MeshPath,
            const std::vector<std::filesystem::path>&    NormalMapPaths,
            const std::vector<SurfaceProfileIndex>&     ProfileMap,
            const std::vector<SurfaceResolution>&       GridResolutions,
            std::uint32_t                               UVSet,
            std::uint32_t                               PreprocessVersion,
            std::uint32_t                               ProfileCount);

        /** @brief Create source fingerprints before rasterization; the texel Profile hash is checked after cache load. */
        [[nodiscard]] static SurfaceCacheMetadata CreateSourceMetadata(
            const std::filesystem::path&               MeshPath,
            const std::vector<std::filesystem::path>& NormalMapPaths,
            const std::vector<SurfaceResolution>&     GridResolutions,
            std::uint32_t                             UVSet,
            std::uint32_t                             PreprocessVersion,
            std::uint32_t                             ProfileCount);

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
        static constexpr std::uint32_t FormatVersion = 2;

        /** @brief Build the stable one-cache-per-Mesh path, preserving its project-relative directories. */
        [[nodiscard]] static std::filesystem::path GetPath(const std::filesystem::path& MeshPath,
                                                            const std::filesystem::path& ProjectRoot,
                                                            const std::filesystem::path& CacheRoot);
        /** @throws std::runtime_error if the path cannot be created or written. */
        static void Save(const std::filesystem::path& Path, const SurfacePreprocessedAsset& Asset);
        /**
         * @brief Load and validate a cache. An empty ExpectedMetadata.ProfileMapHash skips input-hash equality only;
         *        the stored map is still checked against its serialized hash and callers must validate its assignments.
         * @throws std::runtime_error on corruption, unsupported version, or stale metadata.
         */
        [[nodiscard]] static SurfacePreprocessedAsset Load(const std::filesystem::path& Path,
                                                           const SurfaceCacheMetadata&  ExpectedMetadata);
    };
} // namespace MDSS
