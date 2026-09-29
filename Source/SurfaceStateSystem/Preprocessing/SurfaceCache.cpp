/**
 * @file SurfaceCache.cpp
 * @brief Portable .Surface serialization, input fingerprinting and corruption checks.
 */

#include "SurfaceStateSystem/Preprocessing/SurfaceCache.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace MDSS
{
    namespace
    {
        constexpr std::array<std::uint8_t, 8> Magic{'M', 'D', 'S', 'S', 'S', 'R', 'F', '3'};
        constexpr std::size_t HeaderBytes = 52;
        constexpr std::size_t TexelBytes = 140;
        constexpr std::uint64_t HashBasis = 14695981039346656037ULL;
        constexpr std::uint64_t HashPrime = 1099511628211ULL;
        static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);

        class THash
        {
        public:
            void Bytes(std::span<const std::uint8_t> Data)
            {
                for (const std::uint8_t Byte : Data) Value = (Value ^ Byte) * HashPrime;
            }
            void Integer(std::uint64_t Number)
            {
                for (int Shift = 0; Shift < 64; Shift += 8)
                {
                    const std::uint8_t Byte = static_cast<std::uint8_t>(Number >> Shift);
                    Bytes({&Byte, 1});
                }
            }
            void Float(float Number) { Integer(std::bit_cast<std::uint32_t>(Number)); }
            void String(const std::string& Text)
            {
                Integer(Text.size());
                Bytes({reinterpret_cast<const std::uint8_t*>(Text.data()), Text.size()});
            }
            std::uint64_t Value = HashBasis;
        };

        std::string CanonicalPath(const std::filesystem::path& Path)
        {
            return std::filesystem::absolute(Path).lexically_normal().generic_string();
        }

        std::uint64_t HashFile(const std::filesystem::path& Path)
        {
            std::ifstream Input(Path, std::ios::binary);
            if (!Input) throw std::runtime_error("Unable to fingerprint Surface input: " + Path.string());
            THash Hash;
            std::array<std::uint8_t, 65536> Buffer{};
            while (Input)
            {
                Input.read(reinterpret_cast<char*>(Buffer.data()), Buffer.size());
                Hash.Bytes({Buffer.data(), static_cast<std::size_t>(Input.gcount())});
            }
            if (!Input.eof()) throw std::runtime_error("Unable to read Surface input: " + Path.string());
            return Hash.Value;
        }

        void Write32(std::vector<std::uint8_t>& Data, std::uint32_t Number)
        {
            for (int Shift = 0; Shift < 32; Shift += 8) Data.push_back(static_cast<std::uint8_t>(Number >> Shift));
        }
        void Write64(std::vector<std::uint8_t>& Data, std::uint64_t Number)
        {
            Write32(Data, static_cast<std::uint32_t>(Number));
            Write32(Data, static_cast<std::uint32_t>(Number >> 32));
        }
        void WriteFloat(std::vector<std::uint8_t>& Data, float Number) { Write32(Data, std::bit_cast<std::uint32_t>(Number)); }
        void WriteVec3(std::vector<std::uint8_t>& Data, const glm::vec3& V)
        {
            WriteFloat(Data, V.x); WriteFloat(Data, V.y); WriteFloat(Data, V.z);
        }
        void WriteString(std::vector<std::uint8_t>& Data, const std::string& Text)
        {
            if (Text.size() > UINT32_MAX) throw std::runtime_error("Surface cache path is too long.");
            Write32(Data, static_cast<std::uint32_t>(Text.size()));
            Data.insert(Data.end(), Text.begin(), Text.end());
        }

        class TReader
        {
        public:
            explicit TReader(std::span<const std::uint8_t> Bytes) : Data(Bytes) {}
            std::uint32_t Read32()
            {
                Require(4);
                // memcpy accepts unaligned records and avoids a per-byte decode loop.
                std::uint32_t Result;
                std::memcpy(&Result, Data.data() + Offset, sizeof(Result));
                Offset += sizeof(Result);
                if constexpr (std::endian::native == std::endian::big)
                    Result = (Result >> 24) | ((Result >> 8) & 0x0000ff00U) |
                             ((Result << 8) & 0x00ff0000U) | (Result << 24);
                return Result;
            }
            std::uint64_t Read64()
            {
                const std::uint64_t Low = Read32();
                return Low | (std::uint64_t(Read32()) << 32);
            }
            float ReadFloat() { return std::bit_cast<float>(Read32()); }
            glm::vec3 ReadVec3()
            {
                const float X = ReadFloat(); const float Y = ReadFloat(); const float Z = ReadFloat();
                return {X, Y, Z};
            }
            std::string ReadString()
            {
                const std::uint32_t Length = Read32();
                Require(Length);
                const std::string Result(reinterpret_cast<const char*>(Data.data() + Offset), Length);
                Offset += Length;
                return Result;
            }
            std::size_t Offset = 0;
        private:
            void Require(std::size_t Count) const
            {
                if (Count > Data.size() - Offset) throw std::runtime_error("Truncated Surface cache.");
            }
            std::span<const std::uint8_t> Data;
        };

        std::uint64_t TexelCount(const TSurfaceCacheDescriptor& Descriptor)
        {
            if (Descriptor.Surfaces.empty() || Descriptor.ProfilePaths.empty() || Descriptor.TriangleCount == 0 ||
                Descriptor.Surfaces.size() >= InvalidSurfaceID || Descriptor.ProfilePaths.size() >= InvalidSurfaceProfileIndex)
                throw std::runtime_error("Invalid Surface cache descriptor.");
            std::uint64_t Count = 0;
            for (std::size_t Index = 0; Index < Descriptor.Surfaces.size(); ++Index)
            {
                const TSurfaceDefinition& Surface = Descriptor.Surfaces[Index];
                if (Surface.ID != Index) throw std::runtime_error("Surface cache IDs must be dense and ordered.");
                Count += Surface.Resolution.GetTexelCount();
                if (Count > UINT32_MAX) throw std::runtime_error("Surface cache texel count exceeds index range.");
            }
            return Count;
        }

        std::size_t EncodedSize(const TSurfaceCacheDescriptor& Descriptor)
        {
            std::uint64_t Size = HeaderBytes + Descriptor.Surfaces.size() * 12ULL + TexelCount(Descriptor) * TexelBytes;
            for (const auto& Path : Descriptor.ProfilePaths) Size += 4 + CanonicalPath(Path).size();
            if (Size > std::numeric_limits<std::size_t>::max() ||
                Size > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max()))
                throw std::runtime_error("Surface cache size exceeds file/address range.");
            return static_cast<std::size_t>(Size);
        }

        bool Finite(const glm::vec3& V) { return std::isfinite(V.x) && std::isfinite(V.y) && std::isfinite(V.z); }
        bool Unit(const glm::vec3& V) { return Finite(V) && std::abs(glm::length(V) - 1.0F) <= 1.0e-3F; }

        void Validate(const TSharedSurfaceGeometryData& Geometry, const TSurfaceCacheDescriptor& Descriptor)
        {
            if (Geometry.GetTexelCount() != TexelCount(Descriptor) ||
                Geometry.GetSurfaces().size() != Descriptor.Surfaces.size() ||
                Geometry.GetProfileMap().size() != Geometry.GetTexelCount())
                throw std::runtime_error("Surface cache geometry size mismatch.");
            for (std::size_t Index = 0; Index < Descriptor.Surfaces.size(); ++Index)
            {
                const auto& Range = Geometry.GetSurfaces()[Index];
                if (Range.Surface != Descriptor.Surfaces[Index].ID || Range.Resolution != Descriptor.Surfaces[Index].Resolution)
                    throw std::runtime_error("Surface cache resolution mismatch.");
                for (std::size_t I = Range.FirstTexel; I < std::size_t(Range.FirstTexel) + Range.TexelCount; ++I)
                {
                    const auto& T = Geometry.GetTexels()[I];
                    const auto Profile = Geometry.GetProfileMap()[I];
                    if (!Finite(T.Position) || !Finite(T.AreaVector) || !Finite(T.Barycentric) || !Finite(T.Normal) ||
                        !Finite(T.TransferNormal) || !Finite(T.MesoNormal) ||
                        !std::isfinite(T.Geometry.MesoVirtualHeight) || !std::isfinite(T.Geometry.ConcavityWeight) ||
                        !std::isfinite(T.Geometry.MesoMeanCurvature) || !std::isfinite(T.Geometry.MesoGaussianCurvature) ||
                        T.Geometry.ConcavityWeight < 0.0F || T.Geometry.ConcavityWeight > 1.0F)
                        throw std::runtime_error("Surface cache contains invalid geometry values.");
                    if (T.IsValid())
                    {
                        if (T.Surface != Range.Surface || T.Triangle >= Descriptor.TriangleCount || T.Chart == UINT32_MAX ||
                            !Unit(T.Normal) || (T.HasTransferNormal && !Unit(T.TransferNormal)) ||
                            (T.HasMesoNormal && !Unit(T.MesoNormal)) ||
                            std::abs(T.Barycentric.x + T.Barycentric.y + T.Barycentric.z - 1.0F) > 1.0e-3F ||
                            T.Barycentric.x < -1.0e-3F || T.Barycentric.y < -1.0e-3F || T.Barycentric.z < -1.0e-3F ||
                            (Profile != InvalidSurfaceProfileIndex && Profile >= Descriptor.ProfilePaths.size()))
                            throw std::runtime_error("Surface cache contains invalid texel mapping.");
                    }
                    else if (T.Surface != InvalidSurfaceID || T.Triangle != InvalidTriangleID ||
                             Profile != InvalidSurfaceProfileIndex || T.HasTransferNormal || T.HasMesoNormal)
                        throw std::runtime_error("Surface cache contains inconsistent invalid texels.");
                    std::array<TLocalTexelIndex, SurfaceNeighborCount> Seen{};
                    std::size_t SeenCount = 0;
                    for (const auto Neighbor : T.NeighborIndices)
                    {
                        if (Neighbor != InvalidTexelIndex &&
                            (!T.IsValid() || Neighbor >= Geometry.GetTexelCount() || Neighbor == I ||
                             !Geometry.GetTexels()[Neighbor].IsValid()))
                            throw std::runtime_error("Surface cache contains invalid neighbor indices.");
                        if (Neighbor != InvalidTexelIndex)
                        {
                            const auto& Reverse = Geometry.GetTexels()[Neighbor].NeighborIndices;
                            if (std::find(Seen.begin(), Seen.begin() + SeenCount, Neighbor) != Seen.begin() + SeenCount ||
                                std::find(Reverse.begin(), Reverse.end(), static_cast<TLocalTexelIndex>(I)) == Reverse.end())
                                throw std::runtime_error("Surface cache neighbor links must be unique and bidirectional.");
                            Seen[SeenCount++] = Neighbor;
                        }
                    }
                }
            }
        }
    } // namespace

    TSurfaceCacheDescriptor TSurfaceCache::Describe(
        std::span<const TVertex> Vertices, std::span<const TMeshTriangleSource> Triangles,
        std::vector<TSurfaceDefinition> Surfaces, std::span<const std::filesystem::path> NormalMapPaths,
        const std::filesystem::path& DistributionPath, std::vector<std::filesystem::path> ProfilePaths,
        std::span<const TSurfaceProfileIndex> ProfileIndicesBySurface)
    {
        if (Triangles.size() > UINT32_MAX || NormalMapPaths.size() != Surfaces.size() ||
            ProfileIndicesBySurface.size() != Surfaces.size())
            throw std::runtime_error("Surface cache input dimensions are inconsistent.");
        TSurfaceCacheDescriptor Result{0, std::move(Surfaces), std::move(ProfilePaths), static_cast<std::uint32_t>(Triangles.size())};
        (void)TexelCount(Result);
        THash Hash;
        Hash.Integer(PreprocessVersion);
        Hash.Integer(Vertices.size());
        for (const auto& V : Vertices)
        {
            for (int I = 0; I < 3; ++I) Hash.Float(V.Position[I]);
            for (int I = 0; I < 3; ++I) Hash.Float(V.Normal[I]);
            for (int I = 0; I < 2; ++I) Hash.Float(V.UV[I]);
            for (int I = 0; I < 4; ++I) Hash.Float(V.Tangent[I]);
        }
        Hash.Integer(Triangles.size());
        for (const auto& T : Triangles)
        {
            for (const auto Index : T.RenderVertexIndices) Hash.Integer(Index);
            for (const auto Index : T.OriginalPositionIndices) Hash.Integer(static_cast<std::uint32_t>(Index));
            for (const auto Index : T.OriginalUVIndices) Hash.Integer(static_cast<std::uint32_t>(Index));
            Hash.Integer(T.Surface);
        }
        Hash.Integer(Result.Surfaces.size());
        std::unordered_map<std::string, std::uint64_t> NormalHashes;
        for (std::size_t I = 0; I < Result.Surfaces.size(); ++I)
        {
            const auto& S = Result.Surfaces[I];
            Hash.Integer(S.ID); Hash.Integer(S.Resolution.Width); Hash.Integer(S.Resolution.Height);
            if (NormalMapPaths[I].empty()) Hash.String("");
            else
            {
                const std::string Key = CanonicalPath(NormalMapPaths[I]);
                auto It = NormalHashes.find(Key);
                if (It == NormalHashes.end()) It = NormalHashes.emplace(Key, HashFile(NormalMapPaths[I])).first;
                Hash.String(Key); Hash.Integer(It->second);
            }
            Hash.Integer(ProfileIndicesBySurface[I]);
        }
        Hash.String(CanonicalPath(DistributionPath)); Hash.Integer(HashFile(DistributionPath));
        Hash.Integer(Result.ProfilePaths.size());
        for (const auto& Path : Result.ProfilePaths) Hash.String(CanonicalPath(Path));
        Result.Fingerprint = Hash.Value;
        return Result;
    }

    std::filesystem::path TSurfaceCache::GetPath(const std::filesystem::path& CacheRoot,
        const std::filesystem::path& MeshPath, const std::filesystem::path& DistributionPath, std::uint32_t Resolution)
    {
        THash Identity;
        Identity.String(CanonicalPath(MeshPath)); Identity.String(CanonicalPath(DistributionPath));
        std::ostringstream Suffix;
        Suffix << std::hex << std::setw(16) << std::setfill('0') << Identity.Value;
        const std::string Stem = MeshPath.stem().string();
        return CacheRoot / (Stem + "_" + Suffix.str()) / (Stem + "_" + std::to_string(Resolution) + ".Surface");
    }

    std::optional<TSharedSurfaceGeometryData> TSurfaceCache::Load(const std::filesystem::path& Path,
        const TSurfaceCacheDescriptor& Expected, std::string& Diagnostic)
    {
        try
        {
            std::ifstream Input(Path, std::ios::binary | std::ios::ate);
            if (!Input)
            {
                Diagnostic = std::filesystem::exists(Path) ? "unreadable cache" : "missing cache";
                return std::nullopt;
            }
            const std::size_t Size = EncodedSize(Expected);
            if (Input.tellg() != static_cast<std::streamoff>(Size)) throw std::runtime_error("stale/corrupt cache size");
            std::vector<std::uint8_t> Data(Size);
            Input.seekg(0); Input.read(reinterpret_cast<char*>(Data.data()), static_cast<std::streamsize>(Data.size()));
            if (!Input) throw std::runtime_error("truncated cache read");
            if (!std::equal(Magic.begin(), Magic.end(), Data.begin())) throw std::runtime_error("unsupported cache magic");
            TReader Reader(Data); Reader.Offset = Magic.size();
            if (Reader.Read32() != FormatVersion || Reader.Read32() != PreprocessVersion)
                throw std::runtime_error("stale cache version");
            if (Reader.Read64() != Expected.Fingerprint) throw std::runtime_error("stale input fingerprint");
            if (Reader.Read32() != Expected.Surfaces.size() || Reader.Read32() != Expected.ProfilePaths.size() ||
                Reader.Read32() != Expected.TriangleCount || Reader.Read64() != TexelCount(Expected))
                throw std::runtime_error("stale cache dimensions");
            const auto StoredHash = Reader.Read64();
            THash PayloadHash; PayloadHash.Bytes(std::span(Data).subspan(HeaderBytes));
            if (StoredHash != PayloadHash.Value) throw std::runtime_error("corrupt cache checksum");
            for (const auto& Surface : Expected.Surfaces)
            {
                if (Reader.Read32() != Surface.ID || Reader.Read32() != Surface.Resolution.Width ||
                    Reader.Read32() != Surface.Resolution.Height) throw std::runtime_error("stale Surface definitions");
            }
            for (const auto& ProfilePath : Expected.ProfilePaths)
            {
                if (Reader.ReadString() != CanonicalPath(ProfilePath)) throw std::runtime_error("stale Profile table order");
            }
            TSharedSurfaceGeometryData Geometry(Expected.Surfaces);
            std::vector<TSurfaceProfileIndex> Profiles(Geometry.GetTexelCount());
            for (std::size_t I = 0; I < Geometry.GetTexelCount(); ++I)
            {
                auto& T = Geometry.GetTexels()[I];
                T.Surface = Reader.Read32(); T.Triangle = Reader.Read32(); T.Chart = Reader.Read32();
                T.Barycentric = Reader.ReadVec3(); T.Position = Reader.ReadVec3(); T.Normal = Reader.ReadVec3();
                T.TransferNormal = Reader.ReadVec3(); T.MesoNormal = Reader.ReadVec3();
                const auto Flags = Reader.Read32();
                if (Flags > 3) throw std::runtime_error("invalid cache normal flags");
                T.HasTransferNormal = (Flags & 1) != 0; T.HasMesoNormal = (Flags & 2) != 0;
                T.Geometry.MesoVirtualHeight = Reader.ReadFloat(); T.Geometry.ConcavityWeight = Reader.ReadFloat();
                T.Geometry.MesoMeanCurvature = Reader.ReadFloat(); T.Geometry.MesoGaussianCurvature = Reader.ReadFloat();
                for (auto& Neighbor : T.NeighborIndices) Neighbor = Reader.Read32();
                Profiles[I] = Reader.Read32();
                T.AreaVector = Reader.ReadVec3();
            }
            Geometry.SetProfileMap(std::move(Profiles));
            if (Reader.Offset != Data.size()) throw std::runtime_error("trailing cache payload");
            Validate(Geometry, Expected);
            Diagnostic.clear();
            return std::optional<TSharedSurfaceGeometryData>(std::move(Geometry));
        }
        catch (const std::exception& Error)
        {
            Diagnostic = Error.what();
            return std::nullopt;
        }
    }

    void TSurfaceCache::Save(const std::filesystem::path& Path, const TSurfaceCacheDescriptor& Descriptor,
        const TSharedSurfaceGeometryData& Geometry)
    {
        Validate(Geometry, Descriptor);
        std::vector<std::uint8_t> Data;
        Data.reserve(EncodedSize(Descriptor));
        Data.insert(Data.end(), Magic.begin(), Magic.end());
        Write32(Data, FormatVersion); Write32(Data, PreprocessVersion); Write64(Data, Descriptor.Fingerprint);
        Write32(Data, static_cast<std::uint32_t>(Descriptor.Surfaces.size()));
        Write32(Data, static_cast<std::uint32_t>(Descriptor.ProfilePaths.size()));
        Write32(Data, Descriptor.TriangleCount); Write64(Data, Geometry.GetTexelCount()); Write64(Data, 0);
        for (const auto& S : Descriptor.Surfaces)
        {
            Write32(Data, S.ID); Write32(Data, S.Resolution.Width); Write32(Data, S.Resolution.Height);
        }
        for (const auto& ProfilePath : Descriptor.ProfilePaths) WriteString(Data, CanonicalPath(ProfilePath));
        for (std::size_t I = 0; I < Geometry.GetTexelCount(); ++I)
        {
            const auto& T = Geometry.GetTexels()[I];
            Write32(Data, T.Surface); Write32(Data, T.Triangle); Write32(Data, T.Chart);
            WriteVec3(Data, T.Barycentric); WriteVec3(Data, T.Position); WriteVec3(Data, T.Normal);
            WriteVec3(Data, T.TransferNormal); WriteVec3(Data, T.MesoNormal);
            Write32(Data, (T.HasTransferNormal ? 1U : 0U) | (T.HasMesoNormal ? 2U : 0U));
            WriteFloat(Data, T.Geometry.MesoVirtualHeight); WriteFloat(Data, T.Geometry.ConcavityWeight);
            WriteFloat(Data, T.Geometry.MesoMeanCurvature); WriteFloat(Data, T.Geometry.MesoGaussianCurvature);
            for (const auto Neighbor : T.NeighborIndices) Write32(Data, Neighbor);
            Write32(Data, Geometry.GetProfileMap()[I]);
            WriteVec3(Data, T.AreaVector);
        }
        if (Data.size() != EncodedSize(Descriptor)) throw std::runtime_error("Surface cache encoding size mismatch.");
        THash PayloadHash; PayloadHash.Bytes(std::span(Data).subspan(HeaderBytes));
        for (int Byte = 0; Byte < 8; ++Byte) Data[HeaderBytes - 8 + Byte] = static_cast<std::uint8_t>(PayloadHash.Value >> (8 * Byte));
        if (!Path.parent_path().empty()) std::filesystem::create_directories(Path.parent_path());
        auto Temporary = Path;
        Temporary += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                     "." + std::to_string(std::random_device{}()) + ".tmp";
        try
        {
            std::ofstream Output(Temporary, std::ios::binary | std::ios::trunc);
            if (!Output) throw std::runtime_error("Unable to create Surface cache: " + Temporary.string());
            Output.write(reinterpret_cast<const char*>(Data.data()), static_cast<std::streamsize>(Data.size()));
            Output.flush();
            if (!Output) throw std::runtime_error("Unable to write Surface cache: " + Temporary.string());
            Output.close();
            if (!Output) throw std::runtime_error("Unable to close Surface cache: " + Temporary.string());
            std::filesystem::rename(Temporary, Path);
        }
        catch (...)
        {
            std::error_code Ignored;
            std::filesystem::remove(Temporary, Ignored);
            throw;
        }
    }
} // namespace MDSS
