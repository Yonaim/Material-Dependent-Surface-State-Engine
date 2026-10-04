/**
 * @file SceneResourceTests.cpp
 * @brief Scene 교체, 공유 Profile GPU table, rollback과 입력 격리를 검증한다.
 */
#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "DebugUI/DebugUI.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "Logger/Logger.h"
#include "Rendering/Renderer.h"
#include "Scene/SceneLoader.h"
#include "SurfaceState/GPU/SurfaceGPUResources.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"
#include "SurfaceState/SurfaceStateSystem.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
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
    using namespace MDSS::GPU;
    using namespace MDSS::Rendering;
    using namespace MDSS::Asset;
    using namespace MDSS::SurfaceState;
    void TestSurfaceDebugRendering(const GPU::TVulkanContext& Context);
    void TestOverlaySideCompaction(const GPU::TVulkanContext& Context);
    void TestCavityFillDisplayBound(const GPU::TVulkanContext& Context);
}

namespace
{
    using namespace MDSS;
    using namespace MDSS::GPU;
    using namespace MDSS::Rendering;
    using namespace MDSS::Asset;
    using namespace MDSS::SurfaceState;
    using TJson = nlohmann::json;

    void Check(bool Condition, const char* Message)
    {
        if (!Condition)
            throw std::runtime_error(Message);
    }

