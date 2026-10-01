/** @file SceneResourceTests.cpp
 * @brief Scene Registry replacement, shared Profile GPU table, rollback and input isolation.
 */
#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "AssetManager/Loaders/SceneLoader.h"
#include "DebugUI/DebugUI.h"
#include "Logger/Logger.h"
#include "Renderer/Renderer.h"
#include "SurfaceStateSystem/GPU/SurfaceGPUResources.h"
#include "SurfaceStateSystem/SurfaceStateSystem.h"
#include "VulkanContext/VulkanContext.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <numeric>
#include <stdexcept>
#include <string_view>

namespace MDSS::Tests
{
    void TestSurfaceDebugRendering(const TVulkanContext& Context);
}

namespace
{
    using namespace MDSS;
    using TJson = nlohmann::json;

    void Check(bool Condition, const char* Message)
    {
        if (!Condition) throw std::runtime_error(Message);
    }

    struct TFixtures
    {
        std::filesystem::path Root = std::filesystem::temp_directory_path() /
            ("mdss-scene-resources-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TFixtures()
        {
            std::filesystem::create_directories(Root);
            const char* Triangle = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\nf 1/1/1 2/2/1 3/3/1\n";
            std::ofstream(Root / "A.obj") << Triangle;
            std::ofstream(Root / "B.obj") << Triangle;
            WriteProfile("Wet", {"wetness"}, 0.75F);
            WriteProfile("Mud", {"mud"}, 0.5F);
            WriteProfile("Cached", {"heat"}, 1.0F);
            WriteMap("Wet", {"Wet.SRProfile"}, 0);
            WriteMap("Mud", {"Mud.SRProfile"}, 0);
            WriteMap("WetMud", {"Wet.SRProfile", "Mud.SRProfile"}, 0);
            WriteMap("MudWet", {"Mud.SRProfile", "Wet.SRProfile"}, 1);
            WriteScene("Wet", {{"A.obj", "Wet.SurfaceProfileMap"}}, 128);
            WriteScene("Mud", {{"B.obj", "Mud.SurfaceProfileMap"}}, 128);
            WriteScene("Mixed", {{"A.obj", "WetMud.SurfaceProfileMap"},
                                  {"B.obj", "MudWet.SurfaceProfileMap"},
                                  {"A.obj", "Mud.SurfaceProfileMap"}}, 128);
            WriteScene("Empty", {});
        }
        ~TFixtures()
        {
            std::error_code Error;
            std::filesystem::remove_all(Root, Error);
        }
        void Write(const std::string& File, const TJson& Data) const
        {
            std::ofstream(Root / File) << Data.dump(2);
        }
        void WriteProfile(const std::string& Name, const std::vector<std::string>& States, float InputFactor) const
        {
            TJson Entries = TJson::object();
            for (const auto& State : States)
            {
                Entries[State] = {{"stateCapacity", 1.0}, {"inputFactor", InputFactor},
                    {"saturationSpreadFactor", 0.0}, {"gravityFlowFactor", 0.0}, {"decayRate", 0.0},
                    {"cavityDecayProtectionFactor", 0.0}, {"accumulationFactor", 0.0},
                    {"cavityFillFactor", 0.0}, {"thicknessPerAmount", 0.01}};
            }
            Write(Name + ".SRProfile", {{"type", "SurfaceResponseProfile"}, {"version", 4}, {"name", Name},
                                       {"states", Entries}, {"transitions", TJson::array()}});
        }
        void WriteMap(const std::string& Name, const std::vector<std::string>& Profiles, int Index) const
        {
            Write(Name + ".SurfaceProfileMap", {{"type", "SurfaceProfileMap"}, {"version", 1}, {"profiles", Profiles},
                {"surfaces", TJson::array({{{"surfaceId", 0}, {"profileIndex", Index}}})}});
        }
        void WriteScene(const std::string& Name, const std::vector<std::pair<std::string, std::string>>& Inputs,
                        std::uint32_t Resolution = SurfaceSimulationResolution) const
        {
            TJson Objects = TJson::array();
            for (const auto& [Mesh, Map] : Inputs) Objects.push_back({{"mesh", Mesh}, {"surfaceProfileMap", Map}});
            Write(Name + ".Scene", {{"type", "TScene"}, {"version", 1},
                                   {"simulationResolution", Resolution}, {"objects", Objects}});
        }
    };

