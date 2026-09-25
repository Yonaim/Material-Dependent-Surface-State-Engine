/**
 * @file SurfaceStateContractTests.cpp
 * @brief Dynamic State Registry, Profile, shared Surface and cache contracts.
 */

#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/Preprocessing/SurfacePreprocessedAsset.h"
#include "SurfaceStateSystem/State/SurfaceInput.h"
#include "SurfaceStateSystem/State/SurfaceInstanceStateData.h"
#include "SurfaceStateSystem/Types/SurfaceStateRegistry.h"

#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
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

    template <typename TFunction> void CheckDoesNotThrow(TFunction&& Function, const std::string& TestName)
    {
        try
        {
            std::invoke(std::forward<TFunction>(Function));
        }
        catch (const std::exception& Exception)
        {
            Check(false, TestName + " unexpectedly threw: " + Exception.what());
        }
    }

    std::filesystem::path GetFixturePath(const char* FileName)
    {
        return std::filesystem::path(MDSS_TEST_FIXTURE_DIR) / FileName;
    }

    void TestProfileAndRegistry()
    {
        using namespace MDSS;

        Check(NormalizeSurfaceStateName("  WeTnEsS \t") == "wetness", "State names should trim and lowercase");
        Check(NormalizeSurfaceStateName("surface_heat") != NormalizeSurfaceStateName("surface-heat"),
              "normalization should preserve punctuation");
        Check(NormalizeSurfaceStateName("surface heat") != NormalizeSurfaceStateName("surface  heat"),
              "normalization should preserve internal whitespace");

        const auto Profile = SRProfileLoader::Load(7, GetFixturePath("Valid.SRProfile"));
        const auto PartialProfile = SRProfileLoader::Load(8, GetFixturePath("MissingState.SRProfile"));
        Check(Profile->GetName() == "TestProfile", "profile name should come from file data");
        Check(Profile->GetData().States.size() == 2, "profile should load only its declared States");
        Check(Profile->GetData().States.at("wetness").StateCapacity == 2.0F,
              "State parameters should be indexed by canonical name");
        Check(Profile->GetData().Transitions[0].Source == "mud" &&
                  Profile->GetData().Transitions[0].Target == "wetness",
              "transition endpoint names should be normalized");
        Check(PartialProfile->GetData().States.contains("snow"), "arbitrary State names should be accepted");

        const auto UnknownTransition = SRProfileLoader::Load(9, GetFixturePath("UnknownState.SRProfile"));
        CheckThrows([&] { (void)SurfaceStateRegistry({UnknownTransition->GetData()}); },
                    "unknown target State 'wetness'",
                    "transition endpoint missing from loaded Profile set");

        CheckThrows([&] { (void)SRProfileLoader::Load(10, GetFixturePath("WrongFieldType.SRProfile")); },
                    "states.wetness.decayRate must be a number",
                    "wrong JSON field type");
        CheckThrows([&] { (void)SRProfileLoader::Load(11, GetFixturePath("MissingParameter.SRProfile")); },
                    "states.wetness.inputFactor is required",
                    "missing parameter");

        SurfaceResponseProfileData InvalidProfile;
        InvalidProfile.States["wetness"].StateCapacity = 0.0F;
        CheckThrows([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "stateCapacity", "zero capacity");
        InvalidProfile = {};
        InvalidProfile.States["wetness"].DecayRate = -0.1F;
        CheckThrows([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "decayRate", "negative rate");
        InvalidProfile = {};
        InvalidProfile.States["wetness"].CavityRetentionFactor = 1.1F;
        CheckThrows([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "[0, 1]", "factor over one");
        InvalidProfile = {};
        InvalidProfile.States["wetness"].CavityRetentionFactor = 0.0F;
        InvalidProfile.States["wetness"].CavityFillFactor = 1.0F;
        CheckDoesNotThrow([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "factor endpoints");
        InvalidProfile.Transitions.push_back({"wetness", "wetness", 0.5F, 1.0F});
        CheckThrows(
            [&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "different", "normalized self transition");

        SurfaceStateRegistry Registry({Profile->GetData(), PartialProfile->GetData()});
        Check(Registry.GetStateCount() == 3, "Registry should union State names from all Profiles");
        Check(Registry.GetStateId(" WETNESS ") == Registry.GetStateId("wetness"),
              "Registry lookup should normalize names");
        Check(Registry.GetStateName(Registry.GetStateId("snow")) == "snow", "ID should resolve to State name");
        CheckThrows([&] { (void)Registry.GetStateId("steam"); }, "Unknown State", "unregistered State lookup");

        const auto Resolved = Registry.ResolveProfile(Profile->GetData());
        Check(Resolved.States.size() == Registry.GetStateCount(), "resolved data should use registry channel count");
        Check(Resolved.States[Registry.GetStateId("wetness")].has_value() &&
                  Resolved.States[Registry.GetStateId("wetness")]->StateCapacity == 2.0F,
              "parameters should map to their registry channel");
        Check(!Resolved.States[Registry.GetStateId("snow")].has_value(),
              "Profile channels absent from a material response should remain explicitly unsupported");
        Check(Resolved.Transitions[0].Source == Registry.GetStateId("mud") &&
                  Resolved.Transitions[0].Target == Registry.GetStateId("wetness"),
              "transitions should resolve to runtime IDs");

        SurfaceStateRegistry ReorderedRegistry({PartialProfile->GetData(), Profile->GetData()});
        Check(ReorderedRegistry.GetStateId("snow") == Registry.GetStateId("snow") &&
                  ReorderedRegistry.GetStateId("wetness") == Registry.GetStateId("wetness"),
              "Registry IDs should be independent of Profile load order");
    }

    void TestGeometryAndInstanceData()
    {
        using namespace MDSS;

        auto Geometry =
            std::make_shared<SharedSurfaceGeometryData>(std::vector<SurfaceDefinition>{{0, {2, 2}}, {1, {3, 1}}});
        Check(Geometry->GetTexelCount() == 7, "geometry texel count should sum Surface grids");
        Check(!Geometry->GetTexels()[0].IsValid(), "unmapped texels should start invalid");

        Geometry->GetTexels()[0].Surface = 0;
        Geometry->GetTexels()[0].Triangle = 0;
        std::vector<SurfaceProfileIndex> ProfileMap(7, InvalidSurfaceProfileIndex);
        ProfileMap[0] = 5;
        Geometry->SetProfileMap(std::move(ProfileMap));
        SurfaceInstanceStateData Instance(3, Geometry, 3);

        Check(Instance.GetID() == 3 && Instance.GetStateCount() == 3, "instance should retain ID and dynamic channels");
        Check(Instance.GetProfileIndex(0) == 5, "texel Profile index should come from shared SurfaceProfileMap");
        Check(Instance.GetStates().size() == 7, "instance should own one state vector per texel");
        for (const SurfaceStateValues& State : Instance.GetStates())
        {
            Check(State == SurfaceStateValues(3, 0.0F), "all dynamic State channels should start at zero");
        }

        CheckThrows([] { (void)SharedSurfaceGeometryData({}); }, "at least one Surface", "empty Surface list");
        CheckThrows([] { (void)SharedSurfaceGeometryData({{InvalidSurfaceID, {1, 1}}}); },
                    "InvalidSurfaceID is reserved",
                    "reserved Surface sentinel");
        CheckThrows([] { (void)SurfaceResolution{0, 8}.GetTexelCount(); }, "greater than zero", "zero resolution");
        CheckThrows([&] { (void)Instance.GetProfileIndex(7); }, "not present", "out-of-range texel profile lookup");
        CheckThrows(
            [&] { Geometry->SetProfileMap({InvalidSurfaceProfileIndex}); }, "one entry per texel", "wrong map size");

        std::vector<SurfaceProfileIndex> InvalidMap(7, InvalidSurfaceProfileIndex);
        CheckThrows([&] { Geometry->SetProfileMap(std::move(InvalidMap)); },
                    "Valid texels require",
                    "valid texel assigned invalid Profile sentinel");
    }

    void TestContactInputType()
    {
        MDSS::SurfaceContactInput Input;
        Input.TargetInstance = 11;
        Input.State = 3;
        Input.Radius = 0.25F;
        Input.Strength = 0.8F;
        Check(Input.TargetInstance == 11 && Input.State == 3, "input should reference instance and registry State ID");
        Check(Input.Falloff == 1.0F, "contact input should have a stable default falloff");
    }

    void TestSurfaceCache()
    {
        using namespace MDSS;

        SurfaceMappingData Mapping;
        Mapping.Surfaces.push_back({0, {2, 1}, 0, 2});
        Mapping.Texels.resize(2);
        Mapping.Texels[0].Surface = 0;
        Mapping.Texels[0].Triangle = 4;
        Mapping.Texels[0].Chart = 0;
        Mapping.Texels[0].Position = {1.0F, 2.0F, 3.0F};
        Mapping.Texels[0].Normal = {0.0F, 0.0F, 1.0F};

        const SurfaceCacheMetadata Metadata = SurfacePreprocessor::CreateMetadata(
            GetFixturePath("Valid.SRProfile"), {}, {7, InvalidSurfaceProfileIndex}, {{2, 1}}, 0, 1, 8);
        const SurfacePreprocessedAsset Asset =
            SurfacePreprocessor::Build(Mapping, {7, InvalidSurfaceProfileIndex}, 8, Metadata);
        Check(Asset.Geometry.GetProfileIndex(0) == 7, "valid texel should retain its Profile index");
        Check(Asset.Geometry.GetProfileIndex(1) == InvalidSurfaceProfileIndex,
              "invalid texel should use reserved Profile sentinel");
        Check(Asset.Metadata.ProfileMapHash == SurfacePreprocessor::HashProfileMap({7, InvalidSurfaceProfileIndex}),
              "metadata should fingerprint Profile map contents");

        const auto Timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto CachePath =
            std::filesystem::temp_directory_path() / ("mdssp_surface_cache_" + std::to_string(Timestamp) + ".Surface");
        try
        {
            SurfaceCache::Save(CachePath, Asset);
            const SurfacePreprocessedAsset Loaded = SurfaceCache::Load(CachePath, Asset.Metadata);
            Check(Loaded.Geometry.GetTexelCount() == 2, "cache round trip should retain texel count");
            Check(Loaded.Geometry.GetTexels()[0].Position == glm::vec3(1.0F, 2.0F, 3.0F),
                  "cache round trip should retain geometry data");
            Check(Loaded.Geometry.GetProfileIndex(0) == 7, "cache round trip should retain Profile map");

            SurfaceCacheMetadata Stale = Asset.Metadata;
            ++Stale.UVSet;
            CheckThrows([&] { (void)SurfaceCache::Load(CachePath, Stale); }, "stale", "stale cache input metadata");
        }
        catch (const std::exception& Exception)
        {
            Check(false, std::string("Surface cache round trip failed: ") + Exception.what());
        }
        std::error_code RemoveError;
        std::filesystem::remove(CachePath, RemoveError);
        Check(!RemoveError, "temporary Surface cache should be cleaned up");

        CheckThrows([&] { (void)SurfacePreprocessor::Build(Mapping, {7}, 8, Metadata); },
                    "exactly one entry per mapping texel",
                    "profile map with wrong texel count");
        SurfaceCacheMetadata InvalidRangeMetadata;
        InvalidRangeMetadata.MeshHash = "mesh-hash";
        CheckThrows(
            [&]
            { (void)SurfacePreprocessor::Build(Mapping, {8, InvalidSurfaceProfileIndex}, 8, InvalidRangeMetadata); },
            "outside the loaded Profile range",
            "profile map index outside registered profile range");
    }
} // namespace

int main()
{
    TestProfileAndRegistry();
    TestGeometryAndInstanceData();
    TestContactInputType();
    TestSurfaceCache();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " contract test(s) failed.\n";
        return 1;
    }

    std::cout << "All Surface State contract tests passed.\n";
    return 0;
}
