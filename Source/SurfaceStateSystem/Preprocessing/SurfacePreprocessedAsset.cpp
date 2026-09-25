/**
 * @file SurfacePreprocessedAsset.cpp
 * @brief Static Surface mapping/profile data and versioned binary cache.
 */

#include "SurfaceStateSystem/Preprocessing/SurfacePreprocessedAsset.h"

#include "SurfaceStateSystem/Mapping/SurfaceMappingValidation.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace MDSS
{
    namespace
    {
        constexpr std::array<char, 8> Magic = {'M', 'D', 'S', 'S', 'S', 'U', 'R', 'F'};
        constexpr std::uint64_t       FNVOffset = 14695981039346656037ULL;
        constexpr std::uint64_t       FNVPrime = 1099511628211ULL;
        constexpr std::uint32_t       MaxSerializedElements = 100'000'000U;

        void HashByte(std::uint64_t& Hash, std::uint8_t Byte)
        {
            Hash ^= Byte;
            Hash *= FNVPrime;
        }

        std::string FormatHash(std::uint64_t Hash)
        {
            std::ostringstream Output;
            Output << std::hex << std::setfill('0') << std::setw(16) << Hash;
            return Output.str();
        }

        void WriteU32(std::ostream& Output, std::uint32_t Value)
        {
            for (unsigned int Shift = 0; Shift < 32; Shift += 8)
            {
                Output.put(static_cast<char>((Value >> Shift) & 0xFFU));
            }
        }

        void WriteFloat(std::ostream& Output, float Value)
        {
            WriteU32(Output, std::bit_cast<std::uint32_t>(Value));
        }

        void WriteString(std::ostream& Output, const std::string& Value)
        {
            if (Value.size() > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("Surface cache metadata string is too large.");
            }
            WriteU32(Output, static_cast<std::uint32_t>(Value.size()));
            Output.write(Value.data(), static_cast<std::streamsize>(Value.size()));
        }

        std::uint32_t ReadU32(std::istream& Input)
        {
            std::uint32_t Value = 0;
            for (unsigned int Shift = 0; Shift < 32; Shift += 8)
            {
                const int Byte = Input.get();
                if (Byte == std::char_traits<char>::eof())
                {
                    throw std::runtime_error("Surface cache is truncated.");
                }
                Value |= static_cast<std::uint32_t>(static_cast<unsigned char>(Byte)) << Shift;
            }
            return Value;
        }

        float ReadFloat(std::istream& Input)
        {
            return std::bit_cast<float>(ReadU32(Input));
        }

        std::string ReadString(std::istream& Input)
        {
            const std::uint32_t Length = ReadU32(Input);
            if (Length > 1'048'576U)
            {
                throw std::runtime_error("Surface cache metadata string exceeds the supported size.");
            }
            std::string Value(Length, '\0');
            Input.read(Value.data(), static_cast<std::streamsize>(Length));
            if (!Input)
            {
                throw std::runtime_error("Surface cache is truncated while reading metadata.");
            }
            return Value;
        }

        void WriteVec3(std::ostream& Output, const glm::vec3& Value)
        {
            WriteFloat(Output, Value.x);
            WriteFloat(Output, Value.y);
            WriteFloat(Output, Value.z);
        }

        glm::vec3 ReadVec3(std::istream& Input)
        {
            return {ReadFloat(Input), ReadFloat(Input), ReadFloat(Input)};
        }

        void ValidateAsset(const SurfacePreprocessedAsset& Asset)
        {
            if (Asset.Metadata.PreprocessVersion == 0)
            {
                throw std::invalid_argument("Surface cache preprocess version must be greater than zero.");
            }
            if (Asset.Metadata.MeshHash.empty())
            {
                throw std::invalid_argument("Surface cache requires a Mesh content hash.");
            }
            if (Asset.Geometry.GetSurfaces().empty())
            {
                throw std::invalid_argument("Surface cache requires at least one Surface.");
            }
            if (Asset.Metadata.GridResolutions.size() != Asset.Geometry.GetSurfaces().size())
            {
                throw std::invalid_argument("Surface cache resolution metadata does not match its Surface count.");
            }
            for (std::size_t Index = 0; Index < Asset.Geometry.GetSurfaces().size(); ++Index)
            {
                if (Asset.Metadata.GridResolutions[Index] != Asset.Geometry.GetSurfaces()[Index].Resolution)
                {
                    throw std::invalid_argument("Surface cache resolution metadata does not match geometry.");
                }
            }
            if (Asset.Geometry.GetProfileMap().size() != Asset.Geometry.GetTexelCount())
            {
                throw std::invalid_argument("Surface cache ProfileMap size must match its texel count.");
            }
            if (Asset.Metadata.ProfileMapHash != SurfacePreprocessor::HashProfileMap(Asset.Geometry.GetProfileMap()))
            {
                throw std::invalid_argument("Surface cache ProfileMapHash does not match its texel Profile indices.");
            }

            const auto& Texels = Asset.Geometry.GetTexels();
            for (std::size_t Index = 0; Index < Texels.size(); ++Index)
            {
                const SurfaceTexelGeometry& Texel = Texels[Index];
                if (Texel.IsValid() != (Asset.Geometry.GetProfileMap()[Index] != InvalidSurfaceProfileIndex))
                {
                    throw std::invalid_argument("Surface cache valid texels must have a Profile index and invalid "
                                                "texels must use the invalid Profile sentinel.");
                }
                if (!Texel.IsValid())
                {
                    if (Texel.Surface != InvalidSurfaceID || Texel.Triangle != InvalidTriangleID)
                    {
                        throw std::invalid_argument("Invalid Surface cache texels must use both topology sentinels.");
                    }
                    for (const LocalTexelIndex Neighbor : Texel.NeighborIndices)
                    {
                        if (Neighbor != InvalidTexelIndex)
                        {
                            throw std::invalid_argument("Invalid texel in Surface cache cannot have neighbors.");
                        }
                    }
                    continue;
                }
                if (Texel.Surface >= Asset.Geometry.GetSurfaces().size())
                {
                    throw std::invalid_argument("Surface cache texel references an unknown Surface ID.");
                }
                if (Asset.Geometry.GetProfileMap()[Index] >= Asset.Metadata.ProfileCount)
                {
                    throw std::invalid_argument("Surface cache Profile index is outside the loaded Profile range.");
                }
                if (!std::isfinite(Texel.Position.x) || !std::isfinite(Texel.Position.y) ||
                    !std::isfinite(Texel.Position.z) || !std::isfinite(Texel.Normal.x) ||
                    !std::isfinite(Texel.Normal.y) || !std::isfinite(Texel.Normal.z) ||
                    !std::isfinite(Texel.Geometry.MesoVirtualHeight) || !std::isfinite(Texel.Geometry.ConcavityWeight))
                {
                    throw std::invalid_argument("Surface cache contains non-finite geometry data.");
                }
                for (const LocalTexelIndex Neighbor : Texel.NeighborIndices)
                {
                    if (Neighbor == InvalidTexelIndex)
                    {
                        continue;
                    }
                    if (Neighbor >= Texels.size() || !Texels[Neighbor].IsValid())
                    {
                        throw std::invalid_argument("Surface cache contains an invalid neighbor index.");
                    }
                    if (Neighbor == Index)
                    {
                        throw std::invalid_argument("Surface cache texel cannot be its own neighbor.");
                    }
                    if (std::ranges::find(Texels[Neighbor].NeighborIndices, static_cast<LocalTexelIndex>(Index)) ==
                        Texels[Neighbor].NeighborIndices.end())
                    {
                        throw std::invalid_argument("Surface cache neighbor relation is not bidirectional.");
                    }
                }
                for (const float Distance : Texel.NeighborDistances)
                {
                    if (!std::isfinite(Distance) || Distance < 0.0F)
                    {
                        throw std::invalid_argument("Surface cache contains an invalid neighbor distance.");
                    }
                }
            }
        }

        void WriteMetadata(std::ostream& Output, const SurfaceCacheMetadata& Metadata)
        {
            WriteString(Output, Metadata.MeshHash);
            WriteString(Output, Metadata.NormalMapHash);
            WriteString(Output, Metadata.ProfileMapHash);
            WriteU32(Output, Metadata.UVSet);
            WriteU32(Output, Metadata.PreprocessVersion);
            WriteU32(Output, Metadata.ProfileCount);
            if (Metadata.GridResolutions.size() > std::numeric_limits<std::uint32_t>::max())
            {
                throw std::runtime_error("Surface cache contains too many grid resolutions.");
            }
            WriteU32(Output, static_cast<std::uint32_t>(Metadata.GridResolutions.size()));
            for (const SurfaceResolution Resolution : Metadata.GridResolutions)
            {
                WriteU32(Output, Resolution.Width);
                WriteU32(Output, Resolution.Height);
            }
        }

        SurfaceCacheMetadata ReadMetadata(std::istream& Input)
        {
            SurfaceCacheMetadata Metadata;
            Metadata.MeshHash = ReadString(Input);
            Metadata.NormalMapHash = ReadString(Input);
            Metadata.ProfileMapHash = ReadString(Input);
            Metadata.UVSet = ReadU32(Input);
            Metadata.PreprocessVersion = ReadU32(Input);
            Metadata.ProfileCount = ReadU32(Input);
            const std::uint32_t ResolutionCount = ReadU32(Input);
            if (ResolutionCount > MaxSerializedElements)
            {
                throw std::runtime_error("Surface cache resolution metadata count is invalid.");
            }
            Metadata.GridResolutions.reserve(ResolutionCount);
            for (std::uint32_t Index = 0; Index < ResolutionCount; ++Index)
            {
                Metadata.GridResolutions.push_back({ReadU32(Input), ReadU32(Input)});
            }
            return Metadata;
        }
    } // namespace

    SurfacePreprocessedAsset::SurfacePreprocessedAsset(SurfaceCacheMetadata      Metadata,
                                                       SharedSurfaceGeometryData Geometry)
        : Metadata(std::move(Metadata)), Geometry(std::move(Geometry))
    {
        ValidateAsset(*this);
    }

    SurfaceCacheMetadata SurfacePreprocessor::CreateMetadata(const std::filesystem::path&            MeshPath,
                                                             const std::filesystem::path&            NormalMapPath,
                                                             const std::vector<SurfaceProfileIndex>& ProfileMap,
                                                             const std::vector<SurfaceResolution>&   GridResolutions,
                                                             std::uint32_t                           UVSet,
                                                             std::uint32_t                           PreprocessVersion,
                                                             std::uint32_t                           ProfileCount)
    {
        if (PreprocessVersion == 0)
        {
            throw std::invalid_argument("PreprocessVersion must be greater than zero.");
        }
        if (MeshPath.empty())
        {
            throw std::invalid_argument("MeshPath is required to fingerprint a Surface cache.");
        }
        if (ProfileMap.size() > InvalidTexelIndex)
        {
            throw std::overflow_error("Profile map exceeds the supported texel index range.");
        }
        for (const SurfaceProfileIndex Index : ProfileMap)
        {
            if (Index != InvalidSurfaceProfileIndex && Index >= ProfileCount)
            {
                throw std::invalid_argument("Profile map index is outside the loaded Profile range.");
            }
        }

        SurfaceCacheMetadata Metadata;
        Metadata.MeshHash = HashFile(MeshPath);
        Metadata.NormalMapHash = HashFile(NormalMapPath);
        Metadata.ProfileMapHash = HashProfileMap(ProfileMap);
        Metadata.UVSet = UVSet;
        Metadata.PreprocessVersion = PreprocessVersion;
        Metadata.ProfileCount = ProfileCount;
        Metadata.GridResolutions = GridResolutions;
        return Metadata;
    }

    SurfacePreprocessedAsset SurfacePreprocessor::Build(const SurfaceMappingData&        Mapping,
                                                        std::vector<SurfaceProfileIndex> ProfileMap,
                                                        std::uint32_t                    ProfileCount,
                                                        SurfaceCacheMetadata             Metadata)
    {
        ValidateSurfaceMapping(Mapping);
        if (ProfileMap.size() != Mapping.Texels.size())
        {
            throw std::invalid_argument("ProfileMap must provide exactly one entry per mapping texel.");
        }
        Metadata.ProfileCount = ProfileCount;
        if (Metadata.PreprocessVersion == 0)
        {
            throw std::invalid_argument("PreprocessVersion must be greater than zero.");
        }
        const std::string ComputedProfileHash = HashProfileMap(ProfileMap);
        if (!Metadata.ProfileMapHash.empty() && Metadata.ProfileMapHash != ComputedProfileHash)
        {
            throw std::invalid_argument("ProfileMapHash does not match the supplied texel Profile indices.");
        }
        Metadata.ProfileMapHash = ComputedProfileHash;

        std::vector<SurfaceDefinition> Definitions;
        Definitions.reserve(Mapping.Surfaces.size());
        std::vector<SurfaceResolution> Resolutions;
        Resolutions.reserve(Mapping.Surfaces.size());
        for (const SurfaceTexelRange& Surface : Mapping.Surfaces)
        {
            Definitions.push_back({Surface.Surface, Surface.Resolution});
            Resolutions.push_back(Surface.Resolution);
        }
        if (!Metadata.GridResolutions.empty() && Metadata.GridResolutions != Resolutions)
        {
            throw std::invalid_argument("Grid resolution metadata does not match the supplied mapping.");
        }
        Metadata.GridResolutions = std::move(Resolutions);

        SharedSurfaceGeometryData Geometry(std::move(Definitions));
        auto&                     GeometryTexels = Geometry.GetTexels();
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const SurfaceMappingTexel& Source = Mapping.Texels[Index];
            SurfaceTexelGeometry&      Target = GeometryTexels[Index];
            Target.Surface = Source.Surface;
            Target.Triangle = Source.Triangle;
            Target.Barycentric = Source.Barycentric;
            Target.Position = Source.Position;
            Target.Normal = Source.Normal;
            Target.NeighborIndices = Source.Neighbors;
            for (std::size_t NeighborIndex = 0; NeighborIndex < Source.Neighbors.size(); ++NeighborIndex)
            {
                const LocalTexelIndex Neighbor = Source.Neighbors[NeighborIndex];
                if (Neighbor != InvalidTexelIndex)
                {
                    Target.NeighborDistances[NeighborIndex] =
                        glm::length(Source.Position - Mapping.Texels.at(Neighbor).Position);
                }
            }
        }
        Geometry.SetProfileMap(std::move(ProfileMap));
        return SurfacePreprocessedAsset(std::move(Metadata), std::move(Geometry));
    }

    std::string SurfacePreprocessor::HashFile(const std::filesystem::path& Path)
    {
        if (Path.empty())
        {
            return {};
        }
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            throw std::runtime_error("Unable to open Surface cache input for hashing: " + Path.string());
        }

        std::uint64_t Hash = FNVOffset;
        char          Buffer[8192];
        while (Input)
        {
            Input.read(Buffer, sizeof(Buffer));
            const std::streamsize ReadCount = Input.gcount();
            for (std::streamsize Index = 0; Index < ReadCount; ++Index)
            {
                HashByte(Hash, static_cast<std::uint8_t>(static_cast<unsigned char>(Buffer[Index])));
            }
        }
        if (!Input.eof())
        {
            throw std::runtime_error("Failed while hashing Surface cache input: " + Path.string());
        }
        return FormatHash(Hash);
    }

    std::string SurfacePreprocessor::HashProfileMap(const std::vector<SurfaceProfileIndex>& ProfileMap)
    {
        std::uint64_t Hash = FNVOffset;
        for (const SurfaceProfileIndex Value : ProfileMap)
        {
            for (unsigned int Shift = 0; Shift < 32; Shift += 8)
            {
                HashByte(Hash, static_cast<std::uint8_t>((Value >> Shift) & 0xFFU));
            }
        }
        return FormatHash(Hash);
    }

    void SurfaceCache::Save(const std::filesystem::path& Path, const SurfacePreprocessedAsset& Asset)
    {
        ValidateAsset(Asset);
        std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
        if (!Output)
        {
            throw std::runtime_error("Unable to create Surface cache: " + Path.string());
        }

        Output.write(Magic.data(), static_cast<std::streamsize>(Magic.size()));
        WriteU32(Output, FormatVersion);
        WriteMetadata(Output, Asset.Metadata);

        const auto& Surfaces = Asset.Geometry.GetSurfaces();
        WriteU32(Output, static_cast<std::uint32_t>(Surfaces.size()));
        for (const SurfaceTexelRange& Surface : Surfaces)
        {
            WriteU32(Output, Surface.Surface);
            WriteU32(Output, Surface.Resolution.Width);
            WriteU32(Output, Surface.Resolution.Height);
        }

        const auto& Texels = Asset.Geometry.GetTexels();
        WriteU32(Output, static_cast<std::uint32_t>(Texels.size()));
        for (std::size_t Index = 0; Index < Texels.size(); ++Index)
        {
            const SurfaceTexelGeometry& Texel = Texels[Index];
            WriteU32(Output, Texel.Surface);
            WriteU32(Output, Texel.Triangle);
            WriteVec3(Output, Texel.Barycentric);
            WriteVec3(Output, Texel.Position);
            WriteVec3(Output, Texel.Normal);
            WriteFloat(Output, Texel.Geometry.MesoVirtualHeight);
            WriteFloat(Output, Texel.Geometry.ConcavityWeight);
            for (const LocalTexelIndex Neighbor : Texel.NeighborIndices)
            {
                WriteU32(Output, Neighbor);
            }
            for (const float Distance : Texel.NeighborDistances)
            {
                WriteFloat(Output, Distance);
            }
            WriteU32(Output, Asset.Geometry.GetProfileMap()[Index]);
        }

        if (!Output)
        {
            throw std::runtime_error("Failed while writing Surface cache: " + Path.string());
        }
    }

    SurfacePreprocessedAsset SurfaceCache::Load(const std::filesystem::path& Path,
                                                const SurfaceCacheMetadata&  ExpectedMetadata)
    {
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            throw std::runtime_error("Unable to open Surface cache: " + Path.string());
        }

        std::array<char, Magic.size()> FileMagic{};
        Input.read(FileMagic.data(), static_cast<std::streamsize>(FileMagic.size()));
        if (!Input || FileMagic != Magic)
        {
            throw std::runtime_error("Surface cache magic is invalid or the file is truncated.");
        }
        if (ReadU32(Input) != FormatVersion)
        {
            throw std::runtime_error("Surface cache format version is unsupported.");
        }

        SurfaceCacheMetadata Metadata = ReadMetadata(Input);
        if (Metadata != ExpectedMetadata)
        {
            throw std::runtime_error("Surface cache is stale: source metadata does not match.");
        }

        const std::uint32_t SurfaceCount = ReadU32(Input);
        if (SurfaceCount == 0 || SurfaceCount > MaxSerializedElements)
        {
            throw std::runtime_error("Surface cache Surface count is invalid.");
        }
        std::vector<SurfaceDefinition> Definitions;
        Definitions.reserve(SurfaceCount);
        for (std::uint32_t Index = 0; Index < SurfaceCount; ++Index)
        {
            Definitions.push_back({ReadU32(Input), {ReadU32(Input), ReadU32(Input)}});
        }

        SharedSurfaceGeometryData Geometry(std::move(Definitions));
        const std::uint32_t       TexelCount = ReadU32(Input);
        if (TexelCount != Geometry.GetTexelCount() || TexelCount > MaxSerializedElements)
        {
            throw std::runtime_error("Surface cache texel count does not match its Surface resolutions.");
        }

        auto&                            Texels = Geometry.GetTexels();
        std::vector<SurfaceProfileIndex> ProfileMap(TexelCount, InvalidSurfaceProfileIndex);
        for (std::uint32_t Index = 0; Index < TexelCount; ++Index)
        {
            SurfaceTexelGeometry& Texel = Texels[Index];
            Texel.Surface = ReadU32(Input);
            Texel.Triangle = ReadU32(Input);
            Texel.Barycentric = ReadVec3(Input);
            Texel.Position = ReadVec3(Input);
            Texel.Normal = ReadVec3(Input);
            Texel.Geometry.MesoVirtualHeight = ReadFloat(Input);
            Texel.Geometry.ConcavityWeight = ReadFloat(Input);
            for (LocalTexelIndex& Neighbor : Texel.NeighborIndices)
            {
                Neighbor = ReadU32(Input);
            }
            for (float& Distance : Texel.NeighborDistances)
            {
                Distance = ReadFloat(Input);
            }
            ProfileMap[Index] = ReadU32(Input);
        }
        if (Metadata.ProfileMapHash != SurfacePreprocessor::HashProfileMap(ProfileMap))
        {
            throw std::runtime_error("Surface cache Profile map hash does not match serialized entries.");
        }
        Geometry.SetProfileMap(std::move(ProfileMap));
        if (Input.peek() != std::char_traits<char>::eof())
        {
            throw std::runtime_error("Surface cache contains unexpected trailing data.");
        }

        return SurfacePreprocessedAsset(std::move(Metadata), std::move(Geometry));
    }
} // namespace MDSS