    float GetProfileInputFactor(const TSurfaceGPUResourceManager& GPU, const TAssetManager& Assets,
                                const TScene& Scene, TSRProfileAssetHandle Profile, TStateId State)
    {
        const auto Handles = Assets.GetSceneSurfaceProfiles(Scene);
        const auto Found = std::find(Handles.begin(), Handles.end(), Profile);
        Check(Found != Handles.end(), "requested Profile must belong to the Scene");
        const auto* Descriptor = GPU.GetAnyInstanceDescriptors();
        Check(Descriptor != nullptr, "simulated Scene must have descriptors");
        const auto ProfileIndex = static_cast<std::size_t>(Found - Handles.begin());
        TSurfaceGPUProfileParameters Parameters{};
        const auto Offset = GetSurfaceGPUProfileRecordIndex(ProfileIndex, State,
            Assets.GetSurfaceStateRegistry().GetStateCount()) * sizeof(Parameters);
        GPU.GetSceneProfileParametersBuffer().Download(&Parameters, sizeof(Parameters), Offset);
        return Parameters.CapacityInputAndTransfer[1];
    }

    void TestDemoAnimation(const TVulkanContext& Context, const TFixtures& Fixtures)
    {
        Fixtures.Write("Animated.DemoAnim", {
            {"version", 1}, {"durationSeconds", 2.0}, {"loop", true},
            {"tracks", TJson::array({
                {{"target", "object"}, {"property", "position"},
                 {"keys", TJson::array({{{"time", 0.0}, {"value", {0, 0, 0}}},
                                         {{"time", 2.0}, {"value", {2, 0, 0}}}})}},
                {{"target", "object"}, {"property", "rotation"},
                 {"keys", TJson::array({{{"time", 0.0}, {"value", {0, 0, 0, 1}}},
                                         {{"time", 2.0}, {"value", {0, 0, 1, 0}}}})}},
                {{"target", "camera"}, {"property", "target"},
                 {"keys", TJson::array({{{"time", 0.0}, {"value", {0, 0, 0}}},
                                         {{"time", 2.0}, {"value", {2, 0, 0}}}})}}
            })}
        });
        Fixtures.Write("Animated.Scene", {{"type", "TScene"}, {"version", 1},
            {"animation", "Animated.DemoAnim"},
            {"objects", TJson::array({{{"id", "object"}, {"mesh", "A.obj"}}})}});
        TAssetManager Assets(Context);
        TScene Scene = TSceneLoader::Load(Fixtures.Root / "Animated.Scene", Assets);
        Check(Scene.HasDemoAnimation() && !Scene.IsDemoAnimationPlaying(),
              "Scene animation should load paused independently of simulation");
        Scene.PlayDemoAnimation();
        Scene.AdvanceDemoAnimation(1.0F);
        const auto& Transform = Scene.GetStaticMeshInstances()[0].GetTransform();
        Check(std::abs(Transform.Position.x - 1.0F) < 1.0e-5F &&
              std::abs(Transform.RotationDegrees.z - 90.0F) < 1.0e-3F &&
              std::abs(Scene.GetMainCamera().GetTarget().x - 1.0F) < 1.0e-5F,
              "animation should interpolate object and camera tracks at half time");
        Scene.PauseDemoAnimation();
        Scene.AdvanceDemoAnimation(0.5F);
        Check(Scene.GetDemoAnimationTime() == 1.0F,
              "paused animation must not advance its own clock");
        Scene.PlayDemoAnimation();
        Scene.AdvanceDemoAnimation(1.5F);
        Check(std::abs(Scene.GetDemoAnimationTime() - 0.5F) < 1.0e-5F &&
              std::abs(Transform.Position.x - 0.5F) < 1.0e-5F,
              "looping animation should wrap and evaluate the new time");
        Scene.RestartDemoAnimation();
        Check(!Scene.IsDemoAnimationPlaying() && Scene.GetDemoAnimationTime() == 0.0F &&
              Transform.Position.x == 0.0F, "restart should restore the first pose without starting playback");
        TSceneLoader::Save(Scene, Fixtures.Root / "AnimatedSaved.Scene");
        const TScene Saved = TSceneLoader::Load(Fixtures.Root / "AnimatedSaved.Scene", Assets);
        Check(Saved.HasDemoAnimation() && Saved.GetStaticMeshInstances()[0].GetId() == "object",
              "Scene save and load should preserve animation reference and object IDs");

        TScene SurfaceScene = TSceneLoader::Load(Fixtures.Root / "Wet.Scene", Assets);
        Assets.ExchangeSurfaceStateRegistry(Assets.BuildSurfaceStateRegistry(SurfaceScene));
        TSurfaceStateSystem SurfaceStates(Context, Assets, SurfaceScene);
        auto& SurfaceTransform = SurfaceScene.GetStaticMeshInstances()[0].GetTransform();
        const auto& GPU = SurfaceStates.GetGPUResources();
        Check(!GPU.NeedsTransferWeightCacheUpdate(0, SurfaceTransform),
              "new Surface cache should be valid");
        SurfaceTransform.RotationDegrees.x = 35.0F;
        SurfaceTransform.Position.y = 2.0F;
        Check(!GPU.NeedsTransferWeightCacheUpdate(0, SurfaceTransform),
              "rotation and translation should reuse static TransferWeight and world area buffers");
        SurfaceTransform.Scale.x = 2.0F;
        Check(GPU.NeedsTransferWeightCacheUpdate(0, SurfaceTransform),
              "scale changes must still refresh TransferWeight and world area buffers");
        SurfaceTransform.Scale.x = 1.0F;
        SurfaceTransform.RotationDegrees.x = 0.0F;
        const auto WetProfile = Assets.LoadSRProfile(Fixtures.Root / "Wet.SRProfile");
        auto Flow = Assets.GetSRProfile(WetProfile).GetData().States.at("wetness");
        Flow.GeometryTransferFactor = 1.0F;
        const auto WetState = Assets.GetSurfaceStateRegistry().GetStateId("wetness");
        SurfaceStates.SetDebugProfileParameters(WetProfile, WetState, Flow);
        const float FlatBound = SurfaceStates.GetMaximumStableDeltaTime();
        SurfaceTransform.RotationDegrees.x = 90.0F;
        const float TiltedBound = SurfaceStates.GetMaximumStableDeltaTime();
        Check(TiltedBound < FlatBound,
              "Auto timestep bound should reflect rotation even when static TransferWeight cache is reused");

        for (const char* Name : {"Mountain", "Bunny"})
        {
            TScene RealScene = TSceneLoader::Load(
                std::filesystem::path(MDSS_ASSET_DIR) / "Scenes" / (std::string(Name) + ".Scene"), Assets);
            const auto Handle = RealScene.GetStaticMeshInstances().front().GetSurfaceData();
            const auto& Texels = Assets.GetSurfaceData(Handle).GetSharedGeometry()->GetTexels();
            const auto Positive = std::count_if(Texels.begin(), Texels.end(), [](const auto& Texel)
            {
                return Texel.IsValid() && Texel.Geometry.ConcavityWeight > 0.01F;
            });
            Check(Positive > 0, "Mountain and Bunny must contain Macro concavity without normal maps");
            if (std::string_view(Name) == "Mountain")
            {
                Check(RealScene.GetStaticMeshInstances().size() == 4 &&
                      RealScene.GetStaticMeshInstances()[3].GetId() == "mountain_mud_inverted" &&
                      RealScene.GetStaticMeshInstances()[3].GetTransform().Position.z == 1.0F &&
                      RealScene.GetStaticMeshInstances()[3].GetTransform().RotationDegrees.x == 270.0F,
                      "Mountain Scene should have a fixed inverted Mud mountain above the original");
            }
            RealScene.PlayDemoAnimation();
            RealScene.AdvanceDemoAnimation(3.0F);
            const auto& Objects = RealScene.GetStaticMeshInstances();
            Check(std::abs(Objects[0].GetTransform().RotationDegrees.x - Objects[1].GetTransform().RotationDegrees.x) < 1.0e-4F &&
                  std::abs(Objects[1].GetTransform().RotationDegrees.x - Objects[2].GetTransform().RotationDegrees.x) < 1.0e-4F,
                  "matching objects in one Scene must reach the same rotation at the same time");
            if (std::string_view(Name) == "Mountain")
            {
                const glm::vec3 Up = glm::mat3(Objects[0].GetTransform().GetMatrix()) * glm::vec3(0, 1, 0);
                Check(std::abs(Up.z - Objects[0].GetTransform().Scale.y) < 1.0e-4F,
                      "rotating Mountains must stay upright beneath the fixed inverted Mountain");
            }
            RealScene.AdvanceDemoAnimation(3.0F);
            Check(std::abs(Objects[0].GetTransform().RotationDegrees.x - Objects[2].GetTransform().RotationDegrees.x) < 1.0e-4F,
                  "matching objects must remain synchronized through the full turn");
            if (std::string_view(Name) == "Mountain")
                Check(Objects[3].GetTransform().Position.z == 1.0F &&
                      Objects[3].GetTransform().RotationDegrees.x == 270.0F,
                      "the inverted Mud mountain must remain stationary during animation");
        }
    }

