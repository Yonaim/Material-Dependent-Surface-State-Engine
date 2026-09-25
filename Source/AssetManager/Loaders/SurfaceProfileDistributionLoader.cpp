/**
 * @file SurfaceProfileDistributionLoader.cpp
 * @brief Load sidecar mapping from Mesh Surface IDs to ordered SRProfile assets.
 */

#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace MDSS
{
    namespace
    {
        using Json = nlohmann::json;

        const Json& RequireMember(const Json& Object, const char* Name, const std::string& Context)
        {
            const auto Found = Object.find(Name);
            if (Found == Object.end())
            {
                throw std::runtime_error(Context + " is missing required field '" + Name + "'.");
            }
            return *Found;
        }

        std::uint32_t ReadNonNegativeIndex(const Json& Value, const std::string& Name)
        {
            if (!Value.is_number_integer())
            {
                throw std::runtime_error("Surface Profile Map '" + Name + "' must be a non-negative integer.");
            }
            if (Value.is_number_unsigned())
            {
                const std::uint64_t Parsed = Value.get<std::uint64_t>();
                if (Parsed > std::numeric_limits<std::uint32_t>::max())
                {
                    throw std::runtime_error("Surface Profile Map '" + Name +
                                             "' is outside the supported integer range.");
                }
                return static_cast<std::uint32_t>(Parsed);
            }

            const std::int64_t Parsed = Value.get<std::int64_t>();
            if (Parsed < 0 || static_cast<std::uint64_t>(Parsed) > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("Surface Profile Map '" + Name +
                                         "' is outside the supported non-negative integer range.");
            }
            return static_cast<std::uint32_t>(Parsed);
        }
    } // namespace

    SurfaceProfileDistribution SurfaceProfileDistributionLoader::Load(const std::filesystem::path& Path)
    {
        std::ifstream Input(Path);
        if (!Input)
        {
            throw std::runtime_error("Unable to open Surface Profile Map: " + Path.string());
        }

        Json Root;
        try
        {
            Input >> Root;
        }
        catch (const Json::exception& Exception)
        {
            throw std::runtime_error("Invalid Surface Profile Map JSON in '" + Path.string() + "': " + Exception.what());
        }
        if (!Root.is_object())
        {
            throw std::runtime_error("Surface Profile Map root must be a JSON object.");
        }
        const Json& Type = RequireMember(Root, "type", "Surface Profile Map");
        const Json& Version = RequireMember(Root, "version", "Surface Profile Map");
        if (!Type.is_string() || Type.get<std::string>() != "SurfaceProfileMap")
        {
            throw std::runtime_error("Surface Profile Map type must be 'SurfaceProfileMap'.");
        }
        if (ReadNonNegativeIndex(Version, "version") != 1)
        {
            throw std::runtime_error("Surface Profile Map version must be the integer 1.");
        }

        const Json& Profiles = RequireMember(Root, "profiles", "Surface Profile Map");
        const Json& Surfaces = RequireMember(Root, "surfaces", "Surface Profile Map");
        if (!Profiles.is_array() || Profiles.empty())
        {
            throw std::runtime_error("Surface Profile Map 'profiles' must be a non-empty array.");
        }
        if (!Surfaces.is_array() || Surfaces.empty())
        {
            throw std::runtime_error("Surface Profile Map 'surfaces' must be a non-empty array.");
        }
        if (Profiles.size() >= InvalidSurfaceProfileIndex || Surfaces.size() >= InvalidSurfaceID)
        {
            throw std::runtime_error("Surface Profile Map exceeds the supported index range.");
        }

        SurfaceProfileDistribution Result;
        Result.ProfilePaths.reserve(Profiles.size());
        std::unordered_set<std::string> UniquePaths;
        for (std::size_t Index = 0; Index < Profiles.size(); ++Index)
        {
            if (!Profiles[Index].is_string())
            {
                throw std::runtime_error("Surface Profile Map profile path at index " + std::to_string(Index) +
                                         " must be a string.");
            }
            const std::string ProfilePathString = Profiles[Index].get<std::string>();
            if (ProfilePathString.empty())
            {
                throw std::runtime_error("Surface Profile Map profile paths cannot be empty.");
            }
            std::filesystem::path ProfilePath(ProfilePathString);
            if (ProfilePath.is_absolute())
            {
                throw std::runtime_error("Surface Profile Map profile paths must be relative to the sidecar.");
            }
            if (ProfilePath.extension() != ".SRProfile")
            {
                throw std::runtime_error("Surface Profile Map profile paths must use the .SRProfile extension.");
            }
            ProfilePath = (Path.parent_path() / ProfilePath).lexically_normal();
            if (!UniquePaths.emplace(ProfilePath.generic_string()).second)
            {
                throw std::runtime_error("Surface Profile Map contains a duplicate Profile path.");
            }
            Result.ProfilePaths.push_back(std::move(ProfilePath));
        }

        std::vector<SurfaceLocalID> SurfaceIDs;
        SurfaceIDs.reserve(Surfaces.size());
        for (std::size_t EntryIndex = 0; EntryIndex < Surfaces.size(); ++EntryIndex)
        {
            const Json& Entry = Surfaces[EntryIndex];
            if (!Entry.is_object())
            {
                throw std::runtime_error("Surface Profile Map surface entry must be an object.");
            }
            const Json& SurfaceId = RequireMember(Entry, "surfaceId", "Surface Profile Map surface entry");
            const Json& ProfileIndex = RequireMember(Entry, "profileIndex", "Surface Profile Map surface entry");
            const std::uint32_t Surface = ReadNonNegativeIndex(SurfaceId, "surfaceId");
            const std::uint32_t Profile = ReadNonNegativeIndex(ProfileIndex, "profileIndex");
            if (Profile >= Result.ProfilePaths.size())
            {
                throw std::runtime_error("Surface Profile Map profileIndex is outside the profiles array.");
            }
            if (std::ranges::find(SurfaceIDs, Surface) != SurfaceIDs.end())
            {
                throw std::runtime_error("Surface Profile Map contains a duplicate surfaceId.");
            }
            SurfaceIDs.push_back(Surface);
        }

        std::vector<bool> SeenSurfaces(Surfaces.size(), false);
        for (const SurfaceLocalID Surface : SurfaceIDs)
        {
            if (Surface < SeenSurfaces.size())
            {
                SeenSurfaces[Surface] = true;
            }
        }
        for (std::size_t Surface = 0; Surface < SeenSurfaces.size(); ++Surface)
        {
            if (!SeenSurfaces[Surface])
            {
                throw std::runtime_error("Surface Profile Map is missing surfaceId " + std::to_string(Surface) + ".");
            }
        }
        if (std::ranges::any_of(SurfaceIDs, [SurfaceCount = Surfaces.size()](SurfaceLocalID Surface)
                                { return Surface >= SurfaceCount; }))
        {
            throw std::runtime_error("Surface Profile Map surface IDs must be dense and start at zero.");
        }

        Result.ProfileIndicesBySurface.resize(Surfaces.size(), InvalidSurfaceProfileIndex);
        for (std::size_t EntryIndex = 0; EntryIndex < Surfaces.size(); ++EntryIndex)
        {
            const std::uint32_t Surface = ReadNonNegativeIndex(Surfaces[EntryIndex]["surfaceId"], "surfaceId");
            const std::uint32_t Profile = ReadNonNegativeIndex(Surfaces[EntryIndex]["profileIndex"], "profileIndex");
            Result.ProfileIndicesBySurface[Surface] = Profile;
        }
        return Result;
    }

    std::vector<SurfaceProfileIndex>
    SurfaceProfileDistribution::BuildTexelProfileMap(const SurfaceMappingData& Mapping) const
    {
        if (Mapping.Surfaces.size() != ProfileIndicesBySurface.size())
        {
            throw std::runtime_error("Surface Profile Map surface count does not match the Mesh mapping.");
        }
        std::vector<SurfaceProfileIndex> Result(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t TexelIndex = 0; TexelIndex < Mapping.Texels.size(); ++TexelIndex)
        {
            const SurfaceMappingTexel& Texel = Mapping.Texels[TexelIndex];
            if (!Texel.IsValid())
            {
                continue;
            }
            if (Texel.Surface >= ProfileIndicesBySurface.size())
            {
                throw std::runtime_error("Mesh mapping texel references a Surface missing from the Profile Map.");
            }
            Result[TexelIndex] = ProfileIndicesBySurface[Texel.Surface];
        }
        return Result;
    }
} // namespace MDSS
