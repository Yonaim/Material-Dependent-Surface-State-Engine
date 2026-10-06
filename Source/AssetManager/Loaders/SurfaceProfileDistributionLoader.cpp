/**
 * @file SurfaceProfileDistributionLoader.cpp
 * @brief Mesh Surface ID와 정렬된 SRProfile 에셋을 sidecar 파일에서 읽는다.
 */

#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace MDSS::Asset
{
    namespace
    {
        using TJson = nlohmann::json;

        const TJson& RequireMember(const TJson& Object, const char* Name, const std::string& Context)
        {
            const auto Found = Object.find(Name);
            if (Found == Object.end())
            {
                throw std::runtime_error(Context + " is missing required field '" + Name + "'.");
            }
            return *Found;
        }

        std::uint32_t ReadNonNegativeIndex(const TJson& Value, const std::string& Name)
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

        SurfaceState::TSurfaceProfileIndex ReadProfileIndex(const TJson& Value)
        {
            if (Value.is_number_unsigned())
            {
                return ReadNonNegativeIndex(Value, "profileIndex");
            }
            if (!Value.is_number_integer())
            {
                throw std::runtime_error("Surface Profile Map 'profileIndex' must be -1 or a non-negative integer.");
            }

            const std::int64_t Parsed = Value.get<std::int64_t>();
            if (Parsed == -1)
            {
                return SurfaceState::InvalidSurfaceProfileIndex;
            }
            if (Parsed < 0 || static_cast<std::uint64_t>(Parsed) >= SurfaceState::InvalidSurfaceProfileIndex)
            {
                throw std::runtime_error(
                    "Surface Profile Map 'profileIndex' must be -1 or a supported non-negative integer.");
            }
            return static_cast<SurfaceState::TSurfaceProfileIndex>(Parsed);
        }
    } // namespace

#pragma region Distribution_Loading

    TSurfaceProfileDistribution TSurfaceProfileDistributionLoader::Load(const std::filesystem::path& Path)
    {
        std::ifstream Input(Path);
        if (!Input)
        {
            throw std::runtime_error("Unable to open Surface Profile Map: " + Path.string());
        }

        TJson Root;
        try
        {
            Input >> Root;
        }
        catch (const TJson::exception& Exception)
        {
            throw std::runtime_error("Invalid Surface Profile Map JSON in '" + Path.string() +
                                     "': " + Exception.what());
        }
        if (!Root.is_object())
        {
            throw std::runtime_error("Surface Profile Map root must be a JSON object.");
        }
        const TJson& Type = RequireMember(Root, "type", "Surface Profile Map");
        const TJson& Version = RequireMember(Root, "version", "Surface Profile Map");
        if (!Type.is_string() || Type.get<std::string>() != "SurfaceProfileMap")
        {
            throw std::runtime_error("Surface Profile Map type must be 'SurfaceProfileMap'.");
        }
        if (ReadNonNegativeIndex(Version, "version") != 1)
        {
            throw std::runtime_error("Surface Profile Map version must be the integer 1.");
        }

        const TJson& Profiles = RequireMember(Root, "profiles", "Surface Profile Map");
        const TJson& Surfaces = RequireMember(Root, "surfaces", "Surface Profile Map");
        if (!Profiles.is_array() || Profiles.empty())
        {
            throw std::runtime_error("Surface Profile Map 'profiles' must be a non-empty array.");
        }
        if (!Surfaces.is_array() || Surfaces.empty())
        {
            throw std::runtime_error("Surface Profile Map 'surfaces' must be a non-empty array.");
        }
        if (Profiles.size() >= SurfaceState::InvalidSurfaceProfileIndex ||
            Surfaces.size() >= SurfaceState::InvalidSurfaceID)
        {
            throw std::runtime_error("Surface Profile Map exceeds the supported index range.");
        }

        TSurfaceProfileDistribution Result;
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

        std::vector<SurfaceState::TSurfaceLocalID> SurfaceIDs;
        SurfaceIDs.reserve(Surfaces.size());
        for (std::size_t EntryIndex = 0; EntryIndex < Surfaces.size(); ++EntryIndex)
        {
            const TJson& Entry = Surfaces[EntryIndex];
            if (!Entry.is_object())
            {
                throw std::runtime_error("Surface Profile Map surface entry must be an object.");
            }
            const TJson& SurfaceId = RequireMember(Entry, "surfaceId", "Surface Profile Map surface entry");
            const TJson& ProfileIndex = RequireMember(Entry, "profileIndex", "Surface Profile Map surface entry");
            const std::uint32_t                      Surface = ReadNonNegativeIndex(SurfaceId, "surfaceId");
            const SurfaceState::TSurfaceProfileIndex Profile = ReadProfileIndex(ProfileIndex);
            if (Profile != SurfaceState::InvalidSurfaceProfileIndex && Profile >= Result.ProfilePaths.size())
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
        for (const SurfaceState::TSurfaceLocalID Surface : SurfaceIDs)
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
        if (std::ranges::any_of(SurfaceIDs,
                                [SurfaceCount = Surfaces.size()](SurfaceState::TSurfaceLocalID Surface)
                                { return Surface >= SurfaceCount; }))
        {
            throw std::runtime_error("Surface Profile Map surface IDs must be dense and start at zero.");
        }

        Result.ProfileIndicesBySurface.resize(Surfaces.size(), SurfaceState::InvalidSurfaceProfileIndex);
        for (std::size_t EntryIndex = 0; EntryIndex < Surfaces.size(); ++EntryIndex)
        {
            const std::uint32_t Surface = ReadNonNegativeIndex(Surfaces[EntryIndex]["surfaceId"], "surfaceId");
            const SurfaceState::TSurfaceProfileIndex Profile = ReadProfileIndex(Surfaces[EntryIndex]["profileIndex"]);
            Result.ProfileIndicesBySurface[Surface] = Profile;
        }
        return Result;
    }

    std::vector<SurfaceState::TSurfaceProfileIndex>
#pragma endregion

#pragma region Texel_Profile_Mapping

    TSurfaceProfileDistribution::BuildTexelProfileMap(const SurfaceState::TSurfaceMappingData& Mapping) const
    {
        if (Mapping.Surfaces.size() != ProfileIndicesBySurface.size())
        {
            throw std::runtime_error("Surface Profile Map surface count does not match the Mesh mapping.");
        }
        std::vector<SurfaceState::TSurfaceProfileIndex> Result(Mapping.Texels.size(),
                                                               SurfaceState::InvalidSurfaceProfileIndex);
        for (std::size_t TexelIndex = 0; TexelIndex < Mapping.Texels.size(); ++TexelIndex)
        {
            const SurfaceState::TSurfaceMappingTexel& Texel = Mapping.Texels[TexelIndex];
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
#pragma endregion
} // namespace MDSS::Asset