    void TestSceneResources(TWindow& Window, const TVulkanContext& Context, const TFixtures& Fixtures)
    {
        TAssetManager Assets(Context);
        (void)Assets.LoadSRProfile(Fixtures.Root / "Cached.SRProfile");
        TScene Scene = TSceneLoader::Load(Fixtures.Root / "Wet.Scene", Assets);
        Assets.ExchangeSurfaceStateRegistry(Assets.BuildSurfaceStateRegistry(Scene));
        TSurfaceStateSystem SurfaceStates(Context, Assets, Scene);
        TRenderer Renderer(Context, Window, Assets, Scene, SurfaceStates);
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == 0 &&
              Renderer.GetDemoSurfaceStateBindings().Mud == InvalidStateId, "Wet Scene demo bindings must resolve optional names.");
        {
            TDebugUI UI(Context, Window, Renderer, Assets);
            Check(UI.IsFixedSimulationTimestep() && !UI.IsAutoSubsteppingEnabled(),
                  "the actual UI must default to Fixed ON and Auto substepping OFF");
            // Keep the hidden test window from reading or overwriting the editor docking layout.
            ImGui::GetIO().IniFilename = nullptr;
            Renderer.SetTexelGridBlockSize(16);
            Renderer.SetTexelAreaReference(1.0F / (128.0F * 128.0F));
            for (auto Mode : {TRenderViewMode::Lit, TRenderViewMode::SurfaceTexelGrid,
                              TRenderViewMode::SurfaceTexelArea,
                              TRenderViewMode::SolverTransferWeight,
                              TRenderViewMode::MesoHeight,
                              TRenderViewMode::MesoOffset,
                              TRenderViewMode::SurfaceAccumulation,
                              TRenderViewMode::SurfaceFinalGeometry})
            {
                Renderer.SetRenderViewMode(Mode);
                Window.PollEvents();
                UI.BeginFrame(Scene);
                Renderer.RenderFrame(Scene, UI, 0.0F);
            }
            Window.PollEvents();
            UI.BeginFrame(Scene);
            Renderer.RenderFrame(Scene, UI, 1.0F / 15.0F);
            Check(Renderer.GetLastSimulationStepCount() == 4 &&
                  std::abs(Renderer.GetSimulatedSeconds() - 1.0 / 15.0) < 1e-6,
                  "the Renderer must run four fixed steps for a 15 FPS frame with the default policy");
            Check(Renderer.InspectTexel(Scene, 0, 0, {0.2F, 0.2F}),
                  "Macro mesh hit must resolve an inspectable texel.");
            const auto SelectedTexel = Renderer.GetInspectedTexel()->Texel;
            for (int Frame = 0; Frame < 4; ++Frame)
            {
                Window.PollEvents();
                UI.BeginFrame(Scene);
                Renderer.RenderFrame(Scene, UI, 0.0F);
            }
            Check(Renderer.GetTexelSnapshot() && Renderer.GetTexelSnapshot()->Selection.Texel == SelectedTexel,
                  "Renderer must publish a completed asynchronous GPU snapshot.");
            Renderer.SetSceneLitHeightDisplayScale(Scene, 2.0F);
            Check(!Renderer.GetTexelSnapshot(), "Changing display scale must invalidate prior samples.");
            for (int Frame = 0; Frame < 4; ++Frame)
            {
                Window.PollEvents();
                UI.BeginFrame(Scene);
                Renderer.RenderFrame(Scene, UI, 0.0F);
            }
            Check(Renderer.GetTexelSnapshot() && Renderer.GetTexelSnapshot()->Values[4].z == 2.0F,
                  "New snapshots must carry the current Lit height display scale.");
            Renderer.SetSimulationResolution(Scene, 256);
            Check(!Renderer.GetInspectedTexel() && !Renderer.GetTexelSnapshot(),
                  "Resolution replacement must clear Inspector selection and pending samples.");
            Check(Renderer.GetTexelGridBlockSize() == 16 &&
                      Renderer.GetTexelAreaReference() == 1.0F / (128.0F * 128.0F),
                  "resolution change must preserve debug grid size and area color reference");
            Renderer.SetSimulationResolution(Scene, 128);
            vkDeviceWaitIdle(Context.GetDevice());
        }
        Check(Assets.GetSurfaceStateRegistry().GetStateCount() == 1 &&
              Assets.GetSurfaceStateRegistry().GetStateName(0) == "wetness", "cached foreign States must be excluded");
        const auto WetHandle = Assets.LoadSRProfile(Fixtures.Root / "Wet.SRProfile");
        auto Parameters = Assets.GetSRProfile(WetHandle).GetData().States.at("wetness");
        Parameters.InputFactor = 2.5F;
        Renderer.SetDebugProfileParameters(WetHandle, 0, Parameters);

