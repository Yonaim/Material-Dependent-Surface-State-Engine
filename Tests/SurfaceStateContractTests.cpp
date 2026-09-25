/**
 * @file SurfaceStateContractTests.cpp
 * @brief Surface State 자료형·profile loader·geometry 계약 검증.
 */

#include "AssetManager/Loaders/SRProfileLoader.h"
#include "AssetManager/Assets/SRProfileAsset.h"
#include "SurfaceStateSystem/Geometry/SharedSurfaceGeometryData.h"
#include "SurfaceStateSystem/State/SurfaceInput.h"
#include "SurfaceStateSystem/State/SurfaceInstanceStateData.h"
#include "SurfaceStateSystem/Types/SurfaceMappingTypes.h"
#include "SurfaceStateSystem/Types/SurfaceStateTypes.h"

#include <array>
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

    void TestStateChannelContract()
    {
        using namespace MDSS;

        constexpr std::array ExpectedNames = {"wetness", "heat", "burn", "mud"};
        Check(SurfaceStateChannelCount == ExpectedNames.size(), "state channel count must remain four");

        for (std::size_t Index = 0; Index < ExpectedNames.size(); ++Index)
        {
            const SurfaceStateChannel Channel = static_cast<SurfaceStateChannel>(Index);
            Check(GetSurfaceStateChannelName(Channel) == ExpectedNames[Index], "channel order/name mismatch");
            Check(ParseSurfaceStateChannel(ExpectedNames[Index]) == Channel, "channel name should parse to its enum");
            Check(GetSurfaceStateChannelIndex(Channel) == Index, "channel enum index mismatch");
        }

        CheckThrows([] { (void)MDSS::ParseSurfaceStateChannel("snow"); }, "Unknown", "unknown state channel");
        CheckThrows(
            [] { (void)MDSS::GetSurfaceStateChannelName(MDSS::SurfaceStateChannel::Count); }, "valid", "Count channel");
    }

    void TestProfileValidationAndLoading()
    {
        using namespace MDSS;

        std::unique_ptr<SRProfileAsset> Profile = SRProfileLoader::Load(7, GetFixturePath("Valid.SRProfile"));
        Check(Profile->GetName() == "TestProfile", "profile asset name should come from its file data");
        Check(Profile->GetID() == 7, "profile asset ID should be preserved");
        Check(Profile->GetData().States[GetSurfaceStateChannelIndex(SurfaceStateChannel::Wetness)].StateCapacity ==
                  2.0F,
              "state parameters should use the fixed channel order");
        Check(Profile->GetData().Transitions.size() == 1, "profile should load its transition table");
        Check(Profile->GetData().Transitions[0].Source == SurfaceStateChannel::Heat &&
                  Profile->GetData().Transitions[0].Target == SurfaceStateChannel::Burn,
              "Heat to Burn transition should load");

        CheckThrows([&] { (void)SRProfileLoader::Load(8, GetFixturePath("MissingState.SRProfile")); },
                    "define exactly wetness",
                    "profile with a missing channel");
        CheckThrows([&] { (void)SRProfileLoader::Load(9, GetFixturePath("UnknownState.SRProfile")); },
                    "transitions[0].source has unknown channel 'snow'",
                    "profile with an unknown State name");
        CheckThrows([&] { (void)SRProfileLoader::Load(10, GetFixturePath("WrongFieldType.SRProfile")); },
                    "states.wetness.decayRate must be a number",
                    "profile with a wrong JSON field type");
        CheckThrows([&] { (void)SRProfileLoader::Load(11, GetFixturePath("MissingParameter.SRProfile")); },
                    "states.wetness.inputFactor is required",
                    "profile with a missing required parameter");

        SurfaceResponseProfileData InvalidProfile;
        InvalidProfile.States[0].StateCapacity = 0.0F;
        CheckThrows([&] { ValidateSurfaceResponseProfileData(InvalidProfile); },
                    "states.wetness.stateCapacity",
                    "zero state capacity");

        InvalidProfile = {};
        InvalidProfile.States[0].CavityRetentionFactor = 1.1F;
        CheckThrows(
            [&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "[0, 1]", "out-of-range cavity factor");

        InvalidProfile = {};
        InvalidProfile.States[0].DecayRate = -0.1F;
        CheckThrows(
            [&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "states.wetness.decayRate", "negative rate");

        InvalidProfile = {};
        InvalidProfile.States[0].CavityRetentionFactor = 0.0F;
        InvalidProfile.States[0].CavityFillFactor = 1.0F;
        CheckDoesNotThrow([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "Factor endpoints 0 and 1");
        InvalidProfile.States[0].CavityRetentionFactor = 1.0F;
        InvalidProfile.States[0].CavityFillFactor = 0.0F;
        CheckDoesNotThrow([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "Factor endpoints 1 and 0");

        InvalidProfile = {};
        InvalidProfile.States[0].CavityFillFactor = 1.1F;
        CheckThrows(
            [&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "[0, 1]", "out-of-range cavity fill factor");

        InvalidProfile = {};
        InvalidProfile.Transitions.push_back({SurfaceStateChannel::Heat, SurfaceStateChannel::Heat, 0.5F, 1.0F});
        CheckThrows([&] { ValidateSurfaceResponseProfileData(InvalidProfile); }, "different", "self transition");
    }

    void TestGeometryAndInstanceDataContract()
    {
        using namespace MDSS;

        auto Geometry = std::make_shared<SharedSurfaceGeometryData>(std::vector<SurfaceDefinition>{
            {0, {2, 2}},
            {1, {3, 1}},
        });

        Check(Geometry->GetSurfaces().size() == 2, "geometry should retain per-Surface ranges");
        Check(Geometry->GetSurfaces()[0].FirstTexel == 0 && Geometry->GetSurfaces()[0].TexelCount == 4,
              "first Surface range should start at zero");
        Check(Geometry->GetSurfaces()[1].FirstTexel == 4 && Geometry->GetSurfaces()[1].TexelCount == 3,
              "Surface ranges should form a contiguous mesh-local texel index space");
        Check(Geometry->GetTexelCount() == 7, "geometry texel count should equal the sum of Surface resolutions");
        Check(!Geometry->GetTexels()[0].IsValid(), "unmapped CPU texels should start invalid");
        Check(Geometry->GetTexels()[0].NeighborIndices[0] == InvalidTexelIndex,
              "unmapped texel neighbor slots should start at the invalid sentinel");

        SurfaceInstanceStateData Instance(3, Geometry, {5, 9});
        Check(Instance.GetID() == 3, "Surface instance ID should be retained");
        Check(Instance.GetProfileIndex(0) == 5 && Instance.GetProfileIndex(1) == 9,
              "each Surface should resolve to exactly one SRProfile index");
        Check(Instance.GetStates().size() == Geometry->GetTexelCount(), "instance state count should match geometry");
        for (const SurfaceStateValues& State : Instance.GetStates())
        {
            Check(State == SurfaceStateValues{}, "new instance state should initialize every channel to zero");
        }

        CheckThrows(
            [] { (void)SurfaceResolution{0, 8}.GetTexelCount(); }, "greater than zero", "zero Surface resolution");
        CheckThrows([] { (void)SharedSurfaceGeometryData({}); }, "at least one Surface", "empty Surface list");
        CheckThrows([] { (void)SharedSurfaceGeometryData({{InvalidSurfaceID, {1, 1}}}); },
                    "InvalidSurfaceID is reserved",
                    "reserved Surface ID sentinel collision");
        CheckThrows([] { (void)SharedSurfaceGeometryData({{1, {1, 1}}}); }, "dense", "non-dense Surface IDs");
        CheckThrows([&] { (void)SurfaceInstanceStateData(4, Geometry, {InvalidSurfaceProfileIndex, 2}); },
                    "reserved",
                    "invalid Profile index");
        CheckThrows([&] { (void)SurfaceInstanceStateData(5, Geometry, {5}); },
                    "exactly one SRProfile index",
                    "Surface and Profile index count mismatch");
        CheckThrows([&] { (void)Instance.GetProfileIndex(2); }, "not present", "unknown Surface ID");
    }

    void TestContactInputType()
    {
        MDSS::SurfaceContactInput Input;
        Input.TargetInstance = 11;
        Input.StateChannel = MDSS::SurfaceStateChannel::Mud;
        Input.Radius = 0.25F;
        Input.Strength = 0.8F;

        Check(Input.TargetInstance == 11, "contact input should target an instance");
        Check(Input.StateChannel == MDSS::SurfaceStateChannel::Mud,
              "contact input should name one of the state channels");
        Check(Input.Falloff == 1.0F, "contact input should have a stable default falloff");
    }
} // namespace

int main()
{
    TestStateChannelContract();
    TestProfileValidationAndLoading();
    TestGeometryAndInstanceDataContract();
    TestContactInputType();

    if (FailureCount != 0)
    {
        std::cerr << FailureCount << " contract test(s) failed.\n";
        return 1;
    }

    std::cout << "All Surface State contract tests passed.\n";
    return 0;
}