    struct TFixtures
    {
        std::filesystem::path Root =
            std::filesystem::temp_directory_path() /
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
            WriteScene("Mixed",
                       {{"A.obj", "WetMud.SurfaceProfileMap"},
                        {"B.obj", "MudWet.SurfaceProfileMap"},
                        {"A.obj", "Mud.SurfaceProfileMap"}},
                       128);
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
                Entries[State] = {{"stateCapacity", 1.0},
                                  {"inputFactor", InputFactor},
                                  {"saturationSpreadFactor", 0.0},
                                  {"gravityFlowFactor", 0.0},
                                  {"decayRate", 0.0},
                                  {"cavityDecayProtectionFactor", 0.0},
                                  {"accumulationFactor", 0.0},
                                  {"cavityFillFactor", 0.0},
                                  {"thicknessPerAmount", 0.01}};
            }
            Write(Name + ".SRProfile",
                  {{"type", "SurfaceResponseProfile"},
                   {"version", 4},
                   {"name", Name},
                   {"states", Entries},
                   {"transitions", TJson::array()}});
        }
        void WriteMap(const std::string& Name, const std::vector<std::string>& Profiles, int Index) const
        {
            Write(Name + ".SurfaceProfileMap",
                  {{"type", "SurfaceProfileMap"},
                   {"version", 1},
                   {"profiles", Profiles},
                   {"surfaces", TJson::array({{{"surfaceId", 0}, {"profileIndex", Index}}})}});
        }
        void WriteScene(const std::string&                                      Name,
                        const std::vector<std::pair<std::string, std::string>>& Inputs,
                        std::uint32_t Resolution = SurfaceState::SurfaceSimulationResolution) const
        {
            TJson Objects = TJson::array();
            for (const auto& [Mesh, Map] : Inputs)
                Objects.push_back({{"mesh", Mesh}, {"surfaceProfileMap", Map}});
            Write(Name + ".Scene",
                  {{"type", "TScene"}, {"version", 1}, {"simulationResolution", Resolution}, {"objects", Objects}});
        }
    };

    float GetProfileInputFactor(const SurfaceState::TSurfaceGPUResourceManager& GPU,
                                const SurfaceState::TSurfaceDataManager&        SurfaceData,
                                const TScene&                                   Scene,
                                Asset::TSRProfileAssetHandle                    Profile,
                                SurfaceState::TStateId                          State)
    {
        const auto Handles = SurfaceData.GetSceneSurfaceProfiles(Scene);
        const auto Found = std::find(Handles.begin(), Handles.end(), Profile);
        Check(Found != Handles.end(), "requested Profile must belong to the Scene");
        const auto* Descriptor = GPU.GetAnyInstanceDescriptors();
        Check(Descriptor != nullptr, "simulated Scene must have descriptors");
        const auto                                 ProfileIndex = static_cast<std::size_t>(Found - Handles.begin());
        SurfaceState::TSurfaceGPUProfileParameters Parameters{};
        const auto                                 Offset = GetSurfaceGPUProfileRecordIndex(
                                ProfileIndex, State, SurfaceData.GetSurfaceStateRegistry().GetStateCount()) *
                            sizeof(Parameters);
        GPU.GetSceneProfileParametersBuffer().Download(&Parameters, sizeof(Parameters), Offset);
        return Parameters.CapacityInputAndTransfer[1];
    }

    void TestDemoAnimation(const GPU::TVulkanContext& Context, const TFixtures& Fixtures)
    {
        Fixtures.Write(
            "Animated.DemoAnim",
            {{"version", 1},
             {"durationSeconds", 2.0},
             {"loop", true},
             {"tracks",
              TJson::array(
                  {{{"target", "object"},
                    {"property", "position"},
                    {"keys",
                     TJson::array({{{"time", 0.0}, {"value", {0, 0, 0}}}, {{"time", 2.0}, {"value", {2, 0, 0}}}})}},
                   {{"target", "object"},
                    {"property", "rotation"},
                    {"keys",
                     TJson::array(
                         {{{"time", 0.0}, {"value", {0, 0, 0, 1}}}, {{"time", 2.0}, {"value", {0, 0, 1, 0}}}})}},
                   {{"target", "camera"},
                    {"property", "target"},
                    {"keys",
                     TJson::array(
                         {{{"time", 0.0}, {"value", {0, 0, 0}}}, {{"time", 2.0}, {"value", {2, 0, 0}}}})}}})}});
        Fixtures.Write("Animated.Scene",
                       {{"type", "TScene"},
                        {"version", 1},
                        {"animation", "Animated.DemoAnim"},
                        {"objects", TJson::array({{{"id", "object"}, {"mesh", "A.obj"}}})}});
        Asset::TAssetManager              Assets(Context);
        SurfaceState::TSurfaceDataManager SurfaceData(Assets);
        TScene Scene = TSceneLoader::Load(Fixtures.Root / "Animated.Scene", Assets, SurfaceData);
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
        Check(Scene.GetDemoAnimationTime() == 1.0F, "paused animation must not advance its own clock");
        Scene.PlayDemoAnimation();
        Scene.AdvanceDemoAnimation(1.5F);
        Check(std::abs(Scene.GetDemoAnimationTime() - 0.5F) < 1.0e-5F &&
                  std::abs(Transform.Position.x - 0.5F) < 1.0e-5F,
              "looping animation should wrap and evaluate the new time");
        Scene.RestartDemoAnimation();
        Check(!Scene.IsDemoAnimationPlaying() && Scene.GetDemoAnimationTime() == 0.0F && Transform.Position.x == 0.0F,
              "restart should restore the first pose without starting playback");
        TSceneLoader::Save(Scene, Fixtures.Root / "AnimatedSaved.Scene");
        const TScene Saved = TSceneLoader::Load(Fixtures.Root / "AnimatedSaved.Scene", Assets, SurfaceData);
        Check(Saved.HasDemoAnimation() && Saved.GetStaticMeshInstances()[0].GetId() == "object",
              "Scene save and load should preserve animation reference and object IDs");

        TScene SurfaceScene = TSceneLoader::Load(Fixtures.Root / "Wet.Scene", Assets, SurfaceData);
        SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(SurfaceScene));
        SurfaceState::TSurfaceStateSystem SurfaceStates(Context, Assets, SurfaceData, SurfaceScene);
        auto&                             SurfaceTransform = SurfaceScene.GetStaticMeshInstances()[0].GetTransform();
        const auto&                       GPU = SurfaceStates.GetGPUResources();
        Check(!GPU.NeedsTransferWeightCacheUpdate(0, SurfaceTransform), "new Surface cache should be valid");
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
        auto       Flow = Assets.GetSRProfile(WetProfile).GetData().States.at("wetness");
        Flow.GeometryTransferFactor = 1.0F;
        const auto WetState = SurfaceData.GetSurfaceStateRegistry().GetStateId("wetness");
        SurfaceStates.SetDebugProfileParameters(WetProfile, WetState, Flow);
        const float FlatBound = SurfaceStates.GetMaximumStableDeltaTime();
        SurfaceTransform.RotationDegrees.x = 90.0F;
        const float TiltedBound = SurfaceStates.GetMaximumStableDeltaTime();
        Check(TiltedBound < FlatBound,
              "Auto timestep bound should reflect rotation even when static TransferWeight cache is reused");

        for (const char* Name : {"Mountain", "Bunny"})
        {
            TScene RealScene = TSceneLoader::Load(
                std::filesystem::path(MDSS_ASSET_DIR) / "Scenes" / (std::string(Name) + ".Scene"), Assets, SurfaceData);
            const auto  Handle = RealScene.GetStaticMeshInstances().front().GetSurfaceData();
            const auto& Texels = SurfaceData.GetSurfaceData(Handle).GetSharedGeometry()->GetTexels();
            const auto  Positive = std::count_if(Texels.begin(),
                                                Texels.end(),
                                                [](const auto& Texel)
                                                { return Texel.IsValid() && Texel.Geometry.ConcavityWeight > 0.01F; });
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
            Check(std::abs(Objects[0].GetTransform().RotationDegrees.x - Objects[1].GetTransform().RotationDegrees.x) <
                          1.0e-4F &&
                      std::abs(Objects[1].GetTransform().RotationDegrees.x -
                               Objects[2].GetTransform().RotationDegrees.x) < 1.0e-4F,
                  "matching objects in one Scene must reach the same rotation at the same time");
            if (std::string_view(Name) == "Mountain")
            {
                const glm::vec3 Up = glm::mat3(Objects[0].GetTransform().GetMatrix()) * glm::vec3(0, 1, 0);
                Check(std::abs(Up.z - Objects[0].GetTransform().Scale.y) < 1.0e-4F,
                      "rotating Mountains must stay upright beneath the fixed inverted Mountain");
            }
            RealScene.AdvanceDemoAnimation(3.0F);
            Check(std::abs(Objects[0].GetTransform().RotationDegrees.x - Objects[2].GetTransform().RotationDegrees.x) <
                      1.0e-4F,
                  "matching objects must remain synchronized through the full turn");
            if (std::string_view(Name) == "Mountain")
                Check(Objects[3].GetTransform().Position.z == 1.0F &&
                          Objects[3].GetTransform().RotationDegrees.x == 270.0F,
                      "the inverted Mud mountain must remain stationary during animation");
        }
        constexpr std::array<std::pair<std::string_view, std::string_view>, 4> Effects{
            {{"Wetness", "wetness"}, {"WaterFilm", "waterfilm"}, {"Mud", "mud"}, {"Lava", "lava"}}};
        for (const std::string_view Shape : {"Cube", "Bunny", "Mountain"})
        {
            glm::vec3 ReferencePosition(0.0F);
            glm::vec3 ReferenceRotation(0.0F);
            glm::vec3 ReferenceScale(0.0F);
            glm::vec3 ReferenceAnimatedRotation(0.0F);
            bool      bFirstEffect = true;
            for (const auto& [Effect, State] : Effects)
            {
                const std::string Stem = std::string(Shape) + "_" + std::string(Effect);
                TScene            Isolated = TSceneLoader::Load(
                    std::filesystem::path(MDSS_ASSET_DIR) / "Scenes" / (Stem + ".Scene"), Assets, SurfaceData);
                Check(Isolated.GetStaticMeshInstances().size() == 1 && Isolated.GetInitialContacts().size() == 1 &&
                          Isolated.HasDemoAnimation() && Isolated.GetSimulationResolution() == 256,
                      "isolated Scene must have one object, one contact, one animation and fixed resolution");
                const auto& Object = Isolated.GetStaticMeshInstances()[0];
                const auto& Contact = Isolated.GetInitialContacts()[0];
                Check(Contact.Target == Object.GetId() && Contact.State == State && Contact.Radius == 0.42F &&
                          Contact.Strength == 1.5F && Contact.Falloff == 1.0F,
                      "isolated Scene contact must target its only effect with common input parameters");
                const auto Registry = SurfaceData.BuildSurfaceStateRegistry(Isolated);
                Check(Registry.GetStateCount() == 1 && Registry.GetStateName(0) == State,
                      "isolated Scene must expose exactly its selected Surface State");
                const auto& Transform = Object.GetTransform();
                if (bFirstEffect)
                {
                    ReferencePosition = Transform.Position;
                    ReferenceRotation = Transform.RotationDegrees;
                    ReferenceScale = Transform.Scale;
                }
                else
                {
                    Check(glm::length(Transform.Position - ReferencePosition) < 1.0e-5F &&
                              glm::length(Transform.RotationDegrees - ReferenceRotation) < 1.0e-5F &&
                              glm::length(Transform.Scale - ReferenceScale) < 1.0e-5F,
                          "all effects of one shape must start with the same transform");
                }
                const auto& Texels =
                    SurfaceData.GetSurfaceData(Object.GetSurfaceData()).GetSharedGeometry()->GetTexels();
                const glm::mat4 Model = Transform.GetMatrix();
                Check(std::any_of(Texels.begin(),
                                  Texels.end(),
                                  [&](const auto& Texel)
                                  {
                                      return Texel.IsValid() &&
                                             glm::length(glm::vec3(Model * glm::vec4(Texel.Position, 1.0F)) -
                                                         Contact.WorldPosition) < Contact.Radius;
                                  }),
                      "isolated Scene initial contact must reach valid surface texels");
                Isolated.PlayDemoAnimation();
                Isolated.AdvanceDemoAnimation(3.0F);
                const glm::vec3 AnimatedRotation = Object.GetTransform().RotationDegrees;
                if (bFirstEffect)
                {
                    Check(glm::length(AnimatedRotation - ReferenceRotation) > 1.0F,
                          "isolated Scene animation must move its object");
                    ReferenceAnimatedRotation = AnimatedRotation;
                    bFirstEffect = false;
                }
                else
                    Check(glm::length(AnimatedRotation - ReferenceAnimatedRotation) < 1.0e-3F,
                          "all effects of one shape must use the same animation phase");
            }
        }
        TScene LavaScene = TSceneLoader::Load(
            std::filesystem::path(MDSS_ASSET_DIR) / "Scenes/Mountain_Lava.Scene", Assets, SurfaceData);
        Check(LavaScene.GetStaticMeshInstances().size() == 1 &&
                  LavaScene.GetStaticMeshInstances()[0].GetId() == "mountain_lava" &&
                  LavaScene.GetInitialContacts().size() == 1 && LavaScene.GetInitialContacts()[0].State == "lava",
              "Lava Scene must load its Mountain and initial Lava input");
        const auto& LavaObject = LavaScene.GetStaticMeshInstances()[0];
        const auto& LavaContact = LavaScene.GetInitialContacts()[0];
        const auto& LavaTexels =
            SurfaceData.GetSurfaceData(LavaObject.GetSurfaceData()).GetSharedGeometry()->GetTexels();
        const glm::mat4 LavaModel = LavaObject.GetTransform().GetMatrix();
        const auto      ContactedCavityCount =
            std::count_if(LavaTexels.begin(),
                          LavaTexels.end(),
                          [&](const auto& Texel)
                          {
                              return Texel.IsValid() && Texel.Geometry.ConcavityWeight > 0.1F &&
                                     glm::length(glm::vec3(LavaModel * glm::vec4(Texel.Position, 1.0F)) -
                                                 LavaContact.WorldPosition) < LavaContact.Radius;
                          });
        Check(ContactedCavityCount > 0, "Lava initial contact must reach concave Mountain texels");
        const auto LavaProfile =
            Assets.LoadSRProfile(std::filesystem::path(MDSS_ASSET_DIR) / "SurfaceProfiles/DemoLava.SRProfile");
        const auto& Lava = Assets.GetSRProfile(LavaProfile).GetData().States.at("lava");
        Check(Lava.GeometryTransferFactor < 0.01F && Lava.SaturationTransferFactor < 0.01F &&
                  Lava.CavityTransportRetentionFactor > 0.95F && Lava.DecayRate == 0.0F,
              "Lava should move slowly and resist leaving cavities without evaporating");
        const auto LavaRegistry = SurfaceData.BuildSurfaceStateRegistry(LavaScene);
        Check(ResolveDemoSurfaceStates(LavaRegistry).Lava == LavaRegistry.GetStateId("lava"),
              "Lava rendering channel must resolve from the loaded profile");
        LavaScene.PlayDemoAnimation();
        LavaScene.AdvanceDemoAnimation(9.0F);
        const glm::vec3 LavaUp =
            glm::mat3(LavaScene.GetStaticMeshInstances()[0].GetTransform().GetMatrix()) * glm::vec3(0, 1, 0);
        Check(LavaUp.z < 0.0F, "Lava Mountain animation must turn its basin upside down");
    }

    void TestSceneResources(TWindow& Window, const GPU::TVulkanContext& Context, const TFixtures& Fixtures)
    {
        Asset::TAssetManager              Assets(Context);
        SurfaceState::TSurfaceDataManager SurfaceData(Assets);
        (void)Assets.LoadSRProfile(Fixtures.Root / "Cached.SRProfile");
        TScene Scene = TSceneLoader::Load(Fixtures.Root / "Wet.Scene", Assets, SurfaceData);
        SurfaceData.ExchangeSurfaceStateRegistry(SurfaceData.BuildSurfaceStateRegistry(Scene));
        SurfaceState::TSurfaceStateSystem SurfaceStates(Context, Assets, SurfaceData, Scene);
        Rendering::TRenderer              Renderer(Context, Window, Assets, SurfaceData, Scene, SurfaceStates);
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == 0 &&
                  Renderer.GetDemoSurfaceStateBindings().Mud == SurfaceState::InvalidStateId,
              "Wet Scene demo bindings must resolve optional names.");
        {
            TDebugUI UI(Context, Window, Renderer, Assets, SurfaceData);
            Check(UI.IsFixedSimulationTimestep() && !UI.IsAutoSubsteppingEnabled(),
                  "the actual UI must default to Fixed ON and Auto substepping OFF");
            // Keep the hidden test window from reading or overwriting the editor docking layout.
            ImGui::GetIO().IniFilename = nullptr;
            Renderer.SetTexelGridBlockSize(16);
            Renderer.SetTexelAreaReference(1.0F / (128.0F * 128.0F));
            for (auto Mode : {TRenderViewMode::Lit,
                              TRenderViewMode::SurfaceTexelGrid,
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
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateCount() == 1 &&
                  SurfaceData.GetSurfaceStateRegistry().GetStateName(0) == "wetness",
              "cached foreign States must be excluded");
        const auto WetHandle = Assets.LoadSRProfile(Fixtures.Root / "Wet.SRProfile");
        auto       Parameters = Assets.GetSRProfile(WetHandle).GetData().States.at("wetness");
        Parameters.InputFactor = 2.5F;
        Renderer.SetDebugProfileParameters(WetHandle, 0, Parameters);

        auto SwitchScene = [&](const std::string& Name)
        {
            TScene Loaded = TSceneLoader::Load(Fixtures.Root / (Name + ".Scene"), Assets, SurfaceData);
            TScene Previous = Scene;
            Scene = std::move(Loaded);
            try
            {
                Renderer.ReloadSceneResources(Scene);
            }
            catch (...)
            {
                Scene = std::move(Previous);
                throw;
            }
        };
        TScene Mud = TSceneLoader::Load(Fixtures.Root / "Mud.Scene", Assets, SurfaceData);
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateName(0) == "wetness",
              "loading prospective assets must preserve active IDs");
        SwitchScene("Mud");
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateCount() == 1 &&
                  SurfaceData.GetSurfaceStateRegistry().GetStateName(0) == "mud",
              "Scene switch must replace the Registry");
        Check(Renderer.GetDemoSurfaceStateBindings().Mud == 0 &&
                  Renderer.GetDemoSurfaceStateBindings().Wetness == SurfaceState::InvalidStateId,
              "Mud Scene must discard the former Wetness ID.");
        {
            TDebugUI UI(Context, Window, Renderer, Assets, SurfaceData);
            ImGui::GetIO().IniFilename = nullptr;
            Renderer.SetRenderViewMode(TRenderViewMode::Lit);
            const auto                         MudState = SurfaceData.GetSurfaceStateRegistry().GetStateId("mud");
            SurfaceState::TSurfaceContactInput Contact;
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
                Renderer.RenderFrame(Scene, UI, 1.0F / 60.0F);
            }
            Renderer.SetDemoSurfaceEffectSettings({});
        }

        SwitchScene("Mixed");
        const auto&                  Registry = SurfaceData.GetSurfaceStateRegistry();
        const SurfaceState::TStateId WetState = Registry.GetStateId("wetness");
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == WetState &&
                  Renderer.GetDemoSurfaceStateBindings().Mud == Registry.GetStateId("mud"),
              "Scene reload must resolve changed demo IDs.");
        Check(Registry.GetStateCount() == 2, "mixed Scene must use exactly its two States");
        const auto& GPU = Renderer.GetSurfaceGPUResources();
        Check(GPU.GetSceneProfileCount() == 2 && GPU.GetSharedSurfaceDataCount() == 3,
              "different Runtime combinations must share one deduplicated Profile table");
        const auto* A = GPU.GetInstanceDescriptors(0);
        const auto* B = GPU.GetInstanceDescriptors(1);
        const auto* C = GPU.GetInstanceDescriptors(2);
        for (auto Binding : {SurfaceState::TSurfaceGPUDescriptorBinding::ProfileParameters,
                             SurfaceState::TSurfaceGPUDescriptorBinding::ProfileSupported})
        {
            Check(A->GetBoundBufferHandle(Binding, true) == B->GetBoundBufferHandle(Binding, false) &&
                      A->GetBoundBufferHandle(Binding, true) == C->GetBoundBufferHandle(Binding, true),
                  "all Meshes must bind the same Profile GPU buffers");
        }
        Check(A->GetBoundBufferHandle(SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true) !=
                  B->GetBoundBufferHandle(SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true),
              "instance State must stay independent");
        Check(std::abs(GetProfileInputFactor(GPU, SurfaceData, Scene, WetHandle, WetState) - 0.75F) < 1e-6F,
              "Scene switch must discard old numeric-ID overrides");
        Renderer.SetDebugStateChannel(WetState);
        Parameters.InputFactor = 1.25F;
        Renderer.SetDebugProfileParameters(WetHandle, WetState, Parameters);
        Check(std::abs(GetProfileInputFactor(GPU, SurfaceData, Scene, WetHandle, WetState) - 1.25F) < 1e-6F,
              "Profile update must target the single shared table");

        {
            SurfaceState::TSurfaceStateSystem System(Context, Assets, SurfaceData, Scene);
            System.ResetState();
            for (SurfaceState::TSurfaceInstanceID Target : {0U, 1U, 2U})
            {
                SurfaceState::TSurfaceContactInput Contact;
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
                const auto&        Resources = System.GetGPUResources();
                std::vector<float> Values(Resources.GetInstanceTexelCount(Index) * Registry.GetStateCount());
                Resources.GetInstanceCurrentStateBuffer(Index).Download(Values.data(), Values.size() * sizeof(float));
                const float Sum = std::accumulate(Values.begin(), Values.end(), 0.0F);
                Check(Index == 2 ? Sum == 0.0F : Sum > 0.0F,
                      "local Profile order remapping must inject Wetness into both Wet Meshes and skip the Mud Mesh");
                if (Index < 2)
                {
                    const auto& MeshInstance = Scene.GetStaticMeshInstances()[Index];
                    const auto& Geometry =
                        *SurfaceData.GetSurfaceData(MeshInstance.GetSurfaceData()).GetSharedGeometry();
                    const auto Areas =
                        BuildSurfaceGPUWorldTexelAreas(Geometry, MeshInstance.GetTransform().GetMatrix());
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
        Check(
            std::abs(GetProfileInputFactor(Renderer.GetSurfaceGPUResources(), SurfaceData, Scene, WetHandle, WetState) -
                     1.25F) < 1e-6F,
            "resolution change must preserve current Scene tuning");

        {
            // A vertical unit triangle must reduce dt with the new rate, including runtime overrides.
            auto&      Transform = Scene.GetStaticMeshInstances()[0].GetTransform();
            const auto PreviousRotation = Transform.RotationDegrees;
            Transform.RotationDegrees = {90, 0, 0};
            {
                SurfaceState::TSurfaceStateSystem System(Context, Assets, SurfaceData, Scene);
                auto                              Flow = Assets.GetSRProfile(WetHandle).GetData().States.at("wetness");
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
        const std::size_t Count =
            Limits.limits.maxStorageBufferRange / (256U * 256U * SurfaceNeighborCount * sizeof(float)) + 1U;
        std::vector<std::string> OversizedStates;
        for (std::size_t Index = 0; Index < Count; ++Index)
            OversizedStates.push_back("new_" + std::to_string(Index));
        Fixtures.WriteProfile("Oversized", OversizedStates, 1.0F);
        Fixtures.WriteMap("Oversized", {"Oversized.SRProfile"}, 0);
        Fixtures.WriteScene("Oversized", {{"A.obj", "Oversized.SurfaceProfileMap"}});
        const VkBuffer PreviousState =
            Renderer.GetSurfaceGPUResources().GetInstanceDescriptors(0)->GetBoundBufferHandle(
                SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true);
        bool bFailed = false;
        try
        {
            SwitchScene("Oversized");
        }
        catch (const std::length_error&)
        {
            bFailed = true;
        }
        Check(bFailed, "oversized Scene must fail GPU resource construction");
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateCount() == 2 &&
                  SurfaceData.GetSurfaceStateRegistry().GetStateId("wetness") == WetState,
              "failed resource rebuild must restore the previous Registry");
        Check(Renderer.GetSurfaceGPUResources().GetInstanceDescriptors(0)->GetBoundBufferHandle(
                  SurfaceState::TSurfaceGPUDescriptorBinding::CurrentState, true) == PreviousState,
              "failed Scene switch must retain previous GPU resources");
        Check(
            std::abs(GetProfileInputFactor(Renderer.GetSurfaceGPUResources(), SurfaceData, Scene, WetHandle, WetState) -
                     1.25F) < 1e-6F,
            "failed switch must retain previous tuning");
        SwitchScene("Empty");
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateCount() == 0 &&
                  Renderer.GetSurfaceGPUResources().GetManagedInstanceCount() == 0,
              "empty Scene must have no State channels or resources");
        Check(Renderer.GetDemoSurfaceStateBindings().Wetness == SurfaceState::InvalidStateId &&
                  Renderer.GetDemoSurfaceStateBindings().Mud == SurfaceState::InvalidStateId,
              "Empty Registry must resolve both demo States as absent.");
        SwitchScene("Wet");
        Check(SurfaceData.GetSurfaceStateRegistry().GetStateCount() == 1 &&
                  SurfaceData.GetSurfaceStateRegistry().GetStateName(0) == "wetness",
              "switching back must exclude cached failed Scene States");
    }
}

int main(int Argc, char* Argv[])
{
    std::unique_ptr<MDSS::TWindow>             Window;
    std::unique_ptr<MDSS::GPU::TVulkanContext> Context;
    try
    {
        Window = std::make_unique<MDSS::TWindow>(320, 240, "MDSS Scene resource tests");
        glfwHideWindow(Window->GetNativeHandle());
        Context = std::make_unique<MDSS::GPU::TVulkanContext>(*Window);
    }
    catch (const std::exception& Error)
    {
        std::cout << "SKIP: native window/Vulkan context unavailable: " << Error.what() << '\n';
        return 77;
    }
    try
    {
        if (Argc == 2 && std::string_view(Argv[1]) == "--cavity-fill-bound")
        {
            MDSS::Tests::TestCavityFillDisplayBound(*Context);
            std::cout << "Cavity fill display bound checks passed.\n";
            return 0;
        }
        if (Argc == 2 && std::string_view(Argv[1]) == "--lava-scene")
        {
            Asset::TAssetManager              LavaAssets(*Context);
            SurfaceState::TSurfaceDataManager LavaSurfaceData(LavaAssets);
            TScene                            LavaScene = TSceneLoader::Load(
                std::filesystem::path(MDSS_ASSET_DIR) / "Scenes/Mountain_Lava.Scene", LavaAssets, LavaSurfaceData);
            LavaSurfaceData.ExchangeSurfaceStateRegistry(LavaSurfaceData.BuildSurfaceStateRegistry(LavaScene));
            SurfaceState::TSurfaceStateSystem LavaStates(*Context, LavaAssets, LavaSurfaceData, LavaScene);
            Rendering::TRenderer LavaRenderer(*Context, *Window, LavaAssets, LavaSurfaceData, LavaScene, LavaStates);
            TDebugUI             LavaUI(*Context, *Window, LavaRenderer, LavaAssets, LavaSurfaceData);
            ImGui::GetIO().IniFilename = nullptr;
            LavaRenderer.SetRenderViewMode(TRenderViewMode::Lit);
            Check(LavaRenderer.GetDemoSurfaceStateBindings().Lava != SurfaceState::InvalidStateId,
                  "Lava Scene must bind its rendering State");
            Window->PollEvents();
            LavaUI.BeginFrame(LavaScene);
            LavaRenderer.RenderFrame(LavaScene, LavaUI, 1.0F / 60.0F);
            vkDeviceWaitIdle(Context->GetDevice());
            const auto&        StateBuffer = LavaStates.GetGPUResources().GetInstanceCurrentStateBuffer(0);
            std::vector<float> StateValues(StateBuffer.GetSize() / sizeof(float));
            StateBuffer.Download(StateValues.data(), StateBuffer.GetSize());
            Check(std::any_of(StateValues.begin(), StateValues.end(), [](float Value) { return Value > 0.0F; }),
                  "Lava Scene must seed visible State on its first simulation step");
            LavaScene.PlayDemoAnimation();
            LavaScene.AdvanceDemoAnimation(9.0F);
            Window->PollEvents();
            LavaUI.BeginFrame(LavaScene);
            LavaRenderer.RenderFrame(LavaScene, LavaUI, 1.0F / 60.0F);
            vkDeviceWaitIdle(Context->GetDevice());
            LavaUI.RestartScene(LavaScene);
            Check(LavaScene.GetDemoAnimationTime() == 0.0F && !LavaScene.IsDemoAnimationPlaying() &&
                      LavaScene.GetStaticMeshInstances()[0].GetTransform().RotationDegrees.x == 90.0F &&
                      LavaRenderer.GetSimulatedSeconds() == 0.0,
                  "Restart Scene must restore saved pose, animation and simulation clock");
            const auto&        RestartedBuffer = LavaStates.GetGPUResources().GetInstanceCurrentStateBuffer(0);
            std::vector<float> RestartedValues(RestartedBuffer.GetSize() / sizeof(float));
            RestartedBuffer.Download(RestartedValues.data(), RestartedBuffer.GetSize());
            Check(
                std::all_of(RestartedValues.begin(), RestartedValues.end(), [](float Value) { return Value == 0.0F; }),
                "Restart Scene must clear the previous Lava State before replaying initial contact");
            Window->PollEvents();
            LavaUI.BeginFrame(LavaScene);
            LavaRenderer.RenderFrame(LavaScene, LavaUI, 1.0F / 60.0F);
            vkDeviceWaitIdle(Context->GetDevice());
            const auto&        ReplayedBuffer = LavaStates.GetGPUResources().GetInstanceCurrentStateBuffer(0);
            std::vector<float> ReplayedValues(ReplayedBuffer.GetSize() / sizeof(float));
            ReplayedBuffer.Download(ReplayedValues.data(), ReplayedBuffer.GetSize());
            Check(std::any_of(ReplayedValues.begin(), ReplayedValues.end(), [](float Value) { return Value > 0.0F; }),
                  "Restart Scene must apply its initial Lava contact again");
            for (const auto& Entry : MDSS::TLogger::GetEntries())
                Check(Entry.Module != "Vulkan Validation" || Entry.Level != MDSS::TLogLevel::Error,
                      "Lava Scene rendering must have no Vulkan validation errors");
            std::cout << "Lava Scene checks passed.\n";
            return 0;
        }
        if (Argc == 2 && std::string_view(Argv[1]) == "--overlay-side-compaction")
        {
            MDSS::Tests::TestOverlaySideCompaction(*Context);
            {
                TFixtures                         OverlayFixtures;
                Asset::TAssetManager              OverlayAssets(*Context);
                SurfaceState::TSurfaceDataManager OverlaySurfaceData(OverlayAssets);
                TScene                            OverlayScene =
                    TSceneLoader::Load(OverlayFixtures.Root / "Mud.Scene", OverlayAssets, OverlaySurfaceData);
                OverlaySurfaceData.ExchangeSurfaceStateRegistry(
                    OverlaySurfaceData.BuildSurfaceStateRegistry(OverlayScene));
                SurfaceState::TSurfaceStateSystem OverlayStates(
                    *Context, OverlayAssets, OverlaySurfaceData, OverlayScene);
                Rendering::TRenderer OverlayRenderer(
                    *Context, *Window, OverlayAssets, OverlaySurfaceData, OverlayScene, OverlayStates);
                TDebugUI OverlayUI(*Context, *Window, OverlayRenderer, OverlayAssets, OverlaySurfaceData);
                ImGui::GetIO().IniFilename = nullptr;
                OverlayRenderer.SetRenderViewMode(TRenderViewMode::Lit);
                Check(OverlayRenderer.IsLitTexelMeshBaseRendered(OverlayScene, 0),
                      "An active Lit accumulation layer must draw its base with the texel mesh.");
                OverlayRenderer.SetRenderViewMode(TRenderViewMode::MacroGeometry);
                Check(!OverlayRenderer.IsLitTexelMeshBaseRendered(OverlayScene, 0),
                      "Base Geometry view must not report Lit texel mesh rendering.");
                OverlayRenderer.SetRenderViewMode(TRenderViewMode::Lit);
                auto DisabledEffects = OverlayRenderer.GetDemoSurfaceEffectSettings();
                DisabledEffects.bMudDisplacement = false;
                OverlayRenderer.SetDemoSurfaceEffectSettings(DisabledEffects);
                Check(!OverlayRenderer.IsLitTexelMeshBaseRendered(OverlayScene, 0),
                      "The notice must hide when its only accumulation layer is disabled.");
                DisabledEffects.bMudDisplacement = true;
                DisabledEffects.bEnabled = false;
                OverlayRenderer.SetDemoSurfaceEffectSettings(DisabledEffects);
                Check(!OverlayRenderer.IsLitTexelMeshBaseRendered(OverlayScene, 0),
                      "The notice must hide when Lit effects are disabled.");
                DisabledEffects.bEnabled = true;
                OverlayRenderer.SetDemoSurfaceEffectSettings(DisabledEffects);
                Window->PollEvents();
                OverlayUI.BeginFrame(OverlayScene);
                OverlayRenderer.RenderFrame(OverlayScene, OverlayUI, 0.0F);
                OverlayRenderer.SetAccumulationFeedbackEnabled(true);
                Window->PollEvents();
                OverlayUI.BeginFrame(OverlayScene);
                OverlayRenderer.RenderFrame(OverlayScene, OverlayUI, 1.0F / 60.0F, true);
                Check(OverlayRenderer.GetLastSimulationStepCount() == 0 &&
                          OverlayRenderer.GetPendingSimulationSeconds() == 0.0,
                      "changing feedback must prepare resources without advancing simulation time");
                Window->PollEvents();
                OverlayUI.BeginFrame(OverlayScene);
                OverlayRenderer.RenderFrame(OverlayScene, OverlayUI, 1.0F / 60.0F);
                Check(OverlayRenderer.GetLastSimulationStepCount() == 1,
                      "simulation must resume with one normal tick after feedback preparation");
                vkDeviceWaitIdle(Context->GetDevice());
            }
            Context.reset();
            Window.reset();
            for (const auto& Entry : MDSS::TLogger::GetEntries())
                Check(Entry.Module != "Vulkan Validation" || Entry.Level != MDSS::TLogLevel::Error,
                      "Overlay side compaction must have no Vulkan validation errors");
            std::cout << "Overlay side compaction checks passed.\n";
            return 0;
        }
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
            Check(Entry.Module != "Vulkan Validation" || Entry.Level != MDSS::TLogLevel::Error,
                  "Vulkan validation must have no errors");
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