        auto SwitchScene = [&](const std::string& Name)
        {
            TScene Loaded = TSceneLoader::Load(Fixtures.Root / (Name + ".Scene"), Assets);
            TScene Previous = Scene;
            Scene = std::move(Loaded);
            try { Renderer.ReloadSceneResources(Scene); }
            catch (...) { Scene = std::move(Previous); throw; }
        };
        TScene Mud = TSceneLoader::Load(Fixtures.Root / "Mud.Scene", Assets);
        Check(Assets.GetSurfaceStateRegistry().GetStateName(0) == "wetness", "loading prospective assets must preserve active IDs");
        SwitchScene("Mud");
        Check(Assets.GetSurfaceStateRegistry().GetStateCount() == 1 &&
              Assets.GetSurfaceStateRegistry().GetStateName(0) == "mud", "Scene switch must replace the Registry");
        Check(Renderer.GetDemoSurfaceStateBindings().Mud == 0 &&
              Renderer.GetDemoSurfaceStateBindings().Wetness == InvalidStateId, "Mud Scene must discard the former Wetness ID.");
        {
            TDebugUI UI(Context, Window, Renderer, Assets);
            ImGui::GetIO().IniFilename = nullptr;
            Renderer.SetRenderViewMode(TRenderViewMode::Lit);
            const auto MudState = Assets.GetSurfaceStateRegistry().GetStateId("mud");
            TSurfaceContactInput Contact;
            Contact.TargetInstance = 0;
            Contact.State = MudState;
            Contact.Radius = 100;
            Contact.Strength = 10;
            Contact.Falloff = 0;
            SurfaceStates.SubmitContact(Contact);
            for (int Frame = 0; Frame < 3; ++Frame)
            {
                auto Effects = Renderer.GetDemoSurfaceEffectSettings();
                Effects.bMudDisplacement = Frame != 1;
                Effects.bEnabled = Frame != 2;
                Renderer.SetDemoSurfaceEffectSettings(Effects);
                Window.PollEvents();
                UI.BeginFrame(Scene);
                Renderer.RenderFrame(Scene, UI, 1.0F/60.0F);
            }
            Renderer.SetDemoSurfaceEffectSettings({});
        }

