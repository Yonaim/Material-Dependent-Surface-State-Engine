/**
 * @file SharedSurfaceGeometryTests.cpp
 * @brief Shared geometry construction and Runtime preprocessing tests.
 */

#include "AssetManager/Loaders/OBJLoader.h"
#include "AssetManager/Loaders/SurfaceProfileDistributionLoader.h"
#include "SurfaceStateSystem/Geometry/SurfaceGeometryBuilder.h"
#include "SurfaceStateSystem/Mapping/SurfaceMappingBuilder.h"
#include "SurfaceStateSystem/Preprocessing/SurfaceRuntimeData.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <functional>
#include <glm/geometric.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    int FailureCount = 0;

    void Check(bool Condition, const std::string& Message)
    {
        if (!Condition)
        {
            ++FailureCount;
            std::cerr << "FAIL: " << Message << '\n';
        }
    }

    template <typename TFunction>
    void CheckThrows(TFunction&& Function, const std::string& ExpectedMessage, const std::string& TestName)
    {
        try
        {
            std::invoke(std::forward<TFunction>(Function));
            Check(false, TestName + " did not throw");
        }
        catch (const std::exception& Exception)
        {
            Check(std::string(Exception.what()).find(ExpectedMessage) != std::string::npos,
                  TestName + " did not include expected diagnostic '" + ExpectedMessage + "'");
        }
    }

    std::filesystem::path Fixture(const char* RelativePath)
    {
        return std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / RelativePath;
    }

    MDSS::SurfaceMappingData BuildQuad()
    {
        const MDSS::OBJLoadResult Mesh = MDSS::OBJLoader::Load(Fixture("Mapping/QuadNoSeam.obj"));
        return MDSS::SurfaceMappingBuilder::Build(Mesh.Vertices, Mesh.Triangles, {{0, {8, 8}}});
    }

    void TestProfileDistribution()
    {
        using namespace MDSS;
        const SurfaceProfileDistribution Distribution =
            SurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        Check(Distribution.ProfilePaths.size() == 1, "distribution should retain its ordered Profile path table");
        Check(Distribution.ProfilePaths[0].filename() == "Valid.SRProfile",
              "relative Profile path should resolve against the sidecar directory");

        const SurfaceMappingData               Mapping = BuildQuad();
        const std::vector<SurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);
        Check(ProfileMap.size() == Mapping.Texels.size(), "distribution should output one entry per texel");
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            Check(ProfileMap[Index] == (Mapping.Texels[Index].IsValid() ? 0U : InvalidSurfaceProfileIndex),
                  "valid texels should inherit their Surface Profile and invalid texels should keep sentinel");
        }

        CheckThrows(
            []
            {
                (void)SurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/OutOfRange.SurfaceProfileMap"));
            },
            "outside the profiles array",
            "out-of-range Profile index");
        CheckThrows(
            []
            {
                (void)SurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/MissingSurface.SurfaceProfileMap"));
            },
            "missing surfaceId 0",
            "missing Surface assignment");
        CheckThrows(
            []
            {
                (void)SurfaceProfileDistributionLoader::Load(
                    Fixture("SurfaceProfileMaps/InvalidIndexType.SurfaceProfileMap"));
            },
            "non-negative integer",
            "non-integer Surface ID");
    }

    void TestGeometryBuild()
    {
        using namespace MDSS;
        const SurfaceMappingData         Mapping = BuildQuad();
        std::vector<SurfaceProfileIndex> ProfileMap(Mapping.Texels.size(), InvalidSurfaceProfileIndex);
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            if (Mapping.Texels[Index].IsValid())
            {
                ProfileMap[Index] = 0;
            }
        }

        SharedSurfaceGeometryData Geometry = SurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1);
        Check(Geometry.GetTexelCount() == Mapping.Texels.size(), "shared geometry should preserve mapping texel count");
        Check(Geometry.GetSurface(0).Resolution == SurfaceResolution{8, 8},
              "shared geometry should preserve Surface grid resolution");

        bool FoundValid = false;
        for (std::size_t Index = 0; Index < Mapping.Texels.size(); ++Index)
        {
            const SurfaceTexelGeometry& GeometryTexel = Geometry.GetTexels()[Index];
            if (!Mapping.Texels[Index].IsValid())
            {
                Check(!GeometryTexel.IsValid(), "invalid mapping texels should remain invalid in shared geometry");
                Check(Geometry.GetProfileIndex(static_cast<LocalTexelIndex>(Index)) == InvalidSurfaceProfileIndex,
                      "invalid geometry texels should retain the Profile sentinel");
                continue;
            }

            FoundValid = true;
            Check(GeometryTexel.Position == Mapping.Texels[Index].Position,
                  "builder should copy barycentric surface position");
            Check(GeometryTexel.Normal == Mapping.Texels[Index].Normal, "builder should copy surface normal");
            Check(GeometryTexel.NeighborIndices == Mapping.Texels[Index].Neighbors,
                  "builder should copy the seam-aware neighbor graph");
            Check(GeometryTexel.Geometry.MesoVirtualHeight == 0.0F && GeometryTexel.Geometry.ConcavityWeight == 0.0F,
                  "not-yet-built geometry scalars should have zero defaults");
            Check(Geometry.GetProfileIndex(static_cast<LocalTexelIndex>(Index)) == 0,
                  "valid texels should retain the Profile assigned to their Surface");
        }
        Check(FoundValid, "fixture should include valid mapping texels");

        std::size_t CheckedNeighborDistances = 0;
        for (std::size_t Index = 0; Index < Geometry.GetTexelCount(); ++Index)
        {
            const SurfaceTexelGeometry& Texel = Geometry.GetTexels()[Index];
            if (!Texel.IsValid())
            {
                continue;
            }
            for (const LocalTexelIndex Neighbor : Texel.NeighborIndices)
            {
                if (Neighbor == InvalidTexelIndex)
                {
                    continue;
                }
                const glm::vec3 Delta = Geometry.GetTexels()[Neighbor].Position - Texel.Position;
                const float     Distance = glm::length(Delta);
                Check(std::isfinite(Distance) && Distance > 0.0F,
                      "neighbor distance should be finite and computed on demand from positions");
                const auto& ReverseNeighbors = Geometry.GetTexels()[Neighbor].NeighborIndices;
                Check(std::ranges::find(ReverseNeighbors, static_cast<LocalTexelIndex>(Index)) !=
                          ReverseNeighbors.end(),
                      "geometry neighbor links should remain bidirectional");
                ++CheckedNeighborDistances;
            }
        }
        Check(CheckedNeighborDistances != 0, "fixture should include neighbors for on-demand distance checks");

        const auto        ValidMappingTexel = std::ranges::find_if(Mapping.Texels, &SurfaceMappingTexel::IsValid);
        const std::size_t ValidTexelIndex = static_cast<std::size_t>(ValidMappingTexel - Mapping.Texels.begin());
        ProfileMap[ValidTexelIndex] = InvalidSurfaceProfileIndex;
        CheckThrows([&] { (void)SurfaceGeometryBuilder::Build(Mapping, ProfileMap, 1); },
                    "Valid texel Profile index",
                    "valid texel without a Profile");
    }

    void TestRuntimePreprocessing()
    {
        using namespace MDSS;
        const SurfaceProfileDistribution Distribution =
            SurfaceProfileDistributionLoader::Load(Fixture("SurfaceProfileMaps/Valid.SurfaceProfileMap"));
        const SurfaceMappingData               Mapping = BuildQuad();
        const std::vector<SurfaceProfileIndex> ProfileMap = Distribution.BuildTexelProfileMap(Mapping);

        const SurfaceRuntimeData First = SurfacePreprocessor::Build(Mapping, ProfileMap, 1);
        const SurfaceRuntimeData Second = SurfacePreprocessor::Build(Mapping, ProfileMap, 1);
        Check(First.Geometry->GetTexelCount() == Mapping.Texels.size(),
              "Runtime preprocessing should produce one geometry texel per mapping texel");
        Check(First.Geometry->GetSurfaces().size() == Second.Geometry->GetSurfaces().size() &&
                  First.Geometry->GetSurfaces()[0].Surface == Second.Geometry->GetSurfaces()[0].Surface &&
                  First.Geometry->GetSurfaces()[0].Resolution == Second.Geometry->GetSurfaces()[0].Resolution &&
                  First.Geometry->GetSurfaces()[0].FirstTexel == Second.Geometry->GetSurfaces()[0].FirstTexel &&
                  First.Geometry->GetSurfaces()[0].TexelCount == Second.Geometry->GetSurfaces()[0].TexelCount,
              "identical Runtime inputs should deterministically reproduce Surface ranges");
        Check(First.Geometry->GetProfileMap() == Second.Geometry->GetProfileMap(),
              "identical Runtime inputs should deterministically reproduce the texel Profile map");
    }
} // namespace

int main()
{
    TestProfileDistribution();
    TestGeometryBuild();
    TestRuntimePreprocessing();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " shared geometry test(s) failed.\n";
        return 1;
    }
    std::cout << "All shared geometry tests passed.\n";
    return 0;
}
