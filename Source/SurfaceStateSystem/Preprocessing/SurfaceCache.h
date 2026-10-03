/**
 * @file SurfaceCache.h
 * @brief 버전 및 해상도별 최종 CPU Surface geometry cache 인터페이스를 선언한다.
 */

#pragma once

#include "AssetManager/Assets/MeshSourceData.h"
#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>

namespace MDSS
{
    struct TSurfaceCacheDescriptor
    {
        std::uint64_t Fingerprint = 0;
        std::vector<TSurfaceDefinition> Surfaces;
        std::vector<std::filesystem::path> ProfilePaths;
        std::uint32_t TriangleCount = 0;
    };

    class TSurfaceCache final
    {
    public:
        // Bump whenever mapping, sampling, integration or derivative rules change.
        static constexpr std::uint32_t PreprocessVersion = 4;
        // Version 4 includes local texel area vectors; older caches are rebuilt.
        static constexpr std::uint32_t FormatVersion = 4;

        /** @brief Fingerprint parsed mesh inputs, Normal Map bytes and ordered Profile assignments.
         * SRProfile response parameters and Registry channels are deliberately excluded.
         */
        [[nodiscard]] static TSurfaceCacheDescriptor Describe(
            std::span<const TVertex> Vertices,
            std::span<const TMeshTriangleSource> Triangles,
            std::vector<TSurfaceDefinition> Surfaces,
            std::span<const std::filesystem::path> NormalMapPaths,
            const std::filesystem::path& DistributionPath,
            std::vector<std::filesystem::path> ProfilePaths,
            std::span<const TSurfaceProfileIndex> ProfileIndicesBySurface);

        /** @brief Stable Mesh/Map identity directory; retain separate resolution variants. */
        [[nodiscard]] static std::filesystem::path GetPath(
            const std::filesystem::path& CacheRoot,
            const std::filesystem::path& MeshPath,
            const std::filesystem::path& DistributionPath,
            std::uint32_t Resolution);

        /** @brief Missing, stale or corrupt cache returns nullopt with a diagnostic. */
        [[nodiscard]] static std::optional<TSharedSurfaceGeometryData> Load(
            const std::filesystem::path& Path, const TSurfaceCacheDescriptor& Expected, std::string& Diagnostic);

        /** @brief Write explicit little-endian fields to a temporary file, then rename into place.
         * Throws on failure; callers may continue using the freshly built Runtime data.
         */
        static void Save(const std::filesystem::path& Path,
                         const TSurfaceCacheDescriptor& Descriptor,
                         const TSharedSurfaceGeometryData& Geometry);
    };
} // namespace MDSS
