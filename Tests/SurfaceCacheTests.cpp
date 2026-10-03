/**
 * @file SurfaceCacheTests.cpp
 * @brief 최종 geometry cache 왕복, 입력 무효화와 손상 복구를 검증한다.
 */
#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/TextureLoader.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResourceLayout.h"
#include "SurfaceStateSystem/Geometry/MesoGeometryBuilder.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/Mapping/NormalMapTransferNormalBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceCache.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
    using namespace MDSS;
    int  Failures = 0;
    void Check(bool Condition, const std::string& Name)
    {
        if (!Condition)
        {
            ++Failures;
            std::cerr << "FAIL: " << Name << '\n';
        }
    }

    struct TTemporaryDirectory
    {
        std::filesystem::path Path =
            std::filesystem::temp_directory_path() /
            ("mdss-surface-cache-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TTemporaryDirectory()
        {
            std::filesystem::create_directories(Path);
        }
        ~TTemporaryDirectory()
        {
            std::error_code Error;
            std::filesystem::remove_all(Path, Error);
        }
    };

    void Write(const std::filesystem::path& Path, const std::string& Text)
    {
        std::ofstream Output(Path, std::ios::binary);
        Output << Text;
        if (!Output)
            throw std::runtime_error("Unable to write test input.");
    }
    std::vector<std::uint8_t> Read(const std::filesystem::path& Path)
    {
        std::ifstream Input(Path, std::ios::binary);
        return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
    }
    void WriteBytes(const std::filesystem::path& Path, const std::vector<std::uint8_t>& Bytes)
    {
        std::ofstream Output(Path, std::ios::binary);
        Output.write(reinterpret_cast<const char*>(Bytes.data()), static_cast<std::streamsize>(Bytes.size()));
    }

    TSharedSurfaceGeometryData Build(const TOBJLoadResult& Mesh, std::uint32_t Resolution)
    {
        auto Mapping = TSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, {{0, {Resolution, Resolution}}});
        std::vector<TSurfaceProfileIndex> Profiles(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t I = 0; I < Mapping.Texels.size(); ++I)
            if (Mapping.Texels[I].IsValid())
                Profiles[I] = 0;
        auto Geometry = TSurfaceGeometryBuilder::Build(Mapping, std::move(Profiles), 1);
        // Vary tangent-space slopes in both directions to exercise the integrated height and curvature fields.
        TextureData NormalMap{2, 2, {120, 125, 255, 255, 140, 118, 255, 255, 112, 142, 255, 255, 136, 138, 255, 255}};
        for (auto& T : Geometry.GetTexels())
        {
            T.HasTransferNormal =
                BuildNormalMapTransferNormal(T, Mesh.Vertices, Mesh.Triangles, NormalMap, T.TransferNormal, true);
        }
        const auto Report = BuildMesoGeometry(Geometry);
        Check(Report.ActiveTexelCount > 0, "test input generates active Meso geometry");
        // Keep a valid, render-only texel and normal fallback flags in the round-trip coverage.
        auto Map = Geometry.GetProfileMap();
        for (std::size_t I = 0; I < Geometry.GetTexelCount(); ++I)
        {
            if (Geometry.GetTexels()[I].IsValid())
            {
                Map[I] = InvalidSurfaceProfileIndex;
                Geometry.GetTexels()[I].HasMesoNormal = false;
                break;
            }
        }
        Geometry.SetProfileMap(std::move(Map));
        return Geometry;
    }

    void Compare(const TSharedSurfaceGeometryData& A, const TSharedSurfaceGeometryData& B)
    {
        Check(A.GetProfileMap() == B.GetProfileMap(), "Profile indices and invalid sentinels round-trip");
        Check(A.GetTexelCount() == B.GetTexelCount(), "texel count round-trips");
        bool Equal = A.GetTexelCount() == B.GetTexelCount();
        for (std::size_t I = 0; Equal && I < A.GetTexelCount(); ++I)
        {
            const auto& X = A.GetTexels()[I];
            const auto& Y = B.GetTexels()[I];
            Equal = X.Surface == Y.Surface && X.Triangle == Y.Triangle && X.Chart == Y.Chart &&
                    X.Barycentric == Y.Barycentric && X.Position == Y.Position && X.Normal == Y.Normal &&
                    X.AreaVector == Y.AreaVector && X.TransferNormal == Y.TransferNormal &&
                    X.HasTransferNormal == Y.HasTransferNormal && X.MesoNormal == Y.MesoNormal &&
                    X.HasMesoNormal == Y.HasMesoNormal &&
                    X.Geometry.MesoVirtualHeight == Y.Geometry.MesoVirtualHeight &&
                    X.Geometry.ConcavityWeight == Y.Geometry.ConcavityWeight &&
                    X.Geometry.MesoMeanCurvature == Y.Geometry.MesoMeanCurvature &&
                    X.Geometry.MesoGaussianCurvature == Y.Geometry.MesoGaussianCurvature &&
                    X.NeighborIndices == Y.NeighborIndices;
        }
        Check(Equal, "all final texel fields round-trip exactly");
        const auto PackedA = PackSharedSurfaceGeometry(A);
        const auto PackedB = PackSharedSurfaceGeometry(B);
        Check(PackedA.ReverseNeighborSlots == PackedB.ReverseNeighborSlots, "GPU reverse slots match after cache load");
        const auto Model = glm::scale(glm::rotate(glm::mat4(1), 0.7F, glm::vec3(0, 1, 0)), glm::vec3(2, 1, 3));
        Check(BuildSurfaceGPUTransferWeights(A, Model, nullptr, true, true, true) ==
                  BuildSurfaceGPUTransferWeights(B, Model, nullptr, true, true, true),
              "cached geometry gives identical TransferWeights under nonuniform scale");
    }

    void TestBrickCubeMesoHeightSign()
    {
        const auto AssetDirectory =
            std::filesystem::path(MDSS_TEST_FIXTURE_DIR).parent_path().parent_path() / "Assets/Meshes/BrickCube";
        const auto                      Mesh = TOBJLoader::Load(AssetDirectory / "BrickCube.obj");
        const auto                      NormalMap = TextureLoader::LoadRGBA8(AssetDirectory / "BrickCubeNormal.png");
        const auto                      Albedo = TextureLoader::LoadRGBA8(AssetDirectory / "BrickCubeAlbedo.png");
        std::vector<TSurfaceDefinition> Surfaces;
        for (const auto& Triangle : Mesh.Triangles)
        {
            while (Surfaces.size() <= Triangle.Surface)
                Surfaces.push_back({static_cast<TSurfaceLocalID>(Surfaces.size()), {64, 64}});
        }
        const auto Mapping = TSurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, Surfaces);
        std::vector<TSurfaceProfileIndex> Profiles(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t I = 0; I < Mapping.Texels.size(); ++I)
            if (Mapping.Texels[I].IsValid())
                Profiles[I] = 0;
        auto Geometry = TSurfaceGeometryBuilder::Build(Mapping, std::move(Profiles), 1);
        for (auto& Texel : Geometry.GetTexels())
        {
            if (!Texel.IsValid())
                continue;
            Texel.HasTransferNormal = BuildNormalMapTransferNormal(
                Texel, Mesh.Vertices, Mesh.Triangles, NormalMap, Texel.TransferNormal, true);
        }
        const auto Report = BuildMesoGeometry(Geometry);
        Check(Report.ActiveTexelCount > 0, "BrickCube generates Meso heights");

        double      BrickHeight = 0.0, MortarHeight = 0.0;
        std::size_t BrickCount = 0, MortarCount = 0;
        for (const auto& Texel : Geometry.GetTexels())
        {
            if (!Texel.IsValid() || !Texel.HasTransferNormal)
                continue;
            const auto& Triangle = Mesh.Triangles[Texel.Triangle];
            const auto  UV = Texel.Barycentric.x * Mesh.Vertices[Triangle.RenderVertexIndices[0]].UV +
                            Texel.Barycentric.y * Mesh.Vertices[Triangle.RenderVertexIndices[1]].UV +
                            Texel.Barycentric.z * Mesh.Vertices[Triangle.RenderVertexIndices[2]].UV;
            const auto  X = std::min(static_cast<std::uint32_t>(UV.x * Albedo.Width), Albedo.Width - 1U);
            const auto  Y = std::min(static_cast<std::uint32_t>(UV.y * Albedo.Height), Albedo.Height - 1U);
            const auto  Pixel = (static_cast<std::size_t>(Y) * Albedo.Width + X) * 4U;
            const float Red = Albedo.Pixels[Pixel];
            const float Green = Albedo.Pixels[Pixel + 1U];
            const float Blue = Albedo.Pixels[Pixel + 2U];
            if (Red > 1.35F * Green && Red > 1.20F * Blue)
            {
                BrickHeight += Texel.Geometry.MesoVirtualHeight;
                ++BrickCount;
            }
            else if (Red < 1.22F * Green && Green > 1.05F * Blue)
            {
                MortarHeight += Texel.Geometry.MesoVirtualHeight;
                ++MortarCount;
            }
        }
        Check(BrickCount > 100 && MortarCount > 100, "BrickCube samples both brick and mortar texels");
        if (BrickCount > 0 && MortarCount > 0)
        {
            const double MeanBrick = BrickHeight / BrickCount;
            const double MeanMortar = MortarHeight / MortarCount;
            Check(MeanBrick > 0.0 && MeanMortar < 0.0 && MeanBrick > MeanMortar,
                  "BrickCube mortar reconstructs as negative height below the brick faces");
        }
    }

    void Tests()
    {
        TestBrickCubeMesoHeightSign();
        TTemporaryDirectory Temp;
        const auto          Distribution = Temp.Path / "Input.SurfaceProfileMap";
        const auto          Profile = Temp.Path / "Input.SRProfile";
        const auto          Normal = Temp.Path / "normal.bin";
        Write(Distribution, "surface profile assignment v1");
        Write(Profile, "response v1");
        Write(Normal, "normal v1");
        auto Mesh = TOBJLoader::Load(std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / "Mapping/QuadNoSeam.obj");
        auto Describe = [&](std::uint32_t Resolution = 8)
        {
            return TSurfaceCache::Describe(Mesh.Vertices,
                                           Mesh.Triangles,
                                           {{0, {Resolution, Resolution}}},
                                           std::vector<std::filesystem::path>{Normal},
                                           Distribution,
                                           {Profile},
                                           std::vector<TSurfaceProfileIndex>{0});
        };
        const auto  Descriptor = Describe();
        const auto  Cache = TSurfaceCache::GetPath(Temp.Path / "Cache", "Cube.obj", Distribution, 8);
        std::string Diagnostic;
        Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic) && Diagnostic == "missing cache",
              "missing cache requests rebuild");
        auto Geometry = Build(Mesh, 8);
        TSurfaceCache::Save(Cache, Descriptor, Geometry);
        auto Loaded = TSurfaceCache::Load(Cache, Descriptor, Diagnostic);
        Check(Loaded.has_value() && Diagnostic.empty(), "saved final geometry loads");
        if (Loaded)
            Compare(Geometry, *Loaded);
        const auto GoodBytes = Read(Cache);

        auto Changed = Descriptor;
        ++Changed.Fingerprint;
        Check(!TSurfaceCache::Load(Cache, Changed, Diagnostic), "input fingerprint mismatch requests rebuild");
        Changed = Descriptor;
        Changed.Surfaces[0].Resolution = {4, 4};
        Check(!TSurfaceCache::Load(Cache, Changed, Diagnostic), "resolution mismatch requests rebuild");
        Changed = Descriptor;
        Changed.ProfilePaths[0] = Temp.Path / "Other.SRProfile";
        Check(!TSurfaceCache::Load(Cache, Changed, Diagnostic), "Profile table binding mismatch requests rebuild");

        // Stale versions, corrupt payload, truncation and trailing bytes never become Runtime data.
        for (const std::size_t Offset : {std::size_t(0), std::size_t(8), std::size_t(12), GoodBytes.size() - 1})
        {
            auto Bad = GoodBytes;
            Bad[Offset] ^= 0x40;
            WriteBytes(Cache, Bad);
            Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic), "corrupt/versioned cache requests rebuild");
        }
        auto Bad = GoodBytes;
        Bad.resize(20);
        WriteBytes(Cache, Bad);
        Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic), "truncated header requests rebuild");
        Bad = GoodBytes;
        Bad.pop_back();
        WriteBytes(Cache, Bad);
        Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic), "truncated payload requests rebuild");
        Bad = GoodBytes;
        Bad.push_back(0);
        WriteBytes(Cache, Bad);
        Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic), "trailing payload requests rebuild");

        // A valid checksum alone must not admit invalid Runtime geometry.
        std::size_t FirstValid = 0;
        while (!Geometry.GetTexels()[FirstValid].IsValid())
            ++FirstValid;
        const std::size_t RecordsStart =
            52 + 12 + 4 + std::filesystem::absolute(Profile).lexically_normal().generic_string().size();
        auto RejectRecord = [&](std::size_t FieldOffset, std::uint32_t Value, const std::string& ExpectedDiagnostic)
        {
            auto       Bytes = GoodBytes;
            const auto Offset = RecordsStart + FirstValid * 140 + FieldOffset;
            for (int I = 0; I < 4; ++I)
                Bytes[Offset + I] = static_cast<std::uint8_t>(Value >> (I * 8));
            std::uint64_t Hash = 14695981039346656037ULL;
            for (std::size_t I = 52; I < Bytes.size(); ++I)
                Hash = (Hash ^ Bytes[I]) * 1099511628211ULL;
            for (int I = 0; I < 8; ++I)
                Bytes[44 + I] = static_cast<std::uint8_t>(Hash >> (I * 8));
            WriteBytes(Cache, Bytes);
            Check(!TSurfaceCache::Load(Cache, Descriptor, Diagnostic) &&
                      Diagnostic.find(ExpectedDiagnostic) != std::string::npos,
                  "checked payload rejects " + ExpectedDiagnostic);
        };
        RejectRecord(72, 4, "normal flags");
        RejectRecord(24, 0x7fc00000U, "geometry values"); // NaN Position.x
        RejectRecord(92, UINT32_MAX - 1, "neighbor indices");
        RejectRecord(0, 99, "texel mapping");
        TSurfaceCache::Save(Cache, Descriptor, Geometry);
        Check(TSurfaceCache::Load(Cache, Descriptor, Diagnostic).has_value(),
              "corrupt file is replaced by rebuilt data");

        auto Invalid = Geometry;
        for (auto& T : Invalid.GetTexels())
            if (T.IsValid())
            {
                T.NeighborIndices[0] = UINT32_MAX - 1;
                break;
            }
        bool Rejected = false;
        try
        {
            TSurfaceCache::Save(Cache, Descriptor, Invalid);
        }
        catch (const std::exception&)
        {
            Rejected = true;
        }
        Check(Rejected && Read(Cache) == GoodBytes, "invalid save preserves previous complete cache");
        const auto BlockedRoot = Temp.Path / "blocked";
        Write(BlockedRoot, "file, not directory");
        Rejected = false;
        try
        {
            TSurfaceCache::Save(BlockedRoot / "Cube.Surface", Descriptor, Geometry);
        }
        catch (const std::exception&)
        {
            Rejected = true;
        }
        Check(Rejected && Geometry.GetTexelCount() == 64, "save failure leaves built Runtime geometry usable");

        const auto Cache4 = TSurfaceCache::GetPath(Temp.Path / "Cache", "Cube.obj", Distribution, 4);
        TSurfaceCache::Save(Cache4, Describe(4), Build(Mesh, 4));
        Check(Cache != Cache4 && TSurfaceCache::Load(Cache, Descriptor, Diagnostic).has_value() &&
                  TSurfaceCache::Load(Cache4, Describe(4), Diagnostic).has_value(),
              "resolution variants coexist and reload");
        Check(TSurfaceCache::GetPath(Temp.Path / "Cache", "Cube.obj", Temp.Path / "Other.SurfaceProfileMap", 8) !=
                  Cache,
              "different Profile maps have separate identity directories");
        Check(TSurfaceCache::GetPath(Temp.Path / "Cache", "./Cube.obj", Distribution, 8) == Cache,
              "normalized Mesh paths share an identity");

        Write(Profile, "different capacities, channels and response rates");
        Check(Describe().Fingerprint == Descriptor.Fingerprint, "SRProfile parameter/channel edits reuse geometry");
        const auto NormalTime = std::filesystem::last_write_time(Normal);
        Write(Normal, "normal v2");
        std::filesystem::last_write_time(Normal, NormalTime);
        Check(Describe().Fingerprint != Descriptor.Fingerprint,
              "Normal Map bytes invalidate even with unchanged size/mtime");
        Write(Normal, "normal v1");
        Write(Distribution, "surface profile assignment v2");
        Check(Describe().Fingerprint != Descriptor.Fingerprint, "Profile Distribution bytes invalidate cache");
        Write(Distribution, "surface profile assignment v1");
        Check(Describe(4).Fingerprint != Descriptor.Fingerprint, "resolution affects fingerprint");
        Mesh.Vertices[0].Tangent.w *= -1;
        Check(Describe().Fingerprint != Descriptor.Fingerprint, "tangent handedness affects fingerprint");
        Mesh.Vertices[0].Tangent.w *= -1;
        const auto OriginalUV = Mesh.Vertices[0].UV;
        Mesh.Vertices[0].UV.x += 0.01F;
        Check(Describe().Fingerprint != Descriptor.Fingerprint, "simulation UV affects fingerprint");
        Mesh.Vertices[0].UV = OriginalUV;
        const auto OriginalPosition = Mesh.Vertices[0].Position;
        Mesh.Vertices[0].Position.x += 0.5F;
        Check(Describe().Fingerprint != Descriptor.Fingerprint, "mesh positions affect fingerprint");
        Mesh.Vertices[0].Position = OriginalPosition;
        const auto Reordered = TSurfaceCache::Describe(Mesh.Vertices,
                                                       Mesh.Triangles,
                                                       {{0, {8, 8}}},
                                                       std::vector<std::filesystem::path>{Normal},
                                                       Distribution,
                                                       {Profile, Temp.Path / "Second.SRProfile"},
                                                       std::vector<TSurfaceProfileIndex>{0});
        const auto Reversed = TSurfaceCache::Describe(Mesh.Vertices,
                                                      Mesh.Triangles,
                                                      {{0, {8, 8}}},
                                                      std::vector<std::filesystem::path>{Normal},
                                                      Distribution,
                                                      {Temp.Path / "Second.SRProfile", Profile},
                                                      std::vector<TSurfaceProfileIndex>{1});
        Check(Reordered.Fingerprint != Reversed.Fingerprint,
              "ordered Profile table and assignments affect fingerprint");
        Mesh.Triangles[0].OriginalPositionIndices[0] += 10;
        Check(Describe().Fingerprint != Descriptor.Fingerprint, "source topology affects fingerprint");

        // A seam graph survives serialization, preserving reciprocal links and GPU transfer behavior.
        auto       SeamMesh = TOBJLoader::Load(std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / "Mapping/QuadSeam.obj");
        const auto SeamDescriptor = TSurfaceCache::Describe(SeamMesh.Vertices,
                                                            SeamMesh.Triangles,
                                                            {{0, {8, 8}}},
                                                            std::vector<std::filesystem::path>{Normal},
                                                            Distribution,
                                                            {Profile},
                                                            std::vector<TSurfaceProfileIndex>{0});
        auto       SeamGeometry = Build(SeamMesh, 8);
        TSurfaceCache::Save(Cache, SeamDescriptor, SeamGeometry);
        Loaded = TSurfaceCache::Load(Cache, SeamDescriptor, Diagnostic);
        Check(Loaded.has_value(), "atomic overwrite loads new seam geometry");
        if (Loaded)
            Compare(SeamGeometry, *Loaded);
        for (const auto& Entry : std::filesystem::recursive_directory_iterator(Temp.Path))
            Check(Entry.path().extension() != ".tmp", "save leaves no temporary files");
    }
}

int main()
{
    try
    {
        Tests();
    }
    catch (const std::exception& Error)
    {
        ++Failures;
        std::cerr << "Unexpected failure: " << Error.what() << '\n';
    }
    if (Failures == 0)
        std::cout << "Surface cache tests passed.\n";
    return Failures == 0 ? 0 : 1;
}
