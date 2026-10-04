/**
 * @file SurfaceStateContractTests.cpp
 * @brief 동적 State registry, Profile, 공유 Surface와 Runtime preprocessing 계약을 검증한다.
 */

#include "AssetManager/Assets/SRProfileAsset.h"
#include "AssetManager/Loaders/SRProfileLoader.h"
#include "SurfaceState/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceState/Preprocessing/SurfaceRuntimeData.h"
#include "SurfaceState/State/SurfaceInput.h"
#include "SurfaceState/State/SurfaceInstanceStateData.h"
#include "SurfaceState/Types/SurfaceStateRegistry.h"

#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
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
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        Check(NormalizeSurfaceStateName("  WeTnEsS \t") == "wetness", "State names should trim and lowercase");
        Check(NormalizeSurfaceStateName("surface_heat") != NormalizeSurfaceStateName("surface-heat"),
              "normalization should preserve punctuation");
        Check(NormalizeSurfaceStateName("surface heat") != NormalizeSurfaceStateName("surface  heat"),
              "normalization should preserve internal whitespace");

        const auto Profile = TSRProfileLoader::Load(7, GetFixturePath("Valid.SRProfile"));
        const auto PartialProfile = TSRProfileLoader::Load(8, GetFixturePath("MissingState.SRProfile"));
        Check(Profile->GetName() == "TestProfile", "profile name should come from file data");
        Check(Profile->GetData().States.size() == 2, "profile should load only its declared States");
        Check(Profile->GetData().States.at("wetness").StateCapacity == 2.0F,
              "State parameters should be indexed by canonical name");
        Check(Profile->GetData().States.at("wetness").SaturationTransferFactor == 0.4F &&
                  Profile->GetData().States.at("wetness").GeometryTransferFactor == 0.0005F,
              "version 4 profiles should load normalized transfer factors without applying solver rates");
        Check(Profile->GetData().States.at("wetness").CavityTransportRetentionFactor == 0.6F &&
                  Profile->GetData().States.at("wetness").CavityRetentionFactor == 0.5F,
              "version 4 cavity resistance and decay protection keys should map independently");
        Check(PartialProfile->GetData().States.at("snow").SaturationTransferFactor == 0.2F,
              "version 3 profile keys remain readable");
        Check(PartialProfile->GetData().States.at("snow").CavityTransportRetentionFactor == 0.4F,
              "version 3 optional cavity exit key remains readable");
        Check(Profile->GetData().Transitions[0].Source == "mud" &&
                  Profile->GetData().Transitions[0].Target == "wetness",
              "transition endpoint names should be normalized");
        Check(PartialProfile->GetData().States.contains("snow"), "arbitrary State names should be accepted");

        const auto UnknownTransition = TSRProfileLoader::Load(9, GetFixturePath("UnknownState.SRProfile"));
        CheckThrows([&] { (void)SurfaceState::TSurfaceStateRegistry({UnknownTransition->GetData()}); },
                    "unknown target State 'wetness'",
                    "transition endpoint missing from loaded Profile set");

        CheckThrows([&] { (void)TSRProfileLoader::Load(10, GetFixturePath("WrongFieldType.SRProfile")); },
                    "states.wetness.decayRate must be a number",
                    "wrong JSON field type");
        CheckThrows([&] { (void)TSRProfileLoader::Load(11, GetFixturePath("MissingParameter.SRProfile")); },
                    "states.wetness.inputFactor is required",
                    "missing parameter");
        CheckThrows([&] { (void)TSRProfileLoader::Load(12, GetFixturePath("LegacyRates.SRProfile")); },
                    "expected version 3 or 4",
                    "legacy rate schema must not be interpreted as normalized factors");
        CheckThrows([&] { (void)TSRProfileLoader::Load(13, GetFixturePath("InvalidTransferFactor.SRProfile")); },
                    "states.wetness.gravityFlowFactor must be finite and in [0, 1]",
                    "unnormalized transfer factor in JSON");
        CheckThrows([&] { (void)TSRProfileLoader::Load(14, GetFixturePath("MixedVersionKeys.SRProfile")); },
                    "states.wetness.cavityTransportRetentionFactor belongs to the other .SRProfile version",
                    "version 4 must reject legacy optional keys instead of silently ignoring them");

        SurfaceState::TSurfaceResponseProfileData InvalidProfile;
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

        SurfaceState::TSurfaceStateRegistry Registry({Profile->GetData(), PartialProfile->GetData()});
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

        SurfaceState::TSurfaceStateRegistry ReorderedRegistry({PartialProfile->GetData(), Profile->GetData()});
        Check(ReorderedRegistry.GetStateId("snow") == Registry.GetStateId("snow") &&
                  ReorderedRegistry.GetStateId("wetness") == Registry.GetStateId("wetness"),
              "Registry IDs should be independent of Profile load order");
    }

    void TestTransferFactorValidation()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;
        for (const auto& [Member, Name] :
             {std::pair{&SurfaceState::TSurfaceStateParameters::SaturationTransferFactor, "saturationSpreadFactor"},
              std::pair{&SurfaceState::TSurfaceStateParameters::GeometryTransferFactor, "gravityFlowFactor"}})
        {
            SurfaceState::TSurfaceResponseProfileData Profile;
            auto&                       Parameters = Profile.States["wetness"];
            for (float Value : {0.0F, 0.5F, 1.0F})
            {
                Parameters.*Member = Value;
                CheckDoesNotThrow([&] { ValidateSurfaceResponseProfileData(Profile); },
                                  std::string(Name) + " valid normalized factor");
            }
            for (float Value :
                 {-0.1F, 1.1F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
            {
                Parameters.*Member = Value;
                CheckThrows([&] { ValidateSurfaceResponseProfileData(Profile); },
                            std::string("states.wetness.") + Name + " must be finite and in [0, 1]",
                            std::string(Name) + " invalid normalized factor");
            }
        }
    }

    void TestGeometryAndInstanceData()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        auto Geometry =
            std::make_shared<TSharedSurfaceGeometryData>(std::vector<SurfaceState::TSurfaceDefinition>{{0, {2, 2}}, {1, {3, 1}}});
        Check(Geometry->GetTexelCount() == 7, "geometry texel count should sum Surface grids");
        Check(!Geometry->GetTexels()[0].IsValid(), "unmapped texels should start invalid");

        Geometry->GetTexels()[0].Surface = 0;
        Geometry->GetTexels()[0].Triangle = 0;
        std::vector<SurfaceState::TSurfaceProfileIndex> ProfileMap(7, SurfaceState::InvalidSurfaceProfileIndex);
        ProfileMap[0] = 5;
        Geometry->SetProfileMap(std::move(ProfileMap));
        SurfaceState::TSurfaceInstanceStateData Instance(3, Geometry, 3);

        Check(Instance.GetID() == 3 && Instance.GetStateCount() == 3, "instance should retain ID and dynamic channels");
        Check(Instance.GetProfileIndex(0) == 5, "texel Profile index should come from shared SurfaceProfileMap");
        Check(Instance.GetStates().size() == 7, "instance should own one state vector per texel");
        for (const SurfaceState::TSurfaceStateValues& State : Instance.GetStates())
        {
            Check(State == SurfaceState::TSurfaceStateValues(3, 0.0F), "all dynamic State channels should start at zero");
        }

        CheckThrows([] { (void)TSharedSurfaceGeometryData({}); }, "at least one Surface", "empty Surface list");
        CheckThrows([] { (void)TSharedSurfaceGeometryData({{SurfaceState::InvalidSurfaceID, {1, 1}}}); },
                    "InvalidSurfaceID is reserved",
                    "reserved Surface sentinel");
        CheckThrows([] { (void)SurfaceState::TSurfaceResolution{0, 8}.GetTexelCount(); }, "greater than zero", "zero resolution");
        CheckThrows([&] { (void)Instance.GetProfileIndex(7); }, "not present", "out-of-range texel profile lookup");
        CheckThrows(
            [&] { Geometry->SetProfileMap({SurfaceState::InvalidSurfaceProfileIndex}); }, "one entry per texel", "wrong map size");

        std::vector<SurfaceState::TSurfaceProfileIndex> NoSimulationMap(7, SurfaceState::InvalidSurfaceProfileIndex);
        Geometry->SetProfileMap(std::move(NoSimulationMap));
        Check(Geometry->GetProfileIndex(0) == SurfaceState::InvalidSurfaceProfileIndex,
              "valid geometry should allow an invalid Profile sentinel to disable simulation");
    }

    void TestContactInputType()
    {
        MDSS::SurfaceState::TSurfaceContactInput Input;
        Input.TargetInstance = 11;
        Input.State = 3;
        Input.Radius = 0.25F;
        Input.Strength = 0.8F;
        Check(Input.TargetInstance == 11 && Input.State == 3, "input should reference instance and registry State ID");
        Check(Input.Falloff == 1.0F, "contact input should have a stable default falloff");
    }

    void TestRuntimePreprocessing()
    {
        using namespace MDSS;
        using namespace MDSS::Asset;
        using namespace MDSS::SurfaceState;

        SurfaceState::TSurfaceMappingData Mapping;
        Mapping.Surfaces.push_back({0, {2, 1}, 0, 2});
        Mapping.Texels.resize(2);
        Mapping.Texels[0].Surface = 0;
        Mapping.Texels[0].Triangle = 4;
        Mapping.Texels[0].Chart = 0;
        Mapping.Texels[0].Position = {1.0F, 2.0F, 3.0F};
        Mapping.Texels[0].Normal = {0.0F, 0.0F, 1.0F};

        const SurfaceState::TSurfaceRuntimeData Data = SurfaceState::TSurfacePreprocessor::Build(Mapping, {7, SurfaceState::InvalidSurfaceProfileIndex}, 8);
        Check(Data.Geometry->GetProfileIndex(0) == 7, "valid texel should retain its Profile index");
        Check(Data.Geometry->GetProfileIndex(1) == SurfaceState::InvalidSurfaceProfileIndex,
              "invalid texel should use reserved Profile sentinel");

        const SurfaceState::TSurfaceRuntimeData Rebuilt = SurfaceState::TSurfacePreprocessor::Build(Mapping, {7, SurfaceState::InvalidSurfaceProfileIndex}, 8);
        Check(Rebuilt.Geometry->GetTexelCount() == Data.Geometry->GetTexelCount(),
              "Runtime preprocessing should be repeatable without a disk cache");
        Check(Rebuilt.Geometry->GetProfileMap() == Data.Geometry->GetProfileMap(),
              "identical Runtime inputs should reproduce the same texel Profile map");
        Check(Rebuilt.Geometry->GetTexels()[0].Position == Data.Geometry->GetTexels()[0].Position,
              "identical Runtime inputs should reproduce geometry values");

        CheckThrows([&] { (void)SurfaceState::TSurfacePreprocessor::Build(Mapping, {7}, 8); },
                    "exactly one entry per mapping texel",
                    "profile map with wrong texel count");
        CheckThrows([&] { (void)SurfaceState::TSurfacePreprocessor::Build(Mapping, {8, SurfaceState::InvalidSurfaceProfileIndex}, 8); },
                    "outside the loaded Profile range",
                    "profile map index outside registered profile range");
    }
} // namespace

int main()
{
    TestProfileAndRegistry();
    TestTransferFactorValidation();
    TestGeometryAndInstanceData();
    TestContactInputType();
    TestRuntimePreprocessing();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " contract test(s) failed.\n";
        return 1;
    }

    std::cout << "All Surface State contract tests passed.\n";
    return 0;
}
