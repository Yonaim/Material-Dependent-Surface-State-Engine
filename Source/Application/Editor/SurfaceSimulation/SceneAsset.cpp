/**
 * @file SceneAsset.cpp
 * @brief 저작된 상태·필드·규칙을 Surface Simulation의 단위가 정해진 솔버 슬롯에 바인딩한다.
 */
#include "Application/Editor/SurfaceSimulation/SceneAsset.h"
#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "AssetManager/Loaders/YamlAssetLoader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <glm/common.hpp>
#include <glm/trigonometric.hpp>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Typed Assets and Solver Bindings
    // ============================================================
    #pragma region 00_Typed_Assets_and_Solver_Bindings
        // 자산 ID와 단위를 검증하고 실제 계산 슬롯에 연결한다.
        namespace
        {
            using FJson = nlohmann::json;

            struct FSceneAssetEntry
            {
                std::string_view File;
                std::string_view Geometry;
            };

            constexpr std::array SceneAssets{FSceneAssetEntry{"BrickRunoff.Scene", "GroovedPlate"},
                                             FSceneAssetEntry{"SnowOnFabric.Scene", "WovenPatch"},
                                             FSceneAssetEntry{"CharringWood.Scene", "WoodPatch"},
                                             FSceneAssetEntry{"TerracedCatchment.Scene", "TerracedBed"},
                                             FSceneAssetEntry{"HeatedWaterChannel.Scene", "Spillway"},
                                             FSceneAssetEntry{"CanyonCascade.Scene", "CanyonBed"},
                                             FSceneAssetEntry{"GlacierCut.Scene", "GlacialBed"},
                                             FSceneAssetEntry{"CavityFillAndSpill.Scene", "CavityChart"},
                                             FSceneAssetEntry{"FreezeAndThaw.Scene", "PhaseTray"},
                                             FSceneAssetEntry{"MeadowFirebreak.Scene", "MeadowBed"},
                                             FSceneAssetEntry{"IceGate.Scene", "IceGateFork"},
                                             FSceneAssetEntry{"SoilCoveredStonePath.Scene", "SoilPathFork"},
                                             FSceneAssetEntry{"FabricVsSponge.Scene", "MaterialSpecimens"}};
            static_assert(SceneAssets.size() == SceneKeys.size());

            bool IsSurfaceSceneType(const FJson &Type)
            {
                // 기존 v1 파일은 새 에디터의 같은 로더에서 읽는다.
                return Type == "SurfaceScene" || Type == "SurfaceLabScene";
            }

            FJson ReadTypedAsset(const std::filesystem::path &Path, std::string_view Type)
            {
                try
                {
                    std::ifstream Input(Path);
                    if (!Input)
                        throw std::runtime_error("Unable to open asset.");
                    auto        Data = FJson::parse(Input);
                    const auto &AssetType = Data.at("type");
                    const bool  bMatches = AssetType.is_string() &&
                                           (Type == "SurfaceScene" ? IsSurfaceSceneType(AssetType)
                                                                   : AssetType.get_ref<const std::string &>() == Type);
                    if (!bMatches || !Data.at("version").is_number_integer() || Data.at("version") != 1)
                        throw std::runtime_error("Expected " + std::string(Type) + " v1.");
                    return Data;
                }
                catch (const std::exception &Error)
                {
                    throw std::runtime_error(Path.string() + ": " + Error.what());
                }
            }

            FJson ReadSharedAsset(const std::filesystem::path &Directory, const std::string &Id, std::string_view Type)
            {
                if (Id.empty() || !std::ranges::all_of(Id,
                                                       [](char C)
                                                       {
                                                           return (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
                                                                  (C >= '0' && C <= '9') || C == '_';
                                                       }))
                    throw std::runtime_error("Invalid asset ID: " + Id);
                const auto Path =
                    std::filesystem::path(MDSS_ASSET_DIR) / "Simulation" / Directory / (Id + "." + std::string(Type));
                return Asset::LoadVersionedYamlAsset(Path);
            }

            std::filesystem::path SceneTexturePath(const std::filesystem::path &ScenePath,
                                                   const FJson                 &Data,
                                                   const char                  *Key,
                                                   const char                  *Extension = ".png")
            {
                const std::filesystem::path Relative = Data.at(Key).get<std::string>();
                if (Relative.has_root_path() || Relative.extension() != Extension)
                    throw std::runtime_error(std::string(Key) + " must be a " + Extension + " path relative to the scene.");
                return (ScenePath.parent_path() / Relative).lexically_normal();
            }

            float Number(const FJson &Data, const char *Key, float Minimum = 0.F)
            {
                const auto &Value = Data.at(Key);
                if (!Value.is_number())
                    throw std::runtime_error(std::string(Key) + " must be numeric.");
                const auto Result = Value.get<float>();
                if (!std::isfinite(Result) || Result < Minimum)
                    throw std::runtime_error(std::string(Key) + " is outside its valid range.");
                return Result;
            }

            glm::vec3 StateLegendColor(std::size_t Index)
            {
                // 홀수 인덱스는 직전 색의 보색이다. 나머지 비트는 이전 색 사이에 새 짝을 배치한다.
                float Hue = 0.58F;
                for (float Interval = 0.5F; Index != 0; Index >>= 1U, Interval *= 0.5F)
                    Hue += (Index & 1U) != 0 ? Interval : 0.F;
                const auto Rgb = glm::clamp(
                    glm::abs(glm::fract(glm::vec3(Hue) + glm::vec3(0.F, 2.F / 3.F, 1.F / 3.F)) * 6.F - 3.F) - 1.F,
                    0.F,
                    1.F);
                return glm::mix(glm::vec3(0.28F), glm::vec3(1.F), Rgb);
            }

            void BindStateVisualization(FSurfaceSceneSettings &Result, const FJson &State, bool bSubstrate)
            {
                using enum Rendering::ESurfaceStateChannel;
                const auto Id = State.at("id").get<std::string>();
                const bool bMass = State.at("quantity") == "mass" && State.at("unit") == "kg";
                auto       Channel = Count;
                if (bMass && State.at("phase") == "liquid")
                    Channel = bSubstrate ? AbsorbedMass : LiquidMass;
                else if (bMass && !bSubstrate && (State.at("phase") == "ice" || State.at("phase") == "snow"))
                    Channel = IceMass;
                else if (bSubstrate && State.at("quantity") == "progress" && State.at("unit") == "1" &&
                         State.value("source", std::string{}) == "charFraction")
                    Channel = CharFraction;
                else if (bSubstrate && State.at("quantity") == "temperature" && State.at("unit") == "K" &&
                         State.value("source", std::string{}) == "temperature")
                    Channel = Temperature;
                if (Channel == Count ||
                    std::ranges::any_of(Result.States,
                                        [&](const auto &Entry) { return Entry.Channel == Channel || Entry.Id == Id; }))
                    throw std::runtime_error("State cannot bind to a unique visualization channel: " + Id);

                const bool  bTemperature = Channel == Temperature;
                const float Maximum = bTemperature ? Number(State, "displayMax", Liquid::AmbientTemperatureK + 1.F)
                                      : State.contains("displayMax") ? Number(State, "displayMax", 1.e-6F)
                                                                     : 1.F;
                Result.States.push_back(
                    {Channel,
                     Id,
                     State.value("displayName", Id),
                     bMass          ? "kg/m^2"
                     : bTemperature ? "K"
                                    : "",
                     StateLegendColor(Result.States.size()),
                     Maximum,
                     bSubstrate,
                     State.contains("thicknessDisplayMaxMm") ? Number(State, "thicknessDisplayMaxMm", 1.e-6F) : 0.F});
            }

            void BindProfileStateGroup(FSurfaceSceneSettings &Result, const FJson &States, bool bSubstrate)
            {
                if (!States.is_array())
                    throw std::runtime_error("Expected an array of state definitions.");
                for (const auto &State : States)
                    BindStateVisualization(Result, State, bSubstrate);
            }

            std::array<int, 2> Grid(const FJson &Data, const char *Key, int Minimum, int Maximum)
            {
                const auto &Values = Data.at(Key);
                if (!Values.is_array() || Values.size() != 2)
                    throw std::runtime_error(std::string(Key) + " must specify two axes.");
                std::array<int, 2> Result{};
                for (std::size_t Axis = 0; Axis < Values.size(); ++Axis)
                {
                    if (!Values[Axis].is_number_integer() || Values[Axis] < Minimum || Values[Axis] > Maximum)
                        throw std::runtime_error(std::string(Key) + " has an invalid axis size.");
                    Result[Axis] = Values[Axis].get<int>();
                }
                return Result;
            }

            void ReadSceneResolution(FSurfaceSceneSettings &Settings, const FJson &Scene)
            {
                const auto Cells = Grid(Scene, "simulationCells", 1, Liquid::MaxCellsPerAxis);
                const auto Preset = Scene.find("geometrySamplesPerCell");
                auto       Samples = std::array<int, 2>{};

                if (Preset == Scene.end())
                    Samples = Grid(Scene, "geometrySamples", 2, Liquid::MaxSamplesPerAxis);
                else
                {
                    if (Scene.contains("geometrySamples"))
                        throw std::runtime_error("Choose either geometrySamplesPerCell or geometrySamples.");
                    const auto MatchesPreset = [&](int Value) { return *Preset == Value; };
                    const auto Choice = std::ranges::find_if(GeometrySamplesPerCellPresets, MatchesPreset);
                    if (!Preset->is_number_integer() || Choice == GeometrySamplesPerCellPresets.end())
                        throw std::runtime_error("geometrySamplesPerCell must be 1, 2, 4, or 8.");
                    Settings.GeometrySamplesPerCell = *Choice;
                    Samples = {Cells[0] * *Choice, Cells[1] * *Choice};
                    if (std::ranges::any_of(Samples,
                                            [](int Value) { return Value < 2 || Value > Liquid::MaxSamplesPerAxis; }))
                        throw std::runtime_error(
                            "geometrySamplesPerCell produces a geometry grid outside the supported range.");
                }
                if (Cells[0] > Samples[0] || Cells[1] > Samples[1])
                    throw std::runtime_error("Each simulation cell needs at least one geometry sample.");

                Settings.GridColumns = Cells[0];
                Settings.GridRows = Cells[1];
                Settings.ShapeColumns = Samples[0];
                Settings.ShapeRows = Samples[1];
            }

            const FJson &Field(const FJson         &Scene,
                               const FJson         &Profile,
                               const FJson         &Binding,
                               const char          *Port,
                               const char          *Unit,
                               FInteractionBinding &Resolved)
            {
                const auto  Id = Binding.at("inputs").at(Port).get<std::string>();
                const auto &Definition = Profile.at("fields").at(Id);
                if (Definition.at("provider") != "SceneField" || Definition.at("unit") != Unit)
                    throw std::runtime_error("Incompatible field binding: " + Id);
                Resolved.Field = Id;
                Resolved.SceneField = Definition.at("source").get<std::string>();
                const auto &Source = Scene.at("fields").at(Resolved.SceneField);
                if (Source.at("unit") != Unit)
                    throw std::runtime_error("Incompatible scene field unit: " + Id);
                return Source;
            }

            float ArealHeatCapacity(const FJson &Profile)
            {
                const auto  Material = ReadSharedAsset("Materials", Profile.at("substrate"), "PhysicalMaterial");
                const auto &Thermal = Material.at("thermal");
                const auto &Storage = Profile.at("substrateStorage").at("SubstrateTemperature");
                const float Capacity = Number(Thermal, "densityKgM3", 1.e-6F) *
                                       Number(Thermal, "specificHeatJkgK", 1.e-6F) *
                                       Number(Storage, "effectiveThicknessM", 1.e-6F);
                if (!std::isfinite(Capacity) || Capacity < 1.F)
                    throw std::runtime_error("Effective areal heat capacity must be finite and at least 1 J/(m² K).");
                return Capacity;
            }

            void BindLayerStates(FSurfaceSceneSettings &Result, const FJson &Profile, std::vector<std::string> &Ids)
            {
                auto       &Settings = *Result.Profile;
                const auto &Layers = Profile.at("states").at("layer");
                if (!Layers.is_array() || Layers.empty() || Layers.size() > 8)
                    throw std::runtime_error("Film profiles require one to eight ordered layers.");
                if (!std::ranges::any_of(
                        Layers,
                        [&](const auto &Layer)
                        { return Layer.at("id") == Settings.LiquidState && Layer.at("phase") == "liquid"; }))
                    throw std::runtime_error("transport.state must reference a declared liquid layer.");
                for (const auto &Layer : Layers)
                {
                    if (Layer.at("quantity") != "mass" || Layer.at("unit") != "kg")
                        throw std::runtime_error("Layered thin film requires mass states in kg.");
                    const auto  Material = ReadSharedAsset("Materials", Layer.at("material"), "PhysicalMaterial");
                    const auto  PhaseId = Layer.at("phase").get<std::string>();
                    const auto &Phase = Material.at("phases").at(PhaseId);
                    const auto  FilmIndex = static_cast<int>(Ids.size());
                    const auto  Id = Layer.at("id").get<std::string>();
                    if (Id.empty() || Id == Settings.IceState || std::ranges::find(Ids, Id) != Ids.end())
                        throw std::runtime_error("Layer IDs must be nonempty and unique.");
                    const auto Storage = Layer.value("storage", std::string("region"));
                    if (Storage == "cell")
                    {
                        // 눈처럼 셀에 한 값을 두는 층도 같은 Local Update에서 입력으로 연결한다.
                        if ((PhaseId != "ice" && PhaseId != "snow") || !Settings.IceState.empty() || Ids.empty() ||
                            Ids.back() != Settings.LiquidState || &Layer != &Layers.back() ||
                            Material.at("id") != Settings.LiquidMaterial)
                            throw std::runtime_error(
                                "Cell-stored ice or snow must be the final layer above its matching liquid.");
                        Settings.IceState = Id;
                        BindStateVisualization(Result, Layer, false);
                        continue;
                    }
                    if (Storage != "region")
                        throw std::runtime_error("Layer storage must be region or cell.");
                    if (PhaseId == "ice" && Material.at("id") == "Water")
                        Result.FilmIceLayer = FilmIndex;
                    Ids.push_back(Id);
                    if (Id == Settings.LiquidState)
                        Settings.LiquidMaterial = Material.at("id").get<std::string>();
                    constexpr float DegToRad = 0.01745329252F;
                    const auto      Angle = Phase.find("angleOfReposeDeg");
                    const float     Degrees =
                        Angle == Phase.end() || Angle->is_null() ? 0.F : Number(Phase, "angleOfReposeDeg");
                    if (Degrees >= 90.F)
                        throw std::runtime_error("Film repose angle must be below 90 degrees.");
                    Result.FilmLayers.push_back(
                        {Number(Phase, "densityKgM3", 1.e-6F),
                         Number(Phase, "dynamicViscosityPaS", 1.e-9F),
                         Phase.contains("yieldStressPa") ? Number(Phase, "yieldStressPa") : 0.F,
                         std::tan(Degrees * DegToRad),
                         Layer.value("color", std::array<float, 3>{}),
                         Phase.contains("dryViscosityMultiplier") ? Number(Phase, "dryViscosityMultiplier", 1.F) : 1.F,
                         Phase.contains("dryYieldStressPa") ? Number(Phase, "dryYieldStressPa") : 0.F,
                         Phase.contains("wetWaterFraction") ? Number(Phase, "wetWaterFraction", 1.e-6F) : 0.F});
                    const auto &Film = Result.FilmLayers.back();
                    if (Film.WetWaterFraction > 1.F)
                        throw std::runtime_error("A wet mass fraction must lie in [0, 1].");
                    if (!std::ranges::all_of(Film.Color, [](float V) { return std::isfinite(V) && V >= 0.F && V <= 1.F; }))
                        throw std::runtime_error("Film coating colors must be finite RGB values in [0, 1].");
                    const float Range =
                        Layer.contains("thicknessDisplayMaxMm") ? Number(Layer, "thicknessDisplayMaxMm", 1.e-6F) : 0.F;
                    using enum Rendering::ESurfaceStateChannel;
                    const auto Channel =
                        Id == Settings.LiquidState
                            ? LiquidMass
                            : static_cast<Rendering::ESurfaceStateChannel>(static_cast<int>(FirstFilmMass) + FilmIndex);
                    Result.States.push_back({Channel,
                                             Id,
                                             Layer.value("displayName", Id),
                                             "kg/m^2",
                                             StateLegendColor(Result.States.size()),
                                             Layer.contains("displayMax") ? Number(Layer, "displayMax", 1.e-6F)
                                             : Range > 0.F                ? Range * 0.001F * Film.DensityKgM3
                                                                          : 1.F,
                                             false,
                                             Range});
                }
                if (Ids.empty() || Ids.back() != Settings.LiquidState)
                    throw std::runtime_error("transport.state must name the uppermost region-stored liquid layer.");

                // 기본 수심과 급수는 지정 액체 상태를 사용한다.
                const auto Primary = std::ranges::find_if(
                    Result.States,
                    [](const auto &State) { return State.Channel == Rendering::ESurfaceStateChannel::LiquidMass; });
                std::rotate(Result.States.begin(), Primary, Primary + 1);
                using enum Rendering::ESurfaceStateChannel;
                for (std::size_t Layer = 0; Layer < Result.FilmLayers.size(); ++Layer)
                {
                    if (Result.FilmLayers[Layer].WetWaterFraction <= 0.F)
                        continue;
                    const auto &Id = Ids[Layer];
                    const auto  AddComponent = [&](Rendering::ESurfaceStateChannel First,
                                                   const char                     *Suffix,
                                                   const char                     *Label,
                                                   const char                     *Unit,
                                                   float                           Maximum)
                    {
                        const auto Channel = static_cast<Rendering::ESurfaceStateChannel>(static_cast<int>(First) + Layer);
                        Result.States.push_back({Channel,
                                                 Id + Suffix,
                                                 Id + Label,
                                                 Unit,
                                                 StateLegendColor(Result.States.size()),
                                                 Maximum,
                                                 false});
                    };
                    const float MassRange = 0.004F * Result.FilmLayers[Layer].DensityKgM3;
                    AddComponent(FirstFilmSolidMass, ".solids", " solids", "kg/m^2", MassRange);
                    AddComponent(FirstFilmWaterMass, ".water", " mixed water", "kg/m^2", MassRange);
                    AddComponent(FirstFilmWaterFraction, ".moisture", " moisture", "%", 100.F);
                }
            }

            int FilmLayerIndex(std::span<const std::string> Ids, const FJson &Data, const char *Key)
            {
                const auto Reference = Data.at(Key).get<std::string>();
                const auto Id = Reference.starts_with("layer.") ? Reference.substr(6) : Reference;
                const auto At = std::ranges::find(Ids, Id);
                if (At == Ids.end())
                    throw std::runtime_error("Unknown film layer: " + Reference);
                return static_cast<int>(At - Ids.begin());
            }

            void BindInitialFilm(FSurfaceSceneSettings &Result, const FJson &Scene, std::span<const std::string> Ids)
            {
                for (const auto &Patch : Scene.value("initialFilm", FJson::array()))
                {
                    const auto Bounds = Patch.at("boundsUV").get<std::array<float, 4>>();
                    if (!std::ranges::all_of(Bounds, [](float V) { return std::isfinite(V) && V >= 0.F && V <= 1.F; }) ||
                        Bounds[0] >= Bounds[2] || Bounds[1] >= Bounds[3])
                        throw std::runtime_error("Initial film bounds must define a positive rectangle inside the chart.");
                    Result.FilmPatches.push_back(
                        {FilmLayerIndex(Ids, Patch, "state"), Bounds, Number(Patch, "thicknessM")});
                }
                Result.bFilmBedCoupling = Scene.value("filmBedCoupling", true);
                if (Scene.contains("omitFilmSupport"))
                {
                    Result.FilmOmittedSupport = FilmLayerIndex(Ids, Scene, "omitFilmSupport");
                    if (Result.FilmOmittedSupport == static_cast<int>(Ids.size()) - 1)
                        throw std::runtime_error("The control omits a supporting layer below the water.");
                }
                if (Scene.contains("branchRiseM"))
                    Result.BranchRiseM = Number(Scene, "branchRiseM");
                if (Scene.contains("depositionBasinDepthM"))
                    Result.DepositionBasinDepthM = Number(Scene, "depositionBasinDepthM");
                Result.SecondPourStart = Scene.contains("secondPourStartS") ? Number(Scene, "secondPourStartS") : 0.F;
            }

            void BindRules(FSurfaceSceneSettings       &SceneSettings,
                           const FJson                 &Scene,
                           const FJson                 &Profile,
                           std::span<const std::string> Ids)
            {
                auto                &Result = *SceneSettings.Profile;
                const auto          &Layers = Profile.at("states").at("layer");
                const auto          &LiquidState = Result.LiquidState;
                const auto          &IceState = Result.IceState;
                const auto          &AbsorbedState = Result.AbsorbedState;
                const auto          &Bindings = Profile.at("interactions");
                std::optional<float> Capacity;
                if (const auto Storage = Profile.find("substrateStorage"); Storage != Profile.end())
                {
                    if (!Storage->is_object())
                        throw std::runtime_error("substrateStorage must be an object indexed by state ID.");
                    for (auto Entry = Storage->begin(); Entry != Storage->end(); ++Entry)
                    {
                        if (!AbsorbedState.empty() && Entry.key() == AbsorbedState)
                            Capacity = Number(Entry.value(), "capacityKgM2");
                        else if (Entry.key() != "SubstrateTemperature")
                            throw std::runtime_error("Unknown substrate storage state: " + Entry.key());
                    }
                }
                if (!Bindings.is_array())
                    throw std::runtime_error("interactions must be an array.");
                std::unordered_set<std::string> Models;
                for (const auto &Binding : Bindings)
                {
                    const auto RuleId = Binding.at("use").get<std::string>();
                    const auto Rule = ReadSharedAsset("Interactions", RuleId, "Interaction");
                    const auto Model = Rule.at("model").get<std::string>();
                    if (Model == "TemperatureDrivenMelt" && !IceState.empty() && Binding.at("from") == IceState &&
                        Binding.at("to") == LiquidState)
                    {
                        if (!Models.insert(Model).second)
                            throw std::runtime_error("Duplicate cell interaction: " + Model);
                        Result.Melt = {RuleId, IceState, LiquidState, {}, {}};
                        Result.Cell.MeltKgM2SecondK = Number(Rule, "rateKgM2SecondK");
                        if (Binding.at("inputs").at("temperature") == "SubstrateTemperature")
                        {
                            // 같은 v1 프로파일을 참조하는 기존 씬은 실내 온도 값을 재사용한다.
                            Result.Cell.TemperatureK =
                                Scene.contains("substrateTemperatureK")
                                    ? Number(Scene, "substrateTemperatureK", 1.e-6F)
                                    : Number(Scene.at("fields").at("roomTemperature"), "value", 1.e-6F);
                        }
                        else
                            Result.Cell.TemperatureK =
                                Number(Field(Scene, Profile, Binding, "temperature", "K", Result.Melt), "value", 1.e-6F);
                    }
                    else if (Model == "Absorption" && !AbsorbedState.empty() && Binding.at("from") == LiquidState &&
                             Binding.at("to") == AbsorbedState)
                    {
                        if (!Models.insert(Model).second)
                            throw std::runtime_error("Duplicate cell interaction: " + Model);
                        Result.Absorption = {RuleId, LiquidState, AbsorbedState, {}, {}};
                        Result.Cell.AbsorbPerSecond = Number(Rule, "ratePerSecond");
                        Result.Cell.CapacityKgM2 = Capacity ? *Capacity : Number(Rule, "capacityKgM2");
                    }
                    else if (Model == "FirstOrderDrying" && !AbsorbedState.empty() && Binding.at("from") == AbsorbedState)
                    {
                        if (!Models.insert(Model).second)
                            throw std::runtime_error("Duplicate cell interaction: " + Model);
                        Result.Drying = {RuleId, AbsorbedState, {}, {}, {}};
                        const auto &Split = Field(Scene, Profile, Binding, "rateMultiplier", "1", Result.Drying);
                        Result.Cell.DryPerSecond = Number(Rule, "ratePerSecond");
                        Result.DryingSplitV = Number(Split, "belowV");
                        Result.DryingLow = Number(Split, "low");
                        Result.DryingHigh = Number(Split, "high");
                        if (Result.DryingSplitV > 1.F)
                            throw std::runtime_error("The drying boundary must lie within the chart.");
                    }
                    else if (Model == "TemperatureDrivenMelt" || Model == "TemperatureDrivenSolidification")
                    {
                        const int  From = FilmLayerIndex(Ids, Binding, "from"), To = FilmLayerIndex(Ids, Binding, "to");
                        const bool bMelting = Model == "TemperatureDrivenMelt";
                        if (From == To)
                            throw std::runtime_error("A phase transition needs distinct input and output.");
                        const auto MaterialId = Layers.at(From).at("material").get<std::string>();
                        if (Layers.at(To).at("material") != MaterialId)
                            throw std::runtime_error(
                                "A phase transition must use two phases of the same physical material.");
                        const auto  Material = ReadSharedAsset("Materials", MaterialId, "PhysicalMaterial");
                        const auto &Transition = Material.at("transitions").at(Rule.at("transition").get<std::string>());
                        const auto &FromPhase = Layers.at(From).at("phase");
                        const auto &ToPhase = Layers.at(To).at("phase");
                        const auto  Sources = Transition.at("meltFrom").get<std::vector<std::string>>();
                        const bool  bMatches =
                            bMelting ? std::ranges::find(Sources, FromPhase.get<std::string>()) != Sources.end() &&
                                           ToPhase == Transition.at("liquid")
                                     : FromPhase == Transition.at("liquid") && ToPhase == Transition.at("freezeTo");
                        if (!bMatches)
                            throw std::runtime_error("Phases must match the shared material transition.");
                        Liquid::FFilmReaction Reaction{From,
                                                       To,
                                                       To,
                                                       Number(Rule, "rateKgM2SecondK"),
                                                       0.F,
                                                       Number(Transition, "equilibriumTemperatureK", 1.e-6F)};
                        Reaction.TemperatureDirection = bMelting ? 1.F : -1.F;
                        SceneSettings.FilmReactions.push_back(Reaction);
                    }
                    else if (Model == "SoilWaterMix")
                    {
                        const int Soil = FilmLayerIndex(Ids, Binding, "soil"), Mud = FilmLayerIndex(Ids, Binding, "mud");
                        const int Water = FilmLayerIndex(Ids, Binding, "water");
                        if (Soil == Mud || Soil == Water || Mud == Water)
                            throw std::runtime_error("Mixing needs three distinct layers.");
                        SceneSettings.FilmReactions.push_back(
                            {Soil,
                             Mud,
                             Water,
                             Number(Rule, "rateKgM2Second"),
                             Number(Rule, "waterPerSoil", 1.e-6F),
                             0.F,
                             Scene.value("enableFilmRewetting", true) && Rule.contains("rewetKgM2Second")
                                 ? Number(Rule, "rewetKgM2Second")
                                 : 0.F,
                             Rule.contains("dryPerSecond") ? Number(Rule, "dryPerSecond") : 0.F,
                             Scene.contains("filmRewetStartS") ? Number(Scene, "filmRewetStartS") : 0.F});
                    }
                    else
                        throw std::runtime_error("Unsupported local model or state binding: " + Model);
                }
                if (Capacity && Result.Absorption.Rule.empty())
                    throw std::runtime_error("substrateStorage requires an Absorption binding.");
                Result.Liquid.LocalRule =
                    Models.empty() ? Liquid::ELocalMaterialRule::None : Liquid::ELocalMaterialRule::Water;
            }

            std::filesystem::path MaterialTexturePath(const FJson &Appearance, const char *Key)
            {
                const std::filesystem::path Filename = Appearance.at(Key).get<std::string>();
                if (Filename.has_root_path() || Filename.extension() != ".png" ||
                    std::ranges::any_of(Filename, [](const auto &Part) { return Part == ".."; }))
                    throw std::runtime_error(std::string(Key) + " must name a PNG in Textures/SurfaceSimulation.");
                return std::filesystem::path(MDSS_ASSET_DIR) / "Textures" / "SurfaceSimulation" / Filename;
            }

            Rendering::FSnowSurfaceAppearance ReadSnowAppearance(const FJson                                    &Profile,
                                                                 std::span<const Rendering::FStateVisualization> States)
            {
                const auto Appearance = Profile.find("appearance");
                if (Appearance == Profile.end() || !Appearance->contains("snow"))
                    return {};

                const auto &Snow = Appearance->at("snow");
                const auto  Id = Snow.at("state").get<std::string>();
                const auto  State = std::ranges::find(States, Id, &Rendering::FStateVisualization::Id);
                if (State == States.end() || State->Channel != Rendering::ESurfaceStateChannel::IceMass)
                    throw std::runtime_error("Snow appearance must reference a cell-stored ice or snow state.");

                Rendering::FSnowSurfaceAppearance Result;
                Result.BulkDensityKgM3 = Number(Snow, "bulkDensityKgM3", 1.F);
                if (const auto Maps = Snow.find("maps"); Maps != Snow.end())
                {
                    Result.ColorTexture = MaterialTexturePath(*Maps, "colorTexture");
                    Result.NormalTexture = MaterialTexturePath(*Maps, "normalTexture");
                    Result.RepeatsPerMeter = 1.F / Number(*Maps, "tileMeters", 1.e-4F);
                }
                return Result;
            }

            FSubstrateAppearanceSettings ReadSubstrateAppearance(const FJson &Profile, const FJson &Substrate)
            {
                FSubstrateAppearanceSettings Settings;
                Settings.Id = Profile.value("substrate", std::string{});
                if (const auto Optics = Substrate.find("appearance"); Optics != Substrate.end())
                {
                    if (!Optics->is_object())
                        throw std::runtime_error("Substrate appearance must be an object.");
                    auto &Values = Settings.Optics;
                    Settings.bMetallic = Optics->value("metallic", false);
                    if (const auto Depth = Optics->find("wetnessDepth"); Depth != Optics->end())
                    {
                        auto &Wetness = Settings.WetnessDepth;
                        Wetness.InitialContrast = Number(*Depth, "initialContrast");
                        Wetness.DiffusivityM2S = Number(*Depth, "diffusivityM2S", 1.e-10F);
                        if (Wetness.InitialContrast > 0.5F)
                            throw std::runtime_error("Wetness depth initialContrast must be in [0, 0.5].");
                    }
                    if (Optics->contains("dryRoughness"))
                        Values.DryRoughness = Number(*Optics, "dryRoughness");
                    if (Optics->contains("wetRoughness"))
                        Values.WetRoughness = Number(*Optics, "wetRoughness");
                    if (Optics->contains("refractiveIndex"))
                        Values.RefractiveIndex = Number(*Optics, "refractiveIndex", 1.F);
                    if (Optics->contains("specularScale"))
                    {
                        Values.SpecularScale = Number(*Optics, "specularScale");
                        if (Values.SpecularScale > Rendering::MaxSubstrateSpecularScale)
                            throw std::runtime_error("Substrate specularScale must be in [0, 4].");
                    }
                    if (Optics->contains("colorTexture"))
                        Settings.ColorTexture = MaterialTexturePath(*Optics, "colorTexture");
                    if (Optics->contains("normalTexture"))
                        Settings.NormalTexture = MaterialTexturePath(*Optics, "normalTexture");
                    if (Optics->contains("reliefDepthMeters"))
                    {
                        Values.ReliefDepthMeters = Number(*Optics, "reliefDepthMeters");
                        if (Settings.NormalTexture.empty())
                            throw std::runtime_error("reliefDepthMeters requires a height channel in normalTexture.");
                    }
                    if (Optics->contains("normalStrength"))
                        Values.NormalStrength = Number(*Optics, "normalStrength");
                    if (Values.NormalStrength > 2.F)
                        throw std::runtime_error("Substrate normalStrength must be in [0, 2].");
                    if (Values.DryRoughness > 1.F || Values.WetRoughness > 1.F || Values.RefractiveIndex > 4.F)
                        throw std::runtime_error("Substrate roughness must be in [0, 1] and refractiveIndex in [1, 4].");
                }
                return Settings;
            }

            void
            BindProfile(FSurfaceSceneSettings &Result, const FJson &Scene, const FJson &Profile, const FJson &Substrate)
            {
                auto &Settings = Result.Profile.emplace();
                Settings.Substrate = ReadSubstrateAppearance(Profile, Substrate);
                Settings.LiquidState = Profile.at("transport").at("state").get<std::string>();
                std::vector<std::string> LayerIds;
                BindLayerStates(Result, Profile, LayerIds);
                for (const auto &State : Profile.at("states").at("substrate"))
                {
                    BindStateVisualization(Result, State, true);
                    if (Result.States.back().Channel != Rendering::ESurfaceStateChannel::AbsorbedMass)
                        continue;
                    if (State.at("material") != Settings.LiquidMaterial)
                        throw std::runtime_error("Absorption must preserve the material.");
                    Settings.AbsorbedState = State.at("id").get<std::string>();
                }
                Settings.Liquid = Asset::LoadLiquidMaterialById(Settings.LiquidMaterial);
                BindRules(Result, Scene, Profile, LayerIds);
                BindInitialFilm(Result, Scene, LayerIds);
                const auto &IceState = Settings.IceState;
                const auto &AbsorbedState = Settings.AbsorbedState;
                if (const auto Appearance = Profile.find("appearance"); Appearance != Profile.end())
                {
                    if (Appearance->contains("liquidOpticalDepthScale"))
                        Settings.LiquidOpticalDepthScale = Number(*Appearance, "liquidOpticalDepthScale", 1.e-6F);
                    if (Appearance->contains("liquidCoverageDepthMeters"))
                        Settings.LiquidCoverageDepthMeters = Number(*Appearance, "liquidCoverageDepthMeters", 1.e-6F);
                    if (Appearance->contains("liquidTint"))
                    {
                        const auto Tint = Appearance->at("liquidTint").get<std::array<float, 3>>();
                        if (!std::ranges::all_of(Tint, [](float V) { return std::isfinite(V) && V >= 0.F && V <= 1.F; }))
                            throw std::runtime_error("Liquid tint must use finite RGB values in [0, 1].");
                        Result.LiquidTint = {Tint[0], Tint[1], Tint[2], 1.F};
                    }
                    if (Appearance->contains("wetCloth"))
                    {
                        const auto &Cloth = Appearance->at("wetCloth");
                        if (AbsorbedState.empty() || Cloth.at("state") != AbsorbedState)
                            throw std::runtime_error("Wet cloth must reference the bound absorbed water state.");
                        Settings.WetClothReferenceKgM2 = Number(Cloth, "referenceKgM2", 1e-5F);
                    }
                }
                if (const auto Initial = Scene.find("initialLayer"); Initial != Scene.end())
                {
                    if (IceState.empty() || Initial->at("state") != IceState)
                        throw std::runtime_error("initialLayer must reference the bound ice state.");
                    Result.InitialIcePeakKgM2 = Number(*Initial, "peakKgM2");
                    Result.InitialIceCenter = Initial->at("centerUV").get<std::array<float, 2>>();
                    Result.InitialIceInverseRadius = Number(*Initial, "inverseRadius");
                    if (!std::ranges::all_of(Result.InitialIceCenter, [](float V) { return V >= 0.F && V <= 1.F; }))
                        throw std::runtime_error("The initial layer center must lie within the chart.");
                }
            }
        } // namespace
    #pragma endregion

    FSurfaceSceneSettings LoadSceneAsset(const std::filesystem::path &Path)
    {
        try
        {
            auto                  Scene = ReadTypedAsset(Path, "SurfaceScene");
            FSurfaceSceneSettings Result;
            const auto            Geometry = Scene.at("geometry").get<std::string>();
            const auto            GeometryKey =
                Geometry == "PairedFlatSamples" ? std::string_view("MaterialSpecimens") : std::string_view(Geometry);
            const auto Entry = std::ranges::find(SceneAssets, GeometryKey, &FSceneAssetEntry::Geometry);
            if (Entry == SceneAssets.end())
                throw std::runtime_error("Unknown procedural geometry: " + Geometry);
            Result.Scene = static_cast<EScene>(Entry - SceneAssets.begin());
            Result.AssetPath = Path;
            ReadSceneResolution(Result, Scene);
            Result.bSplitSubgridBasins = Scene.value("splitSubgridBasins", true);
            if (const auto Substeps = Scene.find("transportSubsteps"); Substeps != Scene.end())
            {
                if (!Substeps->is_number_integer() ||
                    (*Substeps != 1 && *Substeps != 2 && *Substeps != 4 && *Substeps != 8))
                    throw std::runtime_error("transportSubsteps must be 1, 2, 4, or 8.");
                Result.TransportSubsteps = Substeps->get<int>();
            }
            Result.bSmoothDisplay = Scene.at("smoothDisplay").get<bool>();
            Result.SizeM = Scene.at("sizeM").get<std::array<float, 2>>();
            if (!std::ranges::all_of(Result.SizeM, [](float M) { return std::isfinite(M) && M > 0.F; }))
                throw std::runtime_error("Chart dimensions must be finite and positive.");
            if (Scene.contains("surfaceTexture"))
                Result.SurfaceTexturePath = SceneTexturePath(Path, Scene, "surfaceTexture");
            if (Scene.contains("surfaceNormal"))
                Result.SurfaceNormalPath = SceneTexturePath(Path, Scene, "surfaceNormal");
            if ((Scene.contains("surfaceTexture") || Scene.contains("surfaceNormal")) && Scene.contains("chartAlbedo"))
                throw std::runtime_error("Choose a repeating surface texture or chartAlbedo for the scene.");
            if (const auto Environment = Scene.find("environment"); Environment != Scene.end())
            {
                Result.Environment.TexturePath = SceneTexturePath(Path, *Environment, "texture", ".hdr");
                if (Environment->contains("exposure"))
                {
                    Result.Environment.Exposure = Number(*Environment, "exposure", 1.e-6F);
                    if (Result.Environment.Exposure > 16.F)
                        throw std::runtime_error("Environment exposure must be no greater than 16.");
                }
                if (Environment->contains("yawDegrees"))
                {
                    const float Degrees = Number(*Environment, "yawDegrees", std::numeric_limits<float>::lowest());
                    Result.Environment.YawRadians = glm::radians(std::remainder(Degrees, 360.F));
                }
            }
            if (Scene.contains("chartAlbedo"))
                Result.ChartAlbedoPath = SceneTexturePath(Path, Scene, "chartAlbedo");
            if (Scene.contains("chartNormal"))
            {
                if (Result.ChartAlbedoPath.empty())
                    throw std::runtime_error("chartNormal shares the UV projection of chartAlbedo.");
                Result.ChartNormalPath = SceneTexturePath(Path, Scene, "chartNormal");
            }
            if (Scene.contains("bodyThicknessM"))
                Result.BodyThicknessMeters = Number(Scene, "bodyThicknessM", 1.e-6F);
            if (Scene.contains("wetnessRedistributionStartSeconds"))
                Result.WetnessRedistributionStartSeconds = Number(Scene, "wetnessRedistributionStartSeconds");
            if (const auto Window = Scene.find("sectionWindow"); Window != Scene.end())
            {
                const float Front = Number(*Window, "frontV");
                const float Height = Number(*Window, "heightM", 1.e-5F);
                const float Graduation = Number(*Window, "graduationM", 1.e-5F);
                if (Front >= 1.F || Graduation > Height || Result.Scene == EScene::AbsorptionComparison)
                    throw std::runtime_error(
                        "sectionWindow requires frontV below 1 and graduations within a single chart.");
                auto &Section = Result.SectionWindow;
                Section = {Front, Height, Graduation};
                if (const auto Vessel = Window->find("glassVessel"); Vessel != Window->end())
                {
                    const auto  UV = Vessel->at("interiorUV").get<std::array<float, 4>>();
                    const float Wall = Number(*Vessel, "thicknessM", 1.e-5F);
                    const auto  FitsAxis = [&](int Axis)
                    {
                        const float Margin = Wall / Result.SizeM[Axis];
                        return UV[Axis] >= Margin && UV[Axis + 2] <= 1.F - Margin && UV[Axis] < UV[Axis + 2];
                    };
                    if (!FitsAxis(0) || !FitsAxis(1) || Wall >= Height)
                        throw std::runtime_error("The glass vessel bounds and thickness must fit inside the chart.");
                    Section.VesselInteriorUV = {UV[0], UV[1], UV[2], UV[3]};
                    Section.GlassThicknessMeters = Wall;
                }
                if (const auto Bed = Window->find("solidBedRangeU"); Bed != Window->end())
                {
                    const auto U = Bed->get<std::array<float, 2>>();
                    if (!(U[0] >= 0.F && U[0] < U[1] && U[1] <= 1.F) || Section.GlassThicknessMeters > 0.F)
                        throw std::runtime_error(
                            "solidBedRangeU requires an ordered chart interval on a section window.");
                    Section.SolidBedRangeU = {U[0], U[1]};
                }
            }
            if (Scene.contains("comparisonGapM"))
            {
                if (Result.Scene != EScene::AbsorptionComparison)
                    throw std::runtime_error("comparisonGapM requires a material specimen scene.");
                Result.ComparisonGapMeters = Number(Scene, "comparisonGapM");
            }
            if (const auto Basin = Scene.find("shallowBasin"); Basin != Scene.end())
            {
                const auto Center = Basin->at("centerUV").get<std::array<float, 2>>();
                if (!std::ranges::all_of(Center, [](float V) { return V >= 0.F && V <= 1.F; }))
                    throw std::runtime_error("The basin center must lie within the chart.");
                Result.ShallowBasin =
                    FShallowBasin{Center, Number(*Basin, "radiusM", 1.e-6F), Number(*Basin, "depthM")};
            }
            if (const auto Relief = Scene.find("surfaceRelief"); Relief != Scene.end())
            {
                const auto Cycles = Relief->at("cycles").get<std::array<float, 2>>();
                if (!std::ranges::all_of(Cycles, [](float Value) { return std::isfinite(Value) && Value > 0.F; }))
                    throw std::runtime_error("Surface relief needs finite positive cycle counts.");
                Result.SurfaceRelief = FSurfaceRelief{Number(*Relief, "amplitudeM"), Cycles};
            }
            Result.bOpenOutlet = Scene.value("openOutlet", false);
            Result.bStartPaused = Scene.value("startPaused", false);
            if (Scene.contains("initialPlaybackRate"))
            {
                const float Rate = Number(Scene, "initialPlaybackRate");
                if (std::ranges::find(PlaybackRates, Rate) == PlaybackRates.end())
                    throw std::runtime_error("initialPlaybackRate must be 0.05, 0.1, 0.25, 0.5, 1, 2, or 4.");
                Result.InitialPlaybackRate = Rate;
            }
            Result.PourKg = Number(Scene, "pourKg");
            if (const auto Inlet = Scene.find("scheduledInlet"); Inlet != Scene.end())
            {
                Result.ScheduledInlet = Liquid::FInletFootprint{Inlet->at("centerM").get<std::array<float, 2>>(),
                                                                Inlet->at("sizeM").get<std::array<float, 2>>()};
                for (std::size_t Axis = 0; Axis < Result.SizeM.size(); ++Axis)
                {
                    const float Center = Result.ScheduledInlet->CenterM[Axis];
                    const float Extent = Result.ScheduledInlet->SizeM[Axis];
                    if (!std::isfinite(Center) || !std::isfinite(Extent) || Extent <= 0.F ||
                        2.0 * std::abs(Center) >= static_cast<double>(Result.SizeM[Axis]) + Extent)
                        throw std::runtime_error(
                            "scheduledInlet must have a finite center, positive size and overlap the chart.");
                }
            }
            Result.MaxTiltRadians = Number(Scene, "maxTiltRadians");
            if (Scene.contains("pourStartSeconds"))
                Result.PourStartSeconds = Number(Scene, "pourStartSeconds");
            if (Scene.contains("pourSeconds"))
                Result.PourSeconds = Number(Scene, "pourSeconds", 1.e-6F);
            if (Scene.contains("initialLiquidKg"))
                Result.InitialLiquidKg = Number(Scene, "initialLiquidKg");
            const auto *MassScale = Scene.contains("snowMassScale") ? "snowMassScale" : "iceMassScale";
            if (Scene.contains(MassScale))
            {
                if (Result.Scene != EScene::IceBed)
                    throw std::runtime_error("Frozen water mass scaling requires a glacial bed.");
                Result.IceMassScale = Number(Scene, MassScale, 1.e-6F);
            }
            if (Scene.contains("contactHeatFadeSeconds"))
            {
                if (Result.Scene != EScene::IceBed)
                    throw std::runtime_error("Contact heat timing requires the glacial heat source.");
                Result.ContactHeatFadeSeconds = Number(Scene, "contactHeatFadeSeconds", 1.e-6F);
            }
            if (Scene.contains("geometryCatalog") || Scene.contains("geometrySet") ||
                Scene.contains("geometrySource") || Scene.contains("normalGeometry"))
                throw std::runtime_error("Declare authored surface geometry directly with heightMap.");

            if (Scene.contains("heightMap") && Scene.contains("geometryInput"))
                throw std::runtime_error("Choose one authored geometry input.");
            if (const auto Height = Scene.find("heightMap"); Height != Scene.end())
            {
                Result.Source = SurfaceState::EHeightfieldSource::HeightMap;
                Result.Path = SceneTexturePath(Path, *Height, "texture");
                Result.Scale = Number(*Height, "heightRangeM", 1.e-6F);
                if (Height->contains("heightOffsetM"))
                    Result.HeightOffsetM = Number(*Height, "heightOffsetM", std::numeric_limits<float>::lowest());
            }
            else if (const auto Input = Scene.find("geometryInput"); Input != Scene.end())
            {
                using enum SurfaceState::EHeightfieldSource;
                const auto Type = Input->at("type").get<std::string>();
                if (Type == "Mesh")
                    Result.Source = Mesh;
                else if (Type == "NormalMap")
                    Result.Source = NormalMap;
                else if (Type == "DisplacementMap")
                    Result.Source = DisplacementMap;
                else
                    throw std::runtime_error("geometryInput needs Mesh, NormalMap or DisplacementMap.");
                Result.Path = SceneTexturePath(Path, *Input, "path", Result.Source == Mesh ? ".obj" : ".png");
                Result.Scale = Number(*Input, "scale", 1.e-6F);
            }
            else if (Result.Scene == EScene::Cavity)
                throw std::runtime_error("A CavityChart scene needs authored geometry.");
            Result.ProfileId = Scene.at("surfaceProfile").get<std::string>();
            const auto Profile = ReadSharedAsset("Profiles", Result.ProfileId, "SurfaceProfile");
            const auto Substrate = Profile.contains("substrate")
                                       ? ReadSharedAsset("Materials", Profile.at("substrate"), "PhysicalMaterial")
                                       : FJson::object();
            if (const auto Appearance = Substrate.find("appearance"); Appearance != Substrate.end())
                Result.bSubstrateThermalEmission = Appearance->value("thermalEmission", false);
            if (Profile.contains("transport"))
                BindProfile(Result, Scene, Profile, Substrate);
            else
            {
                if (Result.Scene == EScene::Runoff || Result.Scene == EScene::Textile || Scene.contains("initialLayer"))
                    throw std::runtime_error("This scene requires transport bindings for its authored layers.");
                const auto &Interactions = Profile.at("interactions");
                if (!Interactions.is_array() || !Interactions.empty())
                    throw std::runtime_error("A procedural profile must have an empty interactions list.");
                Result.ProceduralSubstrate = ReadSubstrateAppearance(Profile, Substrate);
                const auto &States = Profile.at("states");
                BindProfileStateGroup(Result, States.at("layer"), false);
                BindProfileStateGroup(Result, States.at("substrate"), true);
            }
            Result.Snow = ReadSnowAppearance(Profile, Result.States);

            if (Result.Scene == EScene::AbsorptionComparison || Scene.contains("comparisonSurfaceProfile") ||
                Scene.contains("comparisonSurfaceProfiles"))
            {
                if (Result.Scene != EScene::AbsorptionComparison || !Result.Profile ||
                    (Scene.contains("comparisonSurfaceProfile") && Scene.contains("comparisonSurfaceProfiles")))
                    throw std::runtime_error(
                        "A substrate comparison needs a primary profile and one list of specimen profiles.");
                const auto  Ids = Scene.contains("comparisonSurfaceProfile")
                                      ? FJson::array({Scene.at("comparisonSurfaceProfile")})
                                      : Scene.value("comparisonSurfaceProfiles", FJson::array());
                const auto &Primary = *Result.Profile;
                if (!Ids.is_array() || Ids.size() >= Rendering::MaxSurfaceSpecimens ||
                    Primary.Absorption.Rule.empty() || !Primary.Melt.Rule.empty() || !Primary.Drying.Rule.empty())
                    throw std::runtime_error("Invalid absorption comparison or specimen count.");

                std::unordered_set<std::string> Substrates{Primary.Substrate.Id};
                std::unordered_set<float>       Capacities{Primary.Cell.CapacityKgM2};
                float                           Maximum = Primary.Cell.CapacityKgM2;
                for (const auto &Id : Ids)
                {
                    FSurfaceSceneSettings Other;
                    const auto SpecimenProfile = ReadSharedAsset("Profiles", Id.get<std::string>(), "SurfaceProfile");
                    const auto SpecimenMaterial =
                        ReadSharedAsset("Materials", SpecimenProfile.at("substrate"), "PhysicalMaterial");
                    BindProfile(Other, Scene, SpecimenProfile, SpecimenMaterial);
                    const auto &Specimen = *Other.Profile;
                    auto        SharedCell = Specimen.Cell;
                    SharedCell.CapacityKgM2 = Primary.Cell.CapacityKgM2;
                    if (Primary.Absorption != Specimen.Absorption || Primary.Cell != SharedCell ||
                        Primary.Liquid != Specimen.Liquid || Result.States != Other.States ||
                        !Specimen.Melt.Rule.empty() || !Specimen.Drying.Rule.empty() ||
                        !Substrates.insert(Specimen.Substrate.Id).second ||
                        !Capacities.insert(Specimen.Cell.CapacityKgM2).second)
                        throw std::runtime_error("Comparison specimens need shared absorption inputs and distinct "
                                                 "substrates and capacities.");
                    Maximum = std::max(Maximum, Specimen.Cell.CapacityKgM2);
                    Result.ComparisonProfiles.push_back(std::move(*Other.Profile));
                }
                for (auto &State : Result.States)
                    if (State.Channel == Rendering::ESurfaceStateChannel::AbsorbedMass)
                        State.DisplayMaximum = Maximum;
            }

            if (const auto Default = Scene.find("defaultSubstrateViewState"); Default != Scene.end())
            {
                Result.DefaultSubstrateViewState = Default->get<std::string>();
                if (std::ranges::none_of(Result.States,
                                         [&](const auto &State)
                                         { return State.bSubstrate && State.Id == Result.DefaultSubstrateViewState; }))
                    throw std::runtime_error("defaultSubstrateViewState must name a registered substrate state.");
            }

            const auto Heating = Scene.find("substrateEnergyPulse");
            const auto Field = Scene.find("substrateTemperatureField");
            if (Heating != Scene.end() || Field != Scene.end())
            {
                if (Scene.contains("substrateQuench") || (Heating != Scene.end() && Field != Scene.end()))
                    throw std::runtime_error("A scene may bind only one authored substrate temperature source.");
                if (std::ranges::none_of(Result.States,
                                         [](const auto &State)
                                         { return State.Channel == Rendering::ESurfaceStateChannel::Temperature; }))
                    throw std::runtime_error("A substrate temperature source requires a temperature state.");
                if (Heating != Scene.end())
                {
                    const float Energy = Number(*Heating, "energyJm2", 0.F);
                    const float Target = Liquid::AmbientTemperatureK + Energy / ArealHeatCapacity(Profile);
                    if (!std::isfinite(Target))
                        throw std::runtime_error("The applied energy must yield a finite temperature.");
                    if (std::ranges::any_of(Result.States, [](const auto &State) { return !State.bSubstrate; }) ||
                        Result.PourKg > 0.F || Result.InitialLiquidKg > 0.F)
                        throw std::runtime_error(
                            "Dry energy-pulse heating requires a substrate-only profile and no liquid input.");
                    Result.TemperatureRamp =
                        FTemperatureRamp{{Target, Target}, Number(*Heating, "durationS", 1.e-6F), true};
                }
                else
                    Result.TemperatureRamp =
                        FTemperatureRamp{{Number(*Field, "u0K", 1.e-6F), Number(*Field, "u1K", 1.e-6F)},
                                         Number(*Field, "rampSeconds", 1.e-6F)};
            }
            if (const auto Quench = Scene.find("substrateQuench"); Quench != Scene.end())
            {
                using enum Rendering::ESurfaceStateChannel;
                if (Result.States.size() != 2 || Result.States[0].Channel != LiquidMass ||
                    Result.States[1].Channel != Temperature ||
                    Profile.at("states").at("layer").front().at("material") != "Water")
                    throw std::runtime_error(
                        "substrateQuench requires surface water and substrate temperature states.");
                const auto Center = Quench->at("centerUV").get<std::array<float, 2>>();
                const auto Radius = Quench->at("radiusUV").get<std::array<float, 2>>();
                if (!std::ranges::all_of(Center, [](float V) { return V >= 0.F && V <= 1.F; }) ||
                    !std::ranges::all_of(Radius, [](float V) { return std::isfinite(V) && V > 0.F; }))
                    throw std::runtime_error("substrateQuench needs a center inside the chart and positive radii.");
                const float HeatCapacity = Quench->contains("arealHeatCapacityJm2K")
                                               ? Number(*Quench, "arealHeatCapacityJm2K", 1.F)
                                               : ArealHeatCapacity(Profile);
                const auto Interaction = Quench->contains("interaction")
                                             ? ReadSharedAsset("Interactions", Quench->at("interaction"), "Interaction")
                                             : *Quench;
                if (Quench->contains("interaction") && Interaction.at("model") != "SubstrateQuench")
                    throw std::runtime_error("A substrate quench requires the SubstrateQuench interaction model.");
                Result.SubstrateQuench =
                    FSubstrateQuenchSettings{Center,
                                             Radius,
                                             Number(*Quench, "peakTemperatureK", Liquid::AmbientTemperatureK),
                                             {HeatCapacity, Number(Interaction, "contactConductanceWm2K")}};
            }
            return Result;
        }
        catch (const std::exception &Error)
        {
            throw std::runtime_error(Path.string() + ": " + Error.what());
        }
    }

    FSurfaceSceneSettings Defaults(EScene Scene)
    {
        auto Settings = LoadSceneAsset(BundledScenePath(Scene));
        if (Settings.Scene != Scene)
            throw std::runtime_error(Settings.AssetPath.string() + ": Geometry does not match the catalog entry.");
        return Settings;
    }

    std::filesystem::path BundledScenePath(EScene Scene)
    {
        return std::filesystem::path(MDSS_ASSET_DIR) / "Scenes" / SceneAssets.at(static_cast<std::size_t>(Scene)).File;
    }

    std::vector<std::filesystem::path> BundledSceneVariants(EScene Scene)
    {
        const auto                         Base = BundledScenePath(Scene);
        const auto                         Prefix = Base.stem().string() + '_';
        std::vector<std::filesystem::path> Paths;
        for (const auto &Entry : std::filesystem::directory_iterator(Base.parent_path()))
        {
            const auto &Path = Entry.path();
            const auto  Stem = Path.stem().string();
            if (Entry.is_regular_file() && Path.extension() == Base.extension() && Stem.starts_with(Prefix) &&
                Stem.size() > Prefix.size())
                Paths.push_back(Path);
        }
        std::ranges::sort(Paths);
        return Paths;
    }

    // ============================================================
    // 02. Side-by-side Scene Assets
    // ============================================================
    #pragma region 02_Side_by_Side_Scene_Assets
        std::optional<FSceneComparison> ReadSceneComparison(const std::filesystem::path &Path)
        {
            std::ifstream Input(Path);
            const auto    Root = FJson::parse(Input, nullptr, false);
            if (!Root.is_object() || !Root.contains("type") || Root["type"] != "SurfaceComparison")
                return std::nullopt;
            try
            {
                if (!Root.at("version").is_number_integer() || Root["version"] != 1)
                    throw std::runtime_error("Expected SurfaceComparison v1.");
                const auto &Cases = Root.at("cases");
                if (!Cases.is_array() || Cases.size() < 2 || Cases.size() > MaxSceneComparisonCases)
                    throw std::runtime_error("A comparison requires two to four cases.");
                FSceneComparison Result;
                Result.Path = std::filesystem::absolute(Path).lexically_normal();
                Result.Title = Root.at("title").get<std::string>();
                Result.KoreanTitle = Root.at("koreanTitle").get<std::string>();
                Result.Description = Root.at("description").get<std::string>();
                if (Result.Title.empty())
                    throw std::runtime_error("A comparison needs a title.");
                for (const auto &Case : Cases)
                {
                    const std::filesystem::path Relative = Case.at("scene").get<std::string>();
                    if (Relative.has_root_path() || Relative.extension() != ".Scene")
                        throw std::runtime_error("A case must reference a relative .Scene path.");
                    auto Scene = (Result.Path.parent_path() / Relative).lexically_normal();
                    auto Label = Case.at("label").get<std::string>();
                    if (Label.empty() || std::ranges::find(Result.Labels, Label) != Result.Labels.end() ||
                        std::ranges::find(Result.Scenes, Scene) != Result.Scenes.end())
                        throw std::runtime_error("A comparison needs distinct scene paths and nonempty labels.");
                    Result.Scenes.push_back(std::move(Scene));
                    Result.Labels.push_back(std::move(Label));
                }
                return Result;
            }
            catch (const std::exception &Error)
            {
                throw std::runtime_error(Path.string() + ": " + Error.what());
            }
        }

        std::vector<FSurfaceSceneSettings> LoadComparisonCases(const FSceneComparison &Comparison)
        {
            std::vector<FSurfaceSceneSettings> Settings;
            Settings.reserve(Comparison.Scenes.size());
            for (const auto &Scene : Comparison.Scenes)
            {
                auto Case = LoadSceneAsset(Scene);
                if ((!Settings.empty() && Case.Scene != Settings.front().Scene) || !Case.ComparisonProfiles.empty())
                    throw std::runtime_error(Comparison.Path.string() +
                                             ": Comparison cases must be independent scenes from one geometry family.");
                Settings.push_back(std::move(Case));
            }
            return Settings;
        }

    #pragma endregion

    bool IsSurfaceSceneAsset(const std::filesystem::path &Path)
    {
        if (Path.empty() || Path.extension() != ".Scene")
            return false;
        std::ifstream Input(Path);
        if (!Input)
            return false;
        const auto Root = FJson::parse(Input, nullptr, false);
        return Root.is_object() && Root.contains("type") &&
               (IsSurfaceSceneType(Root["type"]) || Root["type"] == "SurfaceComparison");
    }
} // namespace MDSS::SurfaceSimulation