        SwitchScene("Mixed");
        const auto& Registry = Assets.GetSurfaceStateRegistry();
        const TStateId WetState = Registry.GetStateId("wetness");
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == WetState &&
              Renderer.GetDemoSurfaceStateBindings().Mud == Registry.GetStateId("mud"), "Scene reload must resolve changed demo IDs.");
        Check(Registry.GetStateCount() == 2, "mixed Scene must use exactly its two States");
        const auto& GPU = Renderer.GetSurfaceGPUResources();
        Check(GPU.GetSceneProfileCount() == 2 && GPU.GetSharedSurfaceDataCount() == 3,
              "different Runtime combinations must share one deduplicated Profile table");
        const auto* A = GPU.GetInstanceDescriptors(0);
        const auto* B = GPU.GetInstanceDescriptors(1);
        const auto* C = GPU.GetInstanceDescriptors(2);
        for (auto Binding : {TSurfaceGPUDescriptorBinding::ProfileParameters, TSurfaceGPUDescriptorBinding::ProfileSupported})
        {
            Check(A->GetBoundBufferHandle(Binding, true) == B->GetBoundBufferHandle(Binding, false) &&
                  A->GetBoundBufferHandle(Binding, true) == C->GetBoundBufferHandle(Binding, true),
                  "all Meshes must bind the same Profile GPU buffers");
        }
        Check(A->GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::CurrentState, true) !=
              B->GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::CurrentState, true), "instance State must stay independent");
        Check(std::abs(GetProfileInputFactor(GPU, Assets, Scene, WetHandle, WetState) - 0.75F) < 1e-6F,
              "Scene switch must discard old numeric-ID overrides");
        Renderer.SetDebugStateChannel(WetState);
        Parameters.InputFactor = 1.25F;
        Renderer.SetDebugProfileParameters(WetHandle, WetState, Parameters);
        Check(std::abs(GetProfileInputFactor(GPU, Assets, Scene, WetHandle, WetState) - 1.25F) < 1e-6F,
              "Profile update must target the single shared table");

        {
            TSurfaceStateSystem System(Context, Assets, Scene);
            System.ResetState();
            for (TSurfaceInstanceID Target : {0U, 1U, 2U})
            {
                TSurfaceContactInput Contact;
                Contact.TargetInstance = Target;
                Contact.State = WetState;
                Contact.Radius = 100.0F;
                Contact.Strength = 1.0F;
                Contact.Falloff = 0.0F;
                System.SubmitContact(Contact);
            }
            const VkCommandBuffer Command = Context.GetCommands().BeginSingleTime();
            System.RecordStep(Command, 1.0F / 60.0F);
            Context.GetCommands().EndSingleTime(Command, Context.GetQueues().GetGraphics());
            for (std::size_t Index = 0; Index < 3; ++Index)
            {
                const auto& Resources = System.GetGPUResources();
                std::vector<float> Values(Resources.GetInstanceTexelCount(Index) * Registry.GetStateCount());
                Resources.GetInstanceCurrentStateBuffer(Index).Download(Values.data(), Values.size() * sizeof(float));
                const float Sum = std::accumulate(Values.begin(), Values.end(), 0.0F);
                Check(Index == 2 ? Sum == 0.0F : Sum > 0.0F,
                      "local Profile order remapping must inject Wetness into both Wet Meshes and skip the Mud Mesh");
                if (Index < 2)
                {
                    const auto& MeshInstance = Scene.GetStaticMeshInstances()[Index];
                    const auto& Geometry = *Assets.GetSurfaceData(MeshInstance.GetSurfaceData()).GetSharedGeometry();
                    const auto Areas = BuildSurfaceGPUWorldTexelAreas(Geometry, MeshInstance.GetTransform().GetMatrix());
                    for (std::size_t Texel = 0; Texel < Areas.size(); ++Texel)
                    {
                        const float Expected = 0.75F * Areas[Texel] / SurfaceStateReferenceArea;
                        Check(std::abs(Values[Texel * Registry.GetStateCount() + WetState] - Expected) < 1e-5F,
                              "contact input must scale per texel world area");
                    }
                }
            }
        }
        Renderer.SetSimulationResolution(Scene, 256);
        Check(std::abs(GetProfileInputFactor(Renderer.GetSurfaceGPUResources(), Assets, Scene, WetHandle, WetState) - 1.25F) < 1e-6F,
              "resolution change must preserve current Scene tuning");

        {
            // A vertical unit triangle must reduce dt with the new rate, including runtime overrides.
            auto& Transform = Scene.GetStaticMeshInstances()[0].GetTransform();
            const auto PreviousRotation = Transform.RotationDegrees;
            Transform.RotationDegrees = {90, 0, 0};
            {
                TSurfaceStateSystem System(Context, Assets, Scene);
                auto Flow = Assets.GetSRProfile(WetHandle).GetData().States.at("wetness");
                Flow.GeometryTransferFactor = 0.5F;
                System.SetDebugProfileParameters(WetHandle, WetState, Flow);
                const float HalfFactorStep = System.GetMaximumStableDeltaTime();
                Check(HalfFactorStep > 0.012F && HalfFactorStep < 0.016F,
                      "calibrated Geometry must lower the transport step bound on a vertical Medium surface");
                Flow.GeometryTransferFactor = 1.0F;
                System.SetDebugProfileParameters(WetHandle, WetState, Flow);
                const float FullFactorStep = System.GetMaximumStableDeltaTime();
                Check(std::abs(FullFactorStep * 2 - HalfFactorStep) < 1e-6F,
                      "doubling Geometry factor must halve its safe step bound");
                System.SetDebugGeometryDriveEnabled(false);
                Check(std::abs(System.GetMaximumStableDeltaTime() - 1.0F / 60) < 1e-7F,
                      "disabling Geometry must remove its calibrated step restriction");
            }
            Transform.RotationDegrees = PreviousRotation;
        }

        // Force a resource-size failure after installing a valid new Registry, before any State allocation.
        VkPhysicalDeviceProperties Limits{};
        vkGetPhysicalDeviceProperties(Context.GetPhysicalDevice(), &Limits);
        const std::size_t Count = Limits.limits.maxStorageBufferRange / (256U * 256U * SurfaceNeighborCount * sizeof(float)) + 1U;
        std::vector<std::string> OversizedStates;
        for (std::size_t Index = 0; Index < Count; ++Index) OversizedStates.push_back("new_" + std::to_string(Index));
        Fixtures.WriteProfile("Oversized", OversizedStates, 1.0F);
        Fixtures.WriteMap("Oversized", {"Oversized.SRProfile"}, 0);
        Fixtures.WriteScene("Oversized", {{"A.obj", "Oversized.SurfaceProfileMap"}});
        const VkBuffer PreviousState = Renderer.GetSurfaceGPUResources().GetInstanceDescriptors(0)->
            GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::CurrentState, true);
        bool bFailed = false;
        try { SwitchScene("Oversized"); }
        catch (const std::length_error&) { bFailed = true; }
        Check(bFailed, "oversized Scene must fail GPU resource construction");
        Check(Assets.GetSurfaceStateRegistry().GetStateCount() == 2 &&
              Assets.GetSurfaceStateRegistry().GetStateId("wetness") == WetState,
              "failed resource rebuild must restore the previous Registry");
        Check(Renderer.GetSurfaceGPUResources().GetInstanceDescriptors(0)->
            GetBoundBufferHandle(TSurfaceGPUDescriptorBinding::CurrentState, true) == PreviousState,
            "failed Scene switch must retain previous GPU resources");
        Check(std::abs(GetProfileInputFactor(Renderer.GetSurfaceGPUResources(), Assets, Scene, WetHandle, WetState) - 1.25F) < 1e-6F,
              "failed switch must retain previous tuning");
        SwitchScene("Empty");
        Check(Assets.GetSurfaceStateRegistry().GetStateCount() == 0 &&
              Renderer.GetSurfaceGPUResources().GetManagedInstanceCount() == 0, "empty Scene must have no State channels or resources");
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == InvalidStateId &&
              Renderer.GetDemoSurfaceStateBindings().Mud == InvalidStateId, "Empty Registry must resolve both demo States as absent.");
        SwitchScene("Wet");
        Check(Assets.GetSurfaceStateRegistry().GetStateCount() == 1 &&
              Assets.GetSurfaceStateRegistry().GetStateName(0) == "wetness", "switching back must exclude cached failed Scene States");
    }
}

int main(int Argc, char* Argv[])
{
    std::unique_ptr<MDSS::TWindow> Window;
    std::unique_ptr<MDSS::TVulkanContext> Context;
    try
    {
        Window = std::make_unique<MDSS::TWindow>(320, 240, "MDSS Scene resource tests");
        glfwHideWindow(Window->GetNativeHandle());
        Context = std::make_unique<MDSS::TVulkanContext>(*Window);
    }
    catch (const std::exception& Error)
    {
        std::cout << "SKIP: native window/Vulkan context unavailable: " << Error.what() << '\n';
        return 77;
    }
    try
    {
        TFixtures Fixtures;
        TestDemoAnimation(*Context, Fixtures);
        const bool DemoOnly = Argc == 2 && std::string_view(Argv[1]) == "--demo-animation";
        if (!DemoOnly)
        {
            MDSS::Tests::TestSurfaceDebugRendering(*Context);
            TestSceneResources(*Window, *Context, Fixtures);
        }
        Context.reset();
        Window.reset();
        for (const auto& Entry : MDSS::TLogger::GetEntries())
        {
            Check(Entry.Module != "Vulkan Validation" || Entry.Level != MDSS::TLogLevel::Error, "Vulkan validation must have no errors");
        }
        std::cout << "Scene resource checks passed.\n";
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "Scene resource check failed: " << Error.what() << '\n';
        return 1;
    }
}
