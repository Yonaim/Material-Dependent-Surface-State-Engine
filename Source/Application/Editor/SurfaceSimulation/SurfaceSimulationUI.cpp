/**
 * @file SurfaceSimulationUI.cpp
 * @brief Surface Simulation 조작 패널을 그리고 실험 리포트의 그림 생성을 요청한다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSimulationUI.h"
#include "Application/Editor/SurfaceSimulation/SceneAsset.h"
#include "AssetManager/Loaders/LiquidMaterialLoader.h"
#include "DebugUI/Panels/EditorLayoutStyle.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <span>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

namespace MDSS::SurfaceSimulation
{
    // ============================================================
    // 00. Lab Controls
    // ============================================================
#pragma region 00_Lab_Controls
    // 실행 루프가 적용할 설정 변경과 실험 명령을 화면에서 선택한다.

    namespace
    {
        // ============================================================
        // 00.00. Setting Rows
        // ============================================================
    #pragma region 00_00_Setting_Rows
        // 위치를 공유하는 라벨과 입력에 서로 다른 ImGui ID를 준다.
        template <typename FDrawControl>
        void DrawSettingRow(const char *Label, FDrawControl DrawControl, const char *Tooltip = nullptr)
        {
            ImGui::PushID(Label);
            if (ImGui::BeginTable("##SettingRow", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 0.43F);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.57F);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextWrapped("%s", Label);
                if (Tooltip)
                    ImGui::SetItemTooltip("%s", Tooltip);
                ImGui::TableSetColumnIndex(1);
                ImGui::AlignTextToFramePadding();
                ImGui::SetNextItemWidth(-FLT_MIN);
                DrawControl();
                if (Tooltip)
                    ImGui::SetItemTooltip("%s", Tooltip);
                ImGui::EndTable();
            }
            ImGui::PopID();
        }

        void DrawSettingToggle(const char *Label, bool &Enabled, const char *Tooltip)
        {
            DrawSettingRow(Label, [&] { ImGui::Checkbox("##Enabled", &Enabled); }, Tooltip);
        }
    #pragma endregion

        // ============================================================
        // 00.01. Asset Editors
        // ============================================================
    #pragma region 00_01_Asset_Editors
        // 파일별 폼은 실행 설정에 연결된 값만 세션 초안에서 변경한다.

        bool EditClampedValue(float &Value, float Minimum, float Maximum)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            float      Edited = Value;
            const bool bChanged = ImGui::InputFloat("##value", &Edited, 0.F, 0.F, "%.6g") && std::isfinite(Edited);
            if (bChanged)
                Value = std::clamp(Edited, Minimum, Maximum);
            return bChanged;
        }

        const char *Utf8(const char8_t *Text)
        {
            return reinterpret_cast<const char *>(Text);
        }

        void DrawAssetTooltip(const char8_t *Description, const char *Filename = nullptr)
        {
            if (!ImGui::BeginItemTooltip())
                return;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * 28.F);
            ImGui::TextWrapped("%s", Utf8(Description));
            if (Filename)
                ImGui::TextWrapped("%s", Filename);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        void BeginPropertyRow(const char *Label, const char8_t *Description)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped("%s", Label);
            DrawAssetTooltip(Description);
            ImGui::TableSetColumnIndex(1);
        }

        bool EditAssetValue(const char    *Label,
                            const char8_t *Description,
                            float         &Value,
                            float          Minimum = 0.F,
                            float          Maximum = FLT_MAX)
        {
            BeginPropertyRow(Label, Description);
            ImGui::PushID(Label);
            const bool bChanged = EditClampedValue(Value, Minimum, Maximum);
            DrawAssetTooltip(Description);
            ImGui::PopID();
            return bChanged;
        }

        void DrawAssetValue(const char *Label, const char8_t *Description, const char *Value)
        {
            BeginPropertyRow(Label, Description);
            ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped("%s", Value);
            DrawAssetTooltip(Description);
        }

        template <typename FDrawRows>
        void DrawAssetSection(const char *Title, FDrawRows DrawRows)
        {
            EditorLayout::SectionTitle(Title, EditorLayout::SectionGap, 3.F);
            if (!ImGui::BeginTable(Title, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
                return;

            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.62F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.38F);
            DrawRows();
            ImGui::EndTable();
        }

        void DrawProceduralUpdates(FSurfaceSceneSettings &Draft, const FProceduralUpdateRates &Defaults)
        {
            auto Rates = Draft.ProceduralUpdateOverride.value_or(Defaults);
            bool bChanged = false;
            if (Defaults.MeltKgM2SecondK > 0.F)
                bChanged |= EditAssetValue("Melt coefficient [kg/(m^2 s K)]",
                                           u8"어는점보다 따뜻할 때 눈과 얼음이 녹는 속도를 조절합니다.",
                                           Rates.MeltKgM2SecondK);
            if (Defaults.FreezeKgM2SecondK > 0.F)
                bChanged |= EditAssetValue(
                    "Freeze coefficient [kg/(m^2 s K)]",
                    u8"동결 계수입니다. 어는점보다 낮은 온도에서 물이 얼음으로 바뀌는 속도를 조절합니다.",
                    Rates.FreezeKgM2SecondK);
            if (Defaults.EvaporateKgM2SecondK > 0.F)
                bChanged |= EditAssetValue("Evaporation coefficient [kg/(m^2 s K)]",
                                           u8"증발 계수입니다. 뜨거운 구간에서 액체 질량이 줄어드는 속도를 조절합니다.",
                                           Rates.EvaporateKgM2SecondK);
            if (Defaults.CharPerSecond > 0.F)
                bChanged |= EditAssetValue(
                    "Char rate [1/s]",
                    u8"탄화율 증가 계수입니다. 가열 조건에서 재질이 타는 정도가 얼마나 빨리 늘어나는지 정합니다.",
                    Rates.CharPerSecond);
            if (bChanged)
                Draft.ProceduralUpdateOverride = Rates;
        }

        enum class EAssetKind
        {
            Profile,
            Liquid,
            Substrate,
            LayerState,
            SubstrateState,
            Interaction,
            Scene
        };

        struct FAssetEntry
        {
            EAssetKind  Kind;
            std::string File;
            std::size_t Index = 0;
        };

        enum class ELocalUpdateSlot
        {
            Melt,
            Absorption,
            Drying
        };
        constexpr std::array BoundUpdates{&FSurfaceProfileSettings::Melt,
                                          &FSurfaceProfileSettings::Absorption,
                                          &FSurfaceProfileSettings::Drying};

        std::string_view AssetStem(const FAssetEntry &Asset)
        {
            if (Asset.Kind == EAssetKind::LayerState || Asset.Kind == EAssetKind::SubstrateState)
                return Asset.File;
            return std::string_view(Asset.File).substr(0, Asset.File.find_last_of('.'));
        }

        std::string AssetTitle(const FAssetEntry &Asset, FSceneAssetText SceneText)
        {
            return std::string(Asset.Kind == EAssetKind::Scene && !SceneText.Title.empty() ? SceneText.Title
                                                                                           : AssetStem(Asset));
        }

        std::vector<FAssetEntry> SceneAssets(const FSurfaceSceneSettings &Draft)
        {
            using enum EAssetKind;
            std::vector<FAssetEntry> Assets;
            const auto              *Settings = Draft.Profile ? &*Draft.Profile : nullptr;
            if (!Draft.ProfileId.empty())
                Assets.push_back({Profile, Draft.ProfileId + ".SurfaceProfile"});
            const auto Filename = [](const std::filesystem::path &Path)
            {
                const auto Name = Path.filename().u8string();
                return std::string(Utf8(Name.c_str()));
            };
            const auto Material = !Draft.LiquidMaterialPath.empty() ? Filename(Draft.LiquidMaterialPath)
                                  : Settings                        ? Settings->LiquidMaterial + ".PhysicalMaterial"
                                                                    : "Water.PhysicalMaterial";
            Assets.push_back({Liquid, Material});
            const auto &SubstrateId = Settings ? Settings->Substrate.Id : Draft.ProceduralSubstrate.Id;
            if (!SubstrateId.empty())
                Assets.push_back({Substrate, SubstrateId + ".PhysicalMaterial"});
            for (std::size_t Index = 0; Index < Draft.States.size(); ++Index)
            {
                const auto &State = Draft.States[Index];
                Assets.push_back({State.bSubstrate ? SubstrateState : LayerState, State.Id, Index});
            }
            if (Settings)
            {
                for (std::size_t Index = 0; Index < BoundUpdates.size(); ++Index)
                {
                    const auto &Rule = Settings->*BoundUpdates[Index];
                    if (!Rule.Rule.empty())
                        Assets.push_back({Interaction, Rule.Rule + ".Interaction", Index});
                }
            }
            Assets.push_back({Scene, Filename(Draft.AssetPath)});
            return Assets;
        }

        void DrawMaterialProperties(FSurfaceSceneSettings         &Draft,
                                    const FSurfaceSceneSettings   &Active,
                                    const Liquid::FLiquidMaterial &ActiveLiquid)
        {
            auto       Material = Draft.LiquidOverride.value_or(ActiveLiquid);
            const bool bHasIce = std::ranges::any_of(
                Draft.States,
                [](const auto &State) { return State.Channel == Rendering::ESurfaceStateChannel::IceMass; });
            bool bChanged = false;
            DrawAssetSection(
                "Physics",
                [&]
                {
                    bChanged |= EditAssetValue(
                        "Liquid density [kg/m^3]",
                        u8"액체 밀도입니다. 같은 질량일 때 밀도가 높을수록 계산에 사용하는 액체 부피가 작아집니다.",
                        Material.DensityKgM3,
                        1.e-6F);
                    bChanged |= EditAssetValue(
                        "Dynamic viscosity [Pa s]",
                        u8"동적 점도입니다. 같은 조건에서 값이 높을수록 액체의 흐름에 대한 저항이 커집니다.",
                        Material.DynamicViscosityPaS,
                        1.e-9F);
                    if (bHasIce)
                        bChanged |= EditAssetValue(
                            "Ice density [kg/m^3]",
                            u8"고체 얼음의 밀도입니다. 얼음 질량을 부피와 표시 두께로 환산할 때 사용합니다.",
                            Material.IceDensityKgM3,
                            1.e-6F);
                });
            DrawAssetSection(
                "Appearance",
                [&]
                {
                    bChanged |=
                        EditAssetValue("Liquid roughness",
                                       u8"액체 표면의 거칠기입니다. 낮을수록 반사가 선명하고 높을수록 넓게 퍼집니다.",
                                       Material.Roughness,
                                       0.F,
                                       1.F);
                    bChanged |= EditAssetValue(
                        "Liquid refractive index",
                        u8"액체의 굴절률입니다. 수면을 통과하는 빛의 휘어짐과 수면 반사 계산에 쓰입니다.",
                        Material.RefractiveIndex,
                        1.F,
                        4.F);
                    if (bHasIce)
                    {
                        bChanged |=
                            EditAssetValue("Ice roughness",
                                           u8"얼음 표면의 광학 거칠기입니다. 낮을수록 얼음에 비친 반사가 선명해집니다.",
                                           Material.IceRoughness,
                                           0.F,
                                           1.F);
                        bChanged |=
                            EditAssetValue("Frost roughness",
                                           u8"서리가 낀 부분의 광학 거칠기입니다. 값이 높을수록 반사가 넓게 퍼집니다.",
                                           Material.FrostRoughness,
                                           0.F,
                                           1.F);
                        bChanged |= EditAssetValue(
                            "Ice refractive index",
                            u8"얼음의 굴절률입니다. 얼음에서 빛이 굴절하고 반사하는 정도를 계산할 때 씁니다.",
                            Material.IceRefractiveIndex,
                            1.F,
                            4.F);
                    }
                });
            if (bChanged)
                Draft.LiquidOverride = Material == ActiveLiquid ? Active.LiquidOverride : std::optional{Material};
        }

        void DrawStateProperties(Rendering::FStateVisualization &State)
        {
            if (State.HasArealMass())
                EditAssetValue(
                    "Areal mass color maximum [kg/m^2]",
                    u8"면밀도 히트맵에서 색이 최대 강도에 도달하는 값입니다. 계산된 질량 자체는 그대로입니다.",
                    State.DisplayMaximum,
                    1.e-6F);
            else if (State.Channel == Rendering::ESurfaceStateChannel::Temperature)
                EditAssetValue("Temperature color maximum [K]",
                               u8"온도 히트맵의 색상 상한입니다. 시뮬레이션 온도 자체는 그대로입니다.",
                               State.DisplayMaximum,
                               1.e-6F);
            else
                EditAssetValue("Progress color maximum",
                               u8"탄화 진행도를 히트맵에서 최대 색상 강도로 표시하는 기준입니다.",
                               State.DisplayMaximum,
                               1.e-6F);
            if (State.HasLayerThickness())
                EditAssetValue("Thickness color maximum [mm]",
                               u8"두께 히트맵에서 색이 최대 강도에 도달하는 두께입니다. 0이면 상태의 질량 범위와 "
                               u8"밀도에서 계산합니다.",
                               State.ThicknessDisplayMaximumMm);
        }

        void DrawBoundUpdate(FSurfaceProfileSettings &Profile, std::size_t Index)
        {
            const auto &Binding = Profile.*BoundUpdates[Index];
            DrawAssetSection(
                "Bindings",
                [&]
                {
                    DrawAssetValue("From", u8"이 규칙이 변환하거나 줄이는 원래 상태입니다.", Binding.From.c_str());
                    DrawAssetValue(
                        "To",
                        u8"변환된 상태를 저장할 대상입니다. Environment는 질량이 환경으로 빠져나간다는 뜻입니다.",
                        Binding.To.empty() ? "Environment" : Binding.To.c_str());
                    if (!Binding.Field.empty())
                        DrawAssetValue("Input field",
                                       u8"이 규칙에 온도나 배수 등 장면별 입력을 제공하는 필드입니다.",
                                       Binding.Field.c_str());
                });
            DrawAssetSection("Physics",
                             [&]
                             {
                                 if (Index == static_cast<std::size_t>(ELocalUpdateSlot::Melt))
                                     EditAssetValue("Melt coefficient [kg/(m^2 s K)]",
                                                    u8"어는점보다 따뜻할 때 눈과 얼음이 녹는 속도를 조절합니다.",
                                                    Profile.Cell.MeltKgM2SecondK);
                                 else if (Index == static_cast<std::size_t>(ELocalUpdateSlot::Absorption))
                                 {
                                     EditAssetValue("Absorption rate [1/s]",
                                                    u8"표면의 물이 바탕으로 스며드는 속도를 조절합니다.",
                                                    Profile.Cell.AbsorbPerSecond);
                                 }
                                 else
                                     EditAssetValue("Drying rate [1/s]",
                                                    u8"바탕에 흡수된 물이 건조로 줄어드는 속도를 조절합니다.",
                                                    Profile.Cell.DryPerSecond);
                             });
        }

        void DrawSceneFields(FSurfaceSceneSettings &Draft, const Liquid::FLiquidMaterial &Material)
        {
            if (auto &Profile = Draft.Profile)
            {
                if (Profile->Melt.Rule.empty() && Profile->Drying.Field.empty())
                {
                    ImGui::TextDisabled("No editable inputs");
                    DrawAssetTooltip(u8"이 씬에서 국소 갱신 규칙에 연결한 편집 가능한 입력이 없습니다.");
                    return;
                }
                DrawAssetSection(
                    "Inputs",
                    [&]
                    {
                        if (!Profile->Melt.Rule.empty())
                            EditAssetValue("Substrate temperature [K]",
                                           u8"씬에서 지정하는 바닥 온도입니다. 융해 규칙의 입력으로 사용합니다.",
                                           Profile->Cell.TemperatureK);
                        if (!Profile->Drying.Field.empty())
                        {
                            EditAssetValue("Drying split V",
                                           u8"표면 좌표 V에서 서로 다른 두 건조 구간을 나누는 경계입니다.",
                                           Profile->DryingSplitV,
                                           0.F,
                                           1.F);
                            EditAssetValue("Drying multiplier below split",
                                           u8"표면 좌표 V가 경계보다 작을 때 기본 건조율에 곱하는 값입니다.",
                                           Profile->DryingLow);
                            EditAssetValue("Drying multiplier above split",
                                           u8"표면 좌표 V가 경계보다 클 때 기본 건조율에 곱하는 값입니다.",
                                           Profile->DryingHigh);
                        }
                    });
                return;
            }
            const auto Defaults = DefaultProceduralUpdateRates(Draft.Scene);
            if (Material.LocalRule == Liquid::ELocalMaterialRule::Water &&
                (Defaults.MeltKgM2SecondK > 0.F || Defaults.FreezeKgM2SecondK > 0.F ||
                 Defaults.EvaporateKgM2SecondK > 0.F || Defaults.CharPerSecond > 0.F))
                DrawAssetSection("Physics", [&] { DrawProceduralUpdates(Draft, Defaults); });
            else
            {
                ImGui::TextDisabled("No editable inputs");
                DrawAssetTooltip(u8"현재 씬에서 바꿀 수 있는 입력 필드나 내장 규칙이 없습니다.");
            }
        }

        void DrawAssetProperties(const FAssetEntry             &Asset,
                                 FSurfaceSceneSettings         &Draft,
                                 const FSurfaceSceneSettings   &Active,
                                 const Liquid::FLiquidMaterial &ActiveLiquid)
        {
            using enum EAssetKind;
            switch (Asset.Kind)
            {
                case Profile:
                {
                    if (Draft.Snow.BulkDensityKgM3 > 0.F)
                    {
                        DrawAssetSection("Snow Appearance",
                                     [&]
                                     {
                                         EditAssetValue("Snow bulk density [kg/m^3]",
                                                        u8"눈의 표시 두께를 계산하는 겉보기 밀도입니다.",
                                                        Draft.Snow.BulkDensityKgM3,
                                                        1.F);
                                     });
                    }
                    if (!Draft.Profile)
                    {
                        DrawAssetSection("Bindings",
                                     []
                                     {
                                         DrawAssetValue("States",
                                                        u8"씬에 인라인으로 정의한 상태입니다.",
                                                        "states.layer / states.substrate");
                                     });
                        break;
                    }
                    auto &Settings = *Draft.Profile;
                    DrawAssetSection("Bindings",
                                 [&]
                                 {
                                     DrawAssetValue("Transport state",
                                                    u8"이 프로파일에서 표면을 따라 이동할 액체 상태입니다.",
                                                    Settings.LiquidState.c_str());
                                     DrawAssetValue("Substrate",
                                                    u8"표면층을 받치는 바탕의 재질 에셋입니다.",
                                                    Settings.Substrate.Id.c_str());
                                 });
                    if (!Settings.Absorption.Rule.empty())
                    DrawAssetSection("Substrate Storage",
                                     [&]
                                     {
                                         EditAssetValue("Absorption capacity [kg/m^2]",
                                                        u8"바탕이 단위 면적당 담을 수 있는 물의 최대 질량입니다.",
                                                        Settings.Cell.CapacityKgM2);
                                     });
                    DrawAssetSection(
                    "Appearance",
                    [&]
                    {
                        if (Settings.WetClothReferenceKgM2 > 0.F)
                            EditAssetValue("Wet cloth color reference [kg/m^2]",
                                           u8"천이 젖었을 때의 색 변화를 계산하는 흡수수 면밀도 기준입니다.",
                                           Settings.WetClothReferenceKgM2,
                                           1.e-5F);
                        EditAssetValue(
                            "Liquid optical depth scale",
                            u8"빛이 액체를 통과하는 광학 경로에 곱하는 배율입니다. 수심 계산값은 그대로입니다.",
                            Settings.LiquidOpticalDepthScale,
                            1.e-6F);
                        EditAssetValue("Liquid coverage depth [m]",
                                       u8"수면 렌더링에서 바닥과 섞인 얕은 막이 완전히 덮이는 기준 깊이입니다.",
                                       Settings.LiquidCoverageDepthMeters,
                                       1.e-6F);
                    });
                    break;
                }
                case Liquid:
                DrawMaterialProperties(Draft, Active, ActiveLiquid);
                break;
                case Substrate:
                {
                    auto &SubstrateSettings = Draft.Profile ? Draft.Profile->Substrate : Draft.ProceduralSubstrate;
                    auto &Optics = SubstrateSettings.Optics;
                    DrawAssetSection(
                    "Appearance",
                    [&]
                    {
                        EditAssetValue("Dry roughness",
                                       u8"바탕이 마른 상태에서의 반사 거칠기입니다. 낮을수록 반사가 선명합니다.",
                                       Optics.DryRoughness,
                                       0.F,
                                       1.F);
                        EditAssetValue("Wet roughness",
                                       u8"바탕이 젖었을 때 적용할 반사 거칠기입니다. 젖음 정도에 따라 마른 값에서 이 "
                                       u8"값으로 바뀝니다.",
                                       Optics.WetRoughness,
                                       0.F,
                                       1.F);
                        EditAssetValue(
                            "Substrate refractive index",
                            u8"바탕 재질의 굴절률입니다. 표면에서 반사되는 빛의 비율을 결정하는 데 쓰입니다.",
                            Optics.RefractiveIndex,
                            1.F,
                            4.F);
                        EditAssetValue("Specular scale",
                                       u8"굴절률로 계산한 정면 반사율에 곱하는 배율입니다.",
                                       Optics.SpecularScale,
                                       0.F,
                                       Rendering::MaxSubstrateSpecularScale);
                        if (!SubstrateSettings.NormalTexture.empty())
                            EditAssetValue("Material normal strength",
                                           u8"노멀 맵의 미세한 방향 변화를 음영에 반영하는 강도입니다.",
                                           Optics.NormalStrength,
                                           0.F,
                                           2.F);
                    });
                    break;
                }
                case LayerState:
                case SubstrateState:
                DrawAssetSection("Appearance", [&] { DrawStateProperties(Draft.States[Asset.Index]); });
                break;
                case Interaction:
                if (!Draft.bLocalUpdates)
                {
                    ImGui::TextDisabled("Local updates off");
                    DrawAssetTooltip(u8"현재 솔버에서 국소 갱신이 꺼져 있어 이 규칙이 적용되지 않습니다.");
                }
                DrawBoundUpdate(*Draft.Profile, Asset.Index);
                break;
                case Scene:
                ImGui::TextDisabled("Edit scene inputs in Conditions.");
                break;
            }
        }

        EAssetKind AssetGroup(EAssetKind Kind)
        {
            return Kind == EAssetKind::Substrate ? EAssetKind::Liquid : Kind;
        }

        struct FAssetGroupText
        {
            const char    *Title;
            const char8_t *Description;
        };

        FAssetGroupText AssetGroupText(EAssetKind Kind)
        {
            using enum EAssetKind;
            switch (Kind)
            {
                case Profile:
                return {"Surface Profiles",
                        u8"재질, 상태, 국소 갱신 규칙을 연결하고 표면의 외관 설정을 묶는 프로파일입니다."};
                case Liquid:
                case Substrate:
                return {"Physical Materials", u8"액체나 바탕의 밀도, 점도, 거칠기, 굴절률 같은 속성을 정의합니다."};
                case LayerState:
                return {"Layer States", u8"바탕 표면 위에 쌓이거나 흐르는 물, 눈, 얼음, 용암 같은 상태입니다."};
                case SubstrateState:
                return {"Substrate States", u8"바탕에 스며든 물, 열 또는 표면의 탄화처럼 바탕에 기록하는 상태입니다."};
                case Interaction:
                return {"Interactions",
                        u8"융해, 흡수, 건조처럼 프로파일에서 연결하는 재사용 가능한 상호작용 규칙입니다."};
                case Scene:
                return {"Scene", u8"현재 씬이 제공하는 온도나 규칙 계수 등 장면별 입력을 확인합니다."};
            }
            return {"", u8""};
        }

        const char8_t *AssetDescription(const FAssetEntry &Asset, FSceneAssetText SceneText)
        {
            if (Asset.Kind == EAssetKind::Scene)
                return SceneText.Description ? SceneText.Description : AssetGroupText(Asset.Kind).Description;

            using FDescription = std::pair<std::string_view, const char8_t *>;
            constexpr FDescription Descriptions[]{
                {"BrickRunoff", u8"홈이 파인 벽돌에서 물이 고이고 흘러가는 데 쓰는 표면 프로파일입니다."},
                {"BrickLava", u8"벽돌 위를 흐르는 용암의 상태와 표시 설정을 연결하는 표면 프로파일입니다."},
                {"SnowyCoat", u8"천 위의 눈이 녹고, 물이 스며들어 마르는 과정을 연결하는 표면 프로파일입니다."},
                {"Water", u8"물과 얼음의 밀도, 액체 물의 점도, 수면과 얼음의 광학 속성을 정의합니다."},
                {"Lava", u8"용암이 흐를 때 쓰는 물성과 발광 액상의 표시 속성을 정의합니다."},
                {"ViscousLiquid", u8"높은 점도의 액체가 흐를 때 쓰는 액체 재질입니다."},
                {"Brick", u8"벽돌 바탕의 반사와 젖음에 따른 광학 속성을 정의합니다."},
                {"Fabric", u8"천 바탕의 마른 상태와 젖은 상태에 쓰는 광학 속성을 정의합니다."},
                {"SurfaceWater",
                    u8"바탕 위에 고이거나 흐르는 액체 물입니다. 히트맵의 면밀도와 두께 범위를 정할 수 있습니다."},
                {"SurfaceLava", u8"바탕 위에 고이거나 흐르는 액체 용암의 상태입니다."},
                {"SnowIce", u8"표면 위에 쌓인 눈과 얼음의 고체 물 질량입니다."},
                {"Ice", u8"표면층에서 고체 얼음으로 남은 물의 질량을 기록합니다."},
                {"AbsorbedWater", u8"천 등의 바탕에 흡수되어 남은 물의 질량을 기록합니다."},
                {"CharFraction", u8"바탕이 얼마나 탄화됐는지 나타내는 상태입니다."},
                {"SubstrateTemperature", u8"바탕 온도를 확인하는 상태입니다. 히트맵의 표시 범위를 조정할 수 있습니다."},
                {"SnowMelt", u8"씬의 온도를 읽어 눈이나 얼음의 질량을 액체 물로 바꾸는 규칙입니다."},
                {"FabricAbsorption",
                    u8"표면 물이 천의 남은 흡수 용량을 넘지 않는 범위에서 바탕으로 이동하는 규칙입니다."},
                {"FabricDrying", u8"천에 흡수된 물이 시간이 지나면서 환경으로 빠져나가는 규칙입니다."}};
            const auto Found = std::ranges::find(Descriptions, AssetStem(Asset), &FDescription::first);
            return Found != std::end(Descriptions) ? Found->second : AssetGroupText(Asset.Kind).Description;
        }

        std::string AssetSelectionKey(const FAssetEntry &Asset)
        {
            return Asset.File + '#' + std::to_string(static_cast<int>(Asset.Kind)) + '#' + std::to_string(Asset.Index);
        }

        FAssetEntry PickAsset(const FSurfaceSceneSettings &Draft, std::string &Selection, FSceneAssetText SceneText)
        {
            const auto Assets = SceneAssets(Draft);
            auto       Selected = std::ranges::find(Assets, Selection, AssetSelectionKey);
            if (Selected == Assets.end())
                Selected = Assets.begin();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##Scene asset", AssetGroupText(Selected->Kind).Title, ImGuiComboFlags_HeightLarge))
            {
                using enum EAssetKind;
                constexpr std::array Groups{Profile, Liquid, LayerState, SubstrateState, Interaction, Scene};
                for (const auto Group : Groups)
                {
                    const auto InGroup = [Group](const auto &Entry) { return AssetGroup(Entry.Kind) == Group; };
                    if (std::ranges::none_of(Assets, InGroup))
                        continue;

                    const auto Text = AssetGroupText(Group);
                    ImGui::SeparatorText(Text.Title);
                    DrawAssetTooltip(Text.Description);
                    for (auto Entry = Assets.begin(); Entry != Assets.end(); ++Entry)
                    {
                        if (!InGroup(*Entry))
                            continue;
                        ImGui::PushID(AssetSelectionKey(*Entry).c_str());
                        if (ImGui::Selectable(AssetTitle(*Entry, SceneText).c_str(), Selected == Entry))
                            Selected = Entry;
                        if (Selected == Entry)
                            ImGui::SetItemDefaultFocus();
                        DrawAssetTooltip(AssetDescription(*Entry, SceneText), Entry->File.c_str());
                        ImGui::PopID();
                    }
                }
                ImGui::EndCombo();
            }
            else
                DrawAssetTooltip(AssetGroupText(Selected->Kind).Description);
            Selection = AssetSelectionKey(*Selected);
            return *Selected;
        }

        int AlignGridWidth(int Width, int Minimum, int Maximum, int ColumnMultiple)
        {
            const int MinBlocks = (Minimum + ColumnMultiple - 1) / ColumnMultiple;
            const int MaxBlocks = Maximum / ColumnMultiple;
            const int Clamped = std::clamp(Width, Minimum, Maximum);
            return std::clamp((Clamped + ColumnMultiple / 2) / ColumnMultiple, MinBlocks, MaxBlocks) * ColumnMultiple;
        }

        glm::ivec2 SimulationGridLimit(const FGridResolution &Resolution)
        {
            const auto GeometryLimit = Resolution.SamplesPerCell == 0
                                           ? Resolution.Geometry
                                           : glm::ivec2(Liquid::MaxSamplesPerAxis / Resolution.SamplesPerCell);
            return {std::min(GeometryLimit.x, Liquid::MaxCellsPerAxis),
                    std::min(GeometryLimit.y, Liquid::MaxCellsPerAxis)};
        }

        void FitGridResolution(FGridResolution &Resolution, int ColumnMultiple)
        {
            auto &Geometry = Resolution.Geometry;
            auto &Simulation = Resolution.Simulation;
            if (Resolution.SamplesPerCell == 0)
            {
                Geometry.x = AlignGridWidth(Geometry.x, 2, Liquid::MaxSamplesPerAxis, ColumnMultiple);
                Geometry.y = std::clamp(Geometry.y, 2, Liquid::MaxSamplesPerAxis);
            }

            const int  MinCells = Resolution.SamplesPerCell == 1 ? 2 : 1;
            const auto Limits = SimulationGridLimit(Resolution);
            Simulation.x = AlignGridWidth(Simulation.x, MinCells, Limits.x, ColumnMultiple);
            Simulation.y = std::clamp(Simulation.y, MinCells, Limits.y);
            if (Resolution.SamplesPerCell > 0)
                Geometry = Simulation * Resolution.SamplesPerCell;
        }

        bool DrawGridAxes(glm::ivec2 &Grid)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            int Axes[]{Grid.x, Grid.y};
            if (!ImGui::InputInt2("##Axes", Axes))
                return false;
            Grid = {Axes[0], Axes[1]};
            return true;
        }

        void RestoreSceneGrid(FGridResolution       &Resolution,
                              const FGridResolution &SceneDefaults,
                              int                    ColumnMultiple,
                              bool                   bRestoreSimulation)
        {
            auto Restored = Resolution;
            if (bRestoreSimulation)
                Restored.Simulation = SceneDefaults.Simulation;
            else
            {
                Restored.Geometry = SceneDefaults.Geometry;
                Restored.SamplesPerCell = SceneDefaults.SamplesPerCell;
            }

            const auto RequestedSimulation = Restored.Simulation;
            FitGridResolution(Restored, ColumnMultiple);
            // 한 줄의 기본값이 현재 다른 격자와 맞지 않으면 씬에 저장된 격자 조합을 복원한다.
            Resolution = Restored.Simulation == RequestedSimulation ? Restored : SceneDefaults;
        }

        std::string GridPresetPreview(glm::ivec2 Grid, std::span<const int> Presets, int Limit, int ColumnMultiple)
        {
            const auto Match = std::ranges::find(Presets, Grid.y);
            if (Match == Presets.end() || *Match > Limit || Grid.x != AlignGridWidth(*Match, 1, Limit, ColumnMultiple))
                return "Custom";
            return std::to_string(Grid.x) + " x " + std::to_string(Grid.y);
        }

        bool DrawSquareGridPresets(glm::ivec2          &Grid,
                                   std::span<const int> Presets,
                                   glm::ivec2           Limits,
                                   int                  ColumnMultiple,
                                   bool                 bSelectCurrent = true)
        {
            for (const int Side : Presets)
            {
                if (Side > Limits.x || Side > Limits.y)
                    continue;

                const glm::ivec2 Size{AlignGridWidth(Side, 1, Limits.x, ColumnMultiple), Side};
                const auto       Label = std::to_string(Size.x) + " x " + std::to_string(Size.y);
                if (ImGui::Selectable(Label.c_str(), bSelectCurrent && Grid == Size))
                {
                    Grid = Size;
                    return true;
                }
            }
            return false;
        }

        void DrawSamplesPerCellPresets(FGridResolution &Resolution)
        {
            ImGui::SeparatorText("Samples per simulation cell");
            for (const int Ratio : GeometrySamplesPerCellPresets)
            {
                const auto Total = Resolution.Simulation * Ratio;
                const bool bFits = Total.x >= 2 && Total.y >= 2 && Total.x <= Liquid::MaxSamplesPerAxis &&
                                   Total.y <= Liquid::MaxSamplesPerAxis;
                const auto Label = std::to_string(Ratio) + " x " + std::to_string(Ratio);
                if (ImGui::Selectable(Label.c_str(),
                                      Resolution.SamplesPerCell == Ratio,
                                      bFits ? 0 : ImGuiSelectableFlags_Disabled))
                    Resolution.SamplesPerCell = Ratio;
                if (!bFits && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Would produce %d x %d geometry samples. Use 2 to %d samples per axis.",
                                      Total.x,
                                      Total.y,
                                      Liquid::MaxSamplesPerAxis);
            }
        }

        auto GeometryEdits(const FSurfaceSceneSettings &Settings)
        {
            return std::tie(Settings.Source,
                            Settings.Path,
                            Settings.SizeM,
                            Settings.HeightOffsetM,
                            Settings.Scale,
                            Settings.BaseFilter,
                            Settings.FineFilter,
                            Settings.bBaseOnly);
        }

        auto AssetEdits(const FSurfaceSceneSettings &Settings)
        {
            return std::tie(Settings.Profile,
                            Settings.Snow,
                            Settings.ProceduralSubstrate,
                            Settings.ComparisonProfiles,
                            Settings.LiquidOverride,
                            Settings.ProceduralUpdateOverride,
                            Settings.States,
                            Settings.ShallowBasin,
                            Settings.SurfaceRelief,
                            Settings.InitialIcePeakKgM2,
                            Settings.InitialIceCenter,
                            Settings.InitialIceInverseRadius);
        }
    #pragma endregion

        // ============================================================
        // 00.02. Injection Brush
        // ============================================================
    #pragma region 00_02_Injection_Brush
        struct FBrushPreset
        {
            float       Value;
            const char *Level;
        };

        void DrawBrushPresets(const char *Label, float &Value, std::span<const FBrushPreset> Presets, const char *Unit)
        {
            // 씬이 바뀌면 이전 온도와 가장 가까운 유효 프리셋을 선택한다.
            const auto Selected = std::ranges::min_element(Presets,
                                                           {},
                                                           [Value](const FBrushPreset &Preset)
                                                           { return std::abs(Preset.Value - Value); });
            Value = Selected->Value;
            const auto Format = [Unit](const FBrushPreset &Preset)
            {
                std::array<char, 64> Text{};
                std::snprintf(Text.data(),
                              Text.size(),
                              "%g %s (%s)",
                              static_cast<double>(Preset.Value),
                              Unit,
                              Preset.Level);
                return Text;
            };
            DrawSettingRow(Label,
                           [&]
                           {
                               if (!ImGui::BeginCombo("##value", Format(*Selected).data()))
                                   return;
                               for (const auto &Preset : Presets)
                               {
                                   const bool bSelected = Preset.Value == Value;
                                   if (ImGui::Selectable(Format(Preset).data(), bSelected))
                                       Value = Preset.Value;
                                   if (bSelected)
                                       ImGui::SetItemDefaultFocus();
                               }
                               ImGui::EndCombo();
                           });
        }

        void DrawBrushNote(const char *Note, ImVec4 Color = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Color);
            ImGui::TextWrapped("%s", Note);
            ImGui::PopStyleColor();
        }

        void DrawBrushDose(FSurfaceSimulationUI &Ui, bool bHeatedChannel, bool bLava)
        {
            constexpr std::array Amounts{FBrushPreset{0.025F, "Very low"},
                                         FBrushPreset{0.075F, "Low"},
                                         FBrushPreset{0.15F, "Medium"},
                                         FBrushPreset{0.35F, "High"},
                                         FBrushPreset{1.F, "Very high"}};
            constexpr std::array Temperatures{FBrushPreset{-20.F, "Cold"},
                                              FBrushPreset{0.F, "Freezing"},
                                              FBrushPreset{45.F, "Warm"},
                                              FBrushPreset{200.F, "Hot"},
                                              FBrushPreset{600.F, "Very hot"}};
            constexpr std::array ChannelTemperatures{FBrushPreset{45.F, "Warm"},
                                                     FBrushPreset{100.F, "Boiling point"},
                                                     FBrushPreset{600.F, "Red glow"},
                                                     FBrushPreset{900.F, "Orange glow"},
                                                     FBrushPreset{1200.F, "Bright glow"}};
            constexpr std::array Durations{FBrushPreset{0.05F, "Very short"},
                                           FBrushPreset{0.25F, "Short"},
                                           FBrushPreset{1.F, "Medium"},
                                           FBrushPreset{5.F, "Long"},
                                           FBrushPreset{30.F, "Very long"}};
            if (Ui.InjectionTarget == EInjectionTarget::Temperature)
            {
                DrawBrushPresets("Target temperature",
                                 Ui.TargetTemperatureC,
                                 bHeatedChannel ? ChannelTemperatures : Temperatures,
                                 Utf8(u8"°C"));
                if (bHeatedChannel)
                    DrawBrushNote(Utf8(u8"Display thresholds: red tint from 377 °C, orange from 712 °C, "
                                       "bright glow from 947 °C. Glow builds gradually. Water cools the steel."),
                                  {1.F, 0.69F, 0.37F, 1.F});
                DrawBrushPresets("Duration", Ui.TemperatureDuration, Durations, "s");
                DrawBrushNote(Utf8(u8"The target °C is reapplied to covered cells for this many simulation seconds."));
            }
            else if (Ui.InjectionTarget == EInjectionTarget::Ice)
            {
                DrawBrushPresets("Mass per press", Ui.IceDoseGrams, Amounts, "g");
                DrawBrushNote("Total ice mass added by one press of Space, shared across the brush.");
            }
            else
            {
                DrawBrushPresets(bLava ? "Lava per press" : "Water per press", Ui.DoseMilliliters, Amounts, "mL");
                DrawBrushNote("Total liquid volume added by one press of Space, shared across the brush.");
            }
        }

        void DrawBrushFootprint(FInjectionBrush &Brush)
        {
            int Shape = static_cast<int>(Brush.Shape);
            DrawSettingRow("Shape",
                           [&]
                           {
                               if (ImGui::Combo("##value", &Shape, "Point\0Circle\0Rectangle\0"))
                                   Brush.Shape = static_cast<EInjectionShape>(Shape);
                           });
            const auto DrawSize = [](const char *Label, float &Meters)
            {
                DrawSettingRow(Label,
                               [&]
                               {
                                   float          Millimeters = Meters * 1000.F;
                                   constexpr auto Flags = ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_NoInput;
                                   if (ImGui::SliderFloat("##value", &Millimeters, 0.1F, 1000.F, "%.1f mm", Flags))
                                       Meters = Millimeters * 0.001F;
                               });
            };
            if (Brush.Shape == EInjectionShape::Circle)
            {
                DrawSize("Radius", Brush.RadiusM);
                DrawBrushNote("Distance from the center to the edge of the circular brush.");
            }
            else if (Brush.Shape == EInjectionShape::Rectangle)
            {
                DrawSize("Width", Brush.SizeM.x);
                DrawSize("Height", Brush.SizeM.y);
                DrawSettingRow(
                    "Rotation",
                    [&]
                    {
                        float Angle = Brush.AngleRadians * 57.2957795F;
                        if (ImGui::SliderFloat("##value", &Angle, -180.F, 180.F, "%.1f deg", ImGuiSliderFlags_NoInput))
                            Brush.AngleRadians = Angle * 0.01745329252F;
                    });
                DrawBrushNote("Size and orientation of the rectangular footprint on the surface.");
            }
            else
                DrawBrushNote("Applies the input at the cursor position.");
        }
    #pragma endregion
    } // namespace

    // ============================================================
    // 01. Grid Resolution Controls
    // ============================================================
    #pragma region 01_Grid_Resolution_Controls
        FGridResolution FGridResolution::From(const FSurfaceSceneSettings &Settings)
        {
            return {{Settings.ShapeColumns, Settings.ShapeRows},
                    {Settings.GridColumns, Settings.GridRows},
                    Settings.GeometrySamplesPerCell};
        }

        void FGridResolution::ApplyTo(FSurfaceSceneSettings &Settings) const
        {
            Settings.ShapeColumns = Geometry.x;
            Settings.ShapeRows = Geometry.y;
            Settings.GridColumns = Simulation.x;
            Settings.GridRows = Simulation.y;
            Settings.GeometrySamplesPerCell = SamplesPerCell;
        }

        bool DrawGridResolution(FGridResolution            &Resolution,
                                int                         ColumnMultiple,
                                const std::array<float, 2> *SizeM,
                                const FGridResolution      *SceneDefaults)
        {
            constexpr std::array SimulationPresets{16, 32, 64, 128};
            constexpr std::array GeometryPresets{32, 64, 128, 256};

            const auto Original = Resolution;
            FitGridResolution(Resolution, ColumnMultiple);
            DrawSettingRow(
                "Simulation cells",
                [&]
                {
                    const auto Limits = SimulationGridLimit(Resolution);
                    const bool bSceneGrid = SceneDefaults && Resolution.Simulation == SceneDefaults->Simulation;
                    const auto Preview =
                        bSceneGrid
                            ? "Scene Default"
                            : GridPresetPreview(Resolution.Simulation, SimulationPresets, Limits.x, ColumnMultiple);
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.49F);
                    if (ImGui::BeginCombo("##Presets", Preview.c_str()))
                    {
                        if (SceneDefaults && ImGui::Selectable("Scene Default", bSceneGrid))
                            RestoreSceneGrid(Resolution, *SceneDefaults, ColumnMultiple, true);
                        DrawSquareGridPresets(Resolution.Simulation,
                                              SimulationPresets,
                                              SimulationGridLimit(Resolution),
                                              ColumnMultiple,
                                              !bSceneGrid);
                        ImGui::EndCombo();
                    }
                    else
                        ImGui::SetItemTooltip("Simulation grid presets");
                    ImGui::SameLine();
                    DrawGridAxes(Resolution.Simulation);
                });
            FitGridResolution(Resolution, ColumnMultiple);

            DrawSettingRow("Geometry samples",
                           [&]
                           {
                               const int   Ratio = Resolution.SamplesPerCell;
                               const bool  bSceneGrid = SceneDefaults && Ratio == SceneDefaults->SamplesPerCell &&
                                                        (Ratio > 0 || Resolution.Geometry == SceneDefaults->Geometry);
                               std::string Preview = "Scene Default";
                               if (!bSceneGrid)
                                   Preview = Ratio > 0
                                                 ? std::to_string(Ratio) + " x " + std::to_string(Ratio) + " / cell"
                                                 : GridPresetPreview(Resolution.Geometry,
                                                                     GeometryPresets,
                                                                     Liquid::MaxSamplesPerAxis,
                                                                     ColumnMultiple);
                               ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.49F);
                               if (ImGui::BeginCombo("##Presets", Preview.c_str()))
                               {
                                   if (SceneDefaults && ImGui::Selectable("Scene Default", bSceneGrid))
                                       RestoreSceneGrid(Resolution, *SceneDefaults, ColumnMultiple, false);
                                   DrawSamplesPerCellPresets(Resolution);
                                   ImGui::SeparatorText("Fixed geometry grid");
                                   if (DrawSquareGridPresets(Resolution.Geometry,
                                                             GeometryPresets,
                                                             glm::ivec2(Liquid::MaxSamplesPerAxis),
                                                             ColumnMultiple,
                                                             Resolution.SamplesPerCell == 0))
                                       Resolution.SamplesPerCell = 0;
                                   ImGui::EndCombo();
                               }
                               else
                                   ImGui::SetItemTooltip("Samples per cell or a fixed geometry grid");
                               ImGui::SameLine();
                               // 직접 입력하면 현재 계산 격자와 독립적인 표본 해상도가 된다.
                               if (DrawGridAxes(Resolution.Geometry))
                                   Resolution.SamplesPerCell = 0;
                           });
            FitGridResolution(Resolution, ColumnMultiple);

            const auto &Simulation = Resolution.Simulation;
            const auto &Geometry = Resolution.Geometry;
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (SizeM)
            {
                ImGui::TextWrapped("%d cells (%.2f x %.2f mm each)",
                                   Simulation.x * Simulation.y,
                                   1000.F * (*SizeM)[0] / Simulation.x,
                                   1000.F * (*SizeM)[1] / Simulation.y);
                ImGui::TextWrapped("%d geometry samples (%.2f x %.2f mm each)",
                                   Geometry.x * Geometry.y,
                                   1000.F * (*SizeM)[0] / Geometry.x,
                                   1000.F * (*SizeM)[1] / Geometry.y);
            }
            else
                ImGui::TextWrapped("%d simulation cells, %d geometry samples",
                                   Simulation.x * Simulation.y,
                                   Geometry.x * Geometry.y);

            const float SamplesX = static_cast<float>(Geometry.x) / Simulation.x;
            const float SamplesY = static_cast<float>(Geometry.y) / Simulation.y;
            ImGui::TextWrapped(Resolution.SamplesPerCell == 0
                                   ? "Fixed geometry grid (%.2g x %.2g samples per cell on average)"
                                   : "%.2g x %.2g geometry samples per cell",
                               SamplesX,
                               SamplesY);
            ImGui::PopStyleColor();
            return Original != Resolution;
        }

    #pragma endregion

        void FSurfaceSimulationUI::SetDraft(const FSurfaceSceneSettings &Settings, bool bOpeningScene)
        {
            if (bOpeningScene || !SceneDefaultGrid)
                SceneDefaultGrid = FGridResolution::From(Settings);
            Draft = Settings;
            InputPath.fill('\0');
            const auto Path = Settings.Path.u8string();
            std::copy_n(Path.begin(), std::min(Path.size(), InputPath.size() - 1), InputPath.begin());
            ProfileError.clear();
        }

        ESurfaceSimulationAction FSurfaceSimulationUI::DrawPanel(EEditorPanel                          Panel,
                                                                 const FSurfaceSceneSettings          &Active,
                                                                 const Liquid::FLiquidMaterial        &ActiveLiquid,
                                                                 float                                 Time,
                                                                 float                                 TiltDegrees,
                                                                 const FSurfaceSimulationDisplayStats &Timing,
                                                                 FSceneAssetText                       SceneText)
        {
            ESurfaceSimulationAction Action = ESurfaceSimulationAction::None;
            ImGui::PushItemWidth(-125.F);
            switch (Panel)
            {
            case EEditorPanel::Simulation:
            {
                if (ImGui::CollapsingHeader("Operations", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawSettingToggle("Transport", Draft.bTransport, "Allow liquid to move between connected regions.");
                    DrawSettingToggle("Local Update",
                                  Draft.bLocalUpdates,
                                  "Allow local changes such as melting, absorption, and drying.");
                    DrawSettingToggle("Source & Boundary",
                                  Draft.bExternalExchange,
                                  "Allow external supply, temperature inputs, and outlet discharge.");
                    DrawSettingRow(
                        "Reconstruction",
                        [] { ImGui::TextDisabled("Always on"); },
                        "Restore the liquid level and derived values used by the simulation.");
                }
                if (ImGui::CollapsingHeader("Storage Regions", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    DrawSettingToggle("Automatic basin splitting",
                                      Draft.bSplitSubgridBasins,
                                      "Give disconnected basins within each simulation cell separate liquid storage. "
                                      "Apply & Restart rebuilds the storage regions.");
                }
                if (ImGui::CollapsingHeader("Resolution"))
                {
                    auto Resolution = FGridResolution::From(Draft);
                    DrawGridResolution(Resolution,
                                   Draft.SpecimenCount(),
                                   &Draft.SizeM,
                                   SceneDefaultGrid ? &*SceneDefaultGrid : nullptr);
                    Resolution.ApplyTo(Draft);
                    if (Draft.SpecimenCount() > 1)
                    ImGui::TextDisabled("Equal widths for %d specimens", Draft.SpecimenCount());
                }
                if (ImGui::CollapsingHeader("Boundary"))
                {
                    ImGui::BeginDisabled(!Draft.bExternalExchange || !Draft.ComparisonProfiles.empty());
                    const char *Choices[] = {Draft.bOpenOutlet ? "Scene default (Open +X edge)" : "Scene default (Closed)",
                                         "Closed",
                                         "Open +X edge"};
                    int         Boundary = !Draft.OpenOutletOverride ? 0 : *Draft.OpenOutletOverride ? 2 : 1;
                    DrawSettingRow("Outlet",
                               [&]
                               {
                                   if (ImGui::Combo("##Outlet", &Boundary, Choices, 3))
                                       Draft.OpenOutletOverride =
                                           Boundary == 0 ? std::nullopt : std::optional{Boundary == 2};
                               });
                    ImGui::EndDisabled();
                    const bool bActiveOpen = Active.ComparisonProfiles.empty() && Active.bExternalExchange &&
                                         Active.OpenOutletOverride.value_or(Active.bOpenOutlet);
                    DrawSettingRow("Active outlet",
                               [&] { ImGui::TextDisabled("%s", bActiveOpen ? "Open +X edge" : "Closed"); });
                    if (!Draft.ComparisonProfiles.empty())
                    ImGui::TextDisabled("All specimens use closed boundaries.");
                    else if (!Draft.bExternalExchange)
                    ImGui::TextDisabled("Source & Boundary is off.");
                }
                if (ImGui::CollapsingHeader("Time Step"))
                {
                    DrawSettingRow("Fixed step", [] { ImGui::TextDisabled("%.2f ms", Liquid::StepSeconds * 1000.F); });
                    const char *Counts[] = {"1", "2", "4", "8"};
                    int         Index = Draft.TransportSubsteps == 8   ? 3
                                    : Draft.TransportSubsteps == 4 ? 2
                                    : Draft.TransportSubsteps == 2 ? 1
                                                                   : 0;
                    DrawSettingRow(
                    "Transport substeps",
                    [&]
                    {
                        if (ImGui::Combo("##Substeps", &Index, Counts, 4))
                            Draft.TransportSubsteps = 1 << Index;
                    },
                    "Repeat flow inside the fixed step. Local updates and scheduled input run once.");
                    DrawSettingRow(
                    "Active transport interval",
                    [&] { ImGui::TextDisabled("%.3f ms", Liquid::StepSeconds * 1000.F / Active.TransportSubsteps); });
                }
                break;
            }
            case EEditorPanel::Object:
            {
                using enum SurfaceState::EHeightfieldSource;
                constexpr std::array Sources{"Scene geometry",
                                         "Height Map",
                                         "Displacement Map",
                                         "Normal Map",
                                         "Reference Mesh"};
                int                  Source = static_cast<int>(Draft.Source);
                if (Draft.Scene == EScene::Cavity || Source > static_cast<int>(HeightMap))
                ImGui::Text("Geometry source: %s", Sources[Source]);
                else if (ImGui::Combo("Geometry source", &Source, Sources.data(), 2))
                Draft.Source = static_cast<SurfaceState::EHeightfieldSource>(Source);
                if (Draft.Source != Procedural)
                {
                    if (ImGui::InputText("Geometry path", InputPath.data(), InputPath.size()))
                    Draft.Path =
                        std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(InputPath.data())));
                    const char *ScaleLabel = Draft.Source == HeightMap         ? "Height range [m]"
                                         : Draft.Source == DisplacementMap ? "Displacement amplitude [m]"
                                                                           : "Geometry scale";
                    ImGui::InputFloat(ScaleLabel, &Draft.Scale, 0.001F, 0.01F, "%.6f");
                    if (Draft.Scene != EScene::Cavity)
                    ImGui::InputFloat("Height offset [m]", &Draft.HeightOffsetM, 0.001F, 0.01F, "%.6f");
                    ImGui::SliderInt("Meso filter", &Draft.FineFilter, 0, 30);
                    ImGui::SliderInt("Base filter", &Draft.BaseFilter, Draft.FineFilter, 150);
                    Draft.BaseFilter = std::max(Draft.BaseFilter, Draft.FineFilter);
                    ImGui::Checkbox("Use only low-frequency base", &Draft.bBaseOnly);
                }
                DrawAssetSection("Surface dimensions",
                             [&]
                             {
                                 EditAssetValue("Width [m]", u8"표면 차트의 가로 길이입니다.", Draft.SizeM[0], 0.001F);
                                 EditAssetValue("Length [m]", u8"표면 차트의 세로 길이입니다.", Draft.SizeM[1], 0.001F);
                             });
                break;
            }
            case EEditorPanel::Assets:
            {
                const auto  Asset = PickAsset(Draft, SelectedAsset, SceneText);
                const auto *Description = AssetDescription(Asset, SceneText);
                ImGui::Spacing();
                ImGui::PushFont(nullptr, EditorLayout::TitleSize);
                ImGui::TextWrapped("%s", AssetTitle(Asset, SceneText).c_str());
                ImGui::PopFont();
                DrawAssetTooltip(Description);
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                if (Asset.Kind == EAssetKind::LayerState || Asset.Kind == EAssetKind::SubstrateState)
                ImGui::TextWrapped("%s.SurfaceProfile / states.%s",
                                   Draft.ProfileId.c_str(),
                                   Asset.Kind == EAssetKind::LayerState ? "layer" : "substrate");
                else
                ImGui::TextWrapped("%s", Asset.File.c_str());
                ImGui::PopStyleColor();
                DrawAssetTooltip(Description);
                ImGui::PushID(SelectedAsset.c_str());
                DrawAssetProperties(Asset, Draft, Active, ActiveLiquid);
                ImGui::PopID();
                if (ImGui::Button("Load Scene Defaults"))
                {
                    try
                    {
                        auto Settings = LoadSceneAsset(Draft.AssetPath);
                        if (Settings.Scene != Draft.Scene || Draft.Profile.has_value() != Settings.Profile.has_value())
                        throw std::runtime_error("Scene profile changed. Reopen the scene to load it.");
                        const auto DefaultLiquid = Settings.Profile && Draft.LiquidMaterialPath.empty()
                                                   ? Settings.Profile->Liquid
                                                   : MDSS::Asset::LoadLiquidMaterial(Draft.LiquidMaterialPath);
                        Draft.States = std::move(Settings.States);
                        Draft.Snow = std::move(Settings.Snow);
                        Draft.ProceduralSubstrate = std::move(Settings.ProceduralSubstrate);
                        Draft.ShallowBasin = Settings.ShallowBasin;
                        Draft.SurfaceRelief = Settings.SurfaceRelief;
                        if (Draft.Profile)
                        {
                            Draft.Profile = std::move(Settings.Profile);
                            Draft.ComparisonProfiles = std::move(Settings.ComparisonProfiles);
                            Draft.InitialIcePeakKgM2 = Settings.InitialIcePeakKgM2;
                            Draft.InitialIceCenter = Settings.InitialIceCenter;
                            Draft.InitialIceInverseRadius = Settings.InitialIceInverseRadius;
                        }
                        Draft.LiquidOverride =
                        DefaultLiquid == ActiveLiquid ? Active.LiquidOverride : std::optional{DefaultLiquid};
                        Draft.ProceduralUpdateOverride.reset();
                        ProfileError.clear();
                    }
                    catch (const std::exception &Error)
                    {
                        ProfileError = Error.what();
                    }
                }
                DrawAssetTooltip(
                u8"씬 파일의 에셋 기본값을 편집란에 불러옵니다. Apply & Restart를 눌러 실행에 반영합니다.");
                if (!ProfileError.empty())
                ImGui::TextWrapped("%s", ProfileError.c_str());
                break;
            }
            case EEditorPanel::Conditions:
            {
                if (!Draft.ComparisonProfiles.empty())
                {
                    ImGui::Text("%.1f g per specimen", Draft.InitialLiquidKg * 1000.F / Draft.SpecimenCount());
                    const auto ShowCapacity = [](const FSurfaceProfileSettings &Specimen)
                    { ImGui::Text("%s  %.2f kg/m^2", Specimen.Substrate.Id.c_str(), Specimen.Cell.CapacityKgM2); };
                    ShowCapacity(*Draft.Profile);
                    for (const auto &Specimen : Draft.ComparisonProfiles)
                    ShowCapacity(Specimen);
                }
                DrawAssetSection("Initial State",
                             [&]
                             {
                                 EditAssetValue("Surface liquid [kg]",
                                                u8"처음에 표면에 올려 두는 액체의 질량입니다.",
                                                Draft.InitialLiquidKg);
                                 if (Draft.Profile &&
                                     (Draft.InitialIcePeakKgM2 > 0.F || Active.InitialIcePeakKgM2 > 0.F))
                                     EditAssetValue("Peak snow or ice [kg/m^2]",
                                                    u8"시작할 때 눈이나 얼음이 가장 두껍게 쌓인 곳의 면밀도입니다.",
                                                    Draft.InitialIcePeakKgM2);
                             });
                DrawAssetSection(
                "Scene Track Defaults",
                [&]
                {
                    EditAssetValue("Inlet mass [kg]", u8"예약 주입의 전체 질량입니다.", Draft.PourKg);
                    EditAssetValue("Inlet start [s]", u8"첫 예약 주입이 시작되는 시각입니다.", Draft.PourStartSeconds);
                    EditAssetValue("Inlet duration [s]",
                                   u8"한 번의 예약 주입에 걸리는 시간입니다.",
                                   Draft.PourSeconds,
                                   Liquid::StepSeconds);
                    if (Draft.Scene != EScene::FreezeThaw)
                    {
                        float Angle = Draft.MaxTiltRadians * 57.2957795F;
                        if (EditAssetValue("Maximum tilt [deg]",
                                           u8"씬 기울이기 트랙이 도달하는 각도입니다.",
                                           Angle,
                                           -60.F,
                                           60.F))
                            Draft.MaxTiltRadians = Angle * 0.01745329252F;
                    }
                });
                DrawSceneFields(Draft, Draft.LiquidOverride.value_or(ActiveLiquid));
                break;
            }
            case EEditorPanel::Tools:
            {
                const bool bSealed = Active.Scene == EScene::FreezeThaw;
                EditorLayout::SectionTitle("Injection Brush");
                if (bSealed || !Active.bExternalExchange)
                ImGui::TextDisabled("%s", bSealed ? "Sealed freeze/thaw cycle." : "Source & Boundary is off.");
                ImGui::BeginDisabled(bSealed || !Active.bExternalExchange);
                ImGui::Checkbox("Enable surface injection", &bInjectionEnabled);
                const auto Ice = std::ranges::find(Active.States,
                                               Rendering::ESurfaceStateChannel::IceMass,
                                               &Rendering::FStateVisualization::Channel);
                const auto LiquidState = std::ranges::find(Active.States,
                                                       Rendering::ESurfaceStateChannel::LiquidMass,
                                                       &Rendering::FStateVisualization::Channel);
                const bool bLiquid = LiquidState != Active.States.end();
                const bool bIce = ActiveLiquid.LocalRule == Liquid::ELocalMaterialRule::Water && Ice != Active.States.end();
                if (InjectionTarget == EInjectionTarget::Ice && !bIce)
                InjectionTarget = EInjectionTarget::PrimaryLiquid;
                if (InjectionTarget == EInjectionTarget::PrimaryLiquid && !bLiquid)
                InjectionTarget = EInjectionTarget::Temperature;

                const auto TargetName = [&](EInjectionTarget Target)
                {
                    if (Target == EInjectionTarget::Temperature)
                    return "Temperature Pulse";
                    if (Target == EInjectionTarget::Ice && bIce)
                    return Ice->Name.c_str();
                    return bLiquid ? LiquidState->Name.c_str() : "Surface liquid";
                };
                DrawSettingRow(
                "Target",
                [&]
                {
                    if (ImGui::BeginCombo("##value", TargetName(InjectionTarget)))
                    {
                        for (const auto Target :
                             {EInjectionTarget::PrimaryLiquid, EInjectionTarget::Ice, EInjectionTarget::Temperature})
                        {
                            if ((Target == EInjectionTarget::Ice && !bIce) ||
                                (Target == EInjectionTarget::PrimaryLiquid && !bLiquid))
                                continue;
                            if (ImGui::Selectable(TargetName(Target), Target == InjectionTarget))
                                InjectionTarget = Target;
                        }
                        ImGui::EndCombo();
                    }
                });
                DrawBrushDose(*this, Active.SubstrateQuench.has_value(), ActiveLiquid.bIncandescent);
                DrawBrushFootprint(Brush);

                if (bInjectionEnabled)
                ImGui::TextWrapped("Aim at the surface and press Space. Each dose advances one step while paused.");
                if (!InputMessage.empty())
                ImGui::TextWrapped("%s", InputMessage.c_str());
                ImGui::EndDisabled();
                EditorLayout::SectionTitle("Surface Tilt");
                ImGui::BeginDisabled(bSealed);
                int Mode = bAutoTilt ? 0 : 1;
                if (ImGui::Combo("Pose control", &Mode, "Follow scene\0Manual\0"))
                {
                    bAutoTilt = Mode == 0;
                    if (!bAutoTilt)
                    ManualTiltDegrees = TiltDegrees;
                }
                if (!bAutoTilt)
                ImGui::SliderFloat("Manual angle", &ManualTiltDegrees, -60.F, 60.F, "%.1f deg");
                ImGui::EndDisabled();
                ImGui::TextDisabled("Current angle  %.1f deg", TiltDegrees);
                if (!bPlaying && !bAutoTilt && !bSealed)
                ImGui::TextWrapped("Step or play to apply the manual angle.");
                break;
            }
            case EEditorPanel::Timeline:
            {
                using enum EditorLayout::EPlaybackIcon;
                if (EditorLayout::PlaybackButton("##PlayPause", bPlaying ? Pause : Play, bPlaying ? "Pause" : "Play"))
                bPlaying = !bPlaying;
                ImGui::SameLine();
                if (EditorLayout::PlaybackButton("##Step", Step, "Advance one fixed step"))
                {
                    bPlaying = false;
                    bSingleStep = true;
                }
                ImGui::SameLine();
                if (EditorLayout::PlaybackButton("##Restart", Restart, "Restart with the currently applied settings"))
                Action = ESurfaceSimulationAction::Restart;
                ImGui::SameLine();
                constexpr std::array Labels{"0.05x", "0.1x", "0.25x", "0.5x", "1x", "2x", "4x"};
                static_assert(Labels.size() == PlaybackRates.size());
                int Index = static_cast<int>(std::ranges::find(PlaybackRates, PlaybackRate) - PlaybackRates.begin());
                ImGui::SetNextItemWidth(EditorLayout::PlaybackSpeedWidth());
                if (ImGui::Combo("##Playback speed", &Index, Labels.data(), static_cast<int>(Labels.size())))
                PlaybackRate = PlaybackRates[Index];
                ImGui::SetItemTooltip("Playback speed");
                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::Text(bPlaying ? "%.2f s" : "%.3f s", Time);
                break;
            }
            case EEditorPanel::ApplySettings:
            {
                const auto SolverEdits = [](const FSurfaceSceneSettings &Settings)
                {
                    return std::tie(Settings.GridColumns,
                                Settings.GridRows,
                                Settings.ShapeColumns,
                                Settings.ShapeRows,
                                Settings.GeometrySamplesPerCell,
                                Settings.OpenOutletOverride,
                                Settings.bSplitSubgridBasins,
                                Settings.bTransport,
                                Settings.TransportSubsteps,
                                Settings.bLocalUpdates,
                                Settings.bExternalExchange);
                };
                const auto Conditions = [](const FSurfaceSceneSettings &Settings)
                {
                    return std::tie(Settings.InitialLiquidKg,
                                Settings.PourKg,
                                Settings.PourStartSeconds,
                                Settings.PourSeconds,
                                Settings.MaxTiltRadians);
                };
                const bool  bSolver = SolverEdits(Draft) != SolverEdits(Active);
                const bool  bGeometry = GeometryEdits(Draft) != GeometryEdits(Active);
                const bool  bConditions = Conditions(Draft) != Conditions(Active);
                const bool  bAssets = AssetEdits(Draft) != AssetEdits(Active);
                const bool  bDirty = bSolver || bGeometry || bAssets || bConditions;
                std::string Pending;
                for (const auto &[bEdited, Label] : {std::pair{bSolver, "Solver"},
                                                 std::pair{bGeometry, "Geometry"},
                                                 std::pair{bAssets, "Assets"},
                                                 std::pair{bConditions, "Conditions"}})
                if (bEdited)
                {
                    if (!Pending.empty())
                        Pending += ", ";
                    Pending += Label;
                }
                if (!ProfileError.empty())
                {
                    ImGui::TextColored({1.F, 0.54F, 0.4F, 1.F}, "Settings error");
                    ImGui::SetItemTooltip("%s", ProfileError.c_str());
                }
                else if (bDirty)
                {
                    ImGui::TextUnformatted("Pending changes");
                    ImGui::SetItemTooltip("%s", Pending.c_str());
                }
                else
                ImGui::TextDisabled("No pending changes");
                ImGui::BeginDisabled(!bDirty);
                if (bDirty)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
                }
                if (ImGui::Button("Apply & Restart"))
                {
                    ProfileError.clear();
                    Action = ESurfaceSimulationAction::Apply;
                }
                if (bDirty)
                ImGui::PopStyleColor(2);
                ImGui::SameLine();
                if (ImGui::Button("Discard Changes"))
                SetDraft(Active);
                ImGui::EndDisabled();
                break;
            }
            case EEditorPanel::Camera:
            {
                if (ImGui::Button("Reset Camera View"))
                Action = ESurfaceSimulationAction::ResetCamera;
                ImGui::TextWrapped("Right-drag to orbit. WASD to move, Shift for faster movement. Scroll or +/- to zoom. "
                               "Home resets the camera.");
                ImGui::SliderFloat("Azimuth", &Views.CameraAzimuth, -180.F, 180.F, "%.0f deg");
                ImGui::SliderFloat("Elevation", &Views.CameraElevation, -89.F, 89.F, "%.0f deg");
                ImGui::SliderFloat("Distance scale", &Views.CameraDistanceScale, 0.25F, 4.F, "%.2fx");
                break;
            }
            case EEditorPanel::SceneTracks:
            {
                const bool bHasTilt = Active.Scene != EScene::FreezeThaw && Active.MaxTiltRadians != 0.F;
                const bool bHasHeat = Active.Scene == EScene::Charring || Active.Scene == EScene::Meadow ||
                                  Active.Scene == EScene::FreezeThaw || Active.Scene == EScene::IceBed;
                const auto Camera = ScheduledCamera(Active.Scene, 6.F);
                const bool bHasCamera = Camera.Azimuth != 0.F || Camera.Elevation != 0.F || Camera.DistanceScale != 1.F ||
                                    Active.Scene == EScene::FreezeThaw;
                ImGui::Text("Scene tracks   %.2f s", Time);
                ImGui::Separator();
                if (Active.PourKg > 0.F)
                {
                    ImGui::BeginDisabled(!Active.bExternalExchange);
                    ImGui::Checkbox("Scheduled inlet", &bScheduledInlet);
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("%.3g g from %.3g to %.3g s",
                                    Active.PourKg * 1000.F,
                                    Active.PourStartSeconds,
                                    Active.PourStartSeconds + Active.PourSeconds);
                    if (Active.SecondPourStart > 0.F)
                    ImGui::TextDisabled("Second inlet from %.3g to %.3g s",
                                        Active.SecondPourStart,
                                        Active.SecondPourStart + Active.PourSeconds);
                }
                else
                ImGui::TextDisabled("Inlet   None");
                if (bHasTilt)
                ImGui::TextDisabled("Tilt   %s  /  %.1f deg", bAutoTilt ? "Following scene" : "Manual", TiltDegrees);
                else
                ImGui::TextDisabled("Tilt   None");
                if (bHasHeat)
                {
                    ImGui::BeginDisabled(!Active.bExternalExchange);
                    ImGui::Checkbox("Scheduled temperature", &bScheduledHeat);
                    ImGui::EndDisabled();
                }
                else
                ImGui::TextDisabled("No scheduled temperature");
                ImGui::BeginDisabled(!bHasCamera);
                ImGui::Checkbox("Cinematic camera", &bAnimateCamera);
                ImGui::EndDisabled();
                break;
            }
            case EEditorPanel::Experiments:
            Action = DrawExperiments();
            break;
            case EEditorPanel::Profiler:
            {
                EditorLayout::SectionTitle("GPU Frame (ms)");
                if (ImGui::BeginTable("SurfaceTimings", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthStretch, 2.2F);
                    ImGui::TableSetupColumn("Latest", ImGuiTableColumnFlags_WidthStretch, 1.F);
                    ImGui::TableSetupColumn("Mean", ImGuiTableColumnFlags_WidthStretch, 1.F);
                    ImGui::TableSetupColumn("P90", ImGuiTableColumnFlags_WidthStretch, 1.F);
                    ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthStretch, 0.85F);
                    ImGui::TableHeadersRow();
                    const auto Row = [](const char *Label, const FSurfaceSimulationTimingStats &Stats)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextWrapped("%s", Label);
                        for (const double Value : {Stats.LatestMilliseconds, Stats.MeanMilliseconds, Stats.P90Milliseconds})
                        {
                            ImGui::TableNextColumn();
                            if (Stats.Samples > 0)
                            ImGui::Text("%.3f", Value);
                            else
                            ImGui::TextDisabled("-");
                        }
                        ImGui::TableNextColumn();
                        ImGui::Text("%zu", Stats.Samples);
                    };
                    Row("Simulation steps", Timing.Solver);
                    Row("Snapshot copy", Timing.Snapshot);
                    Row("Viewport drawing", Timing.Draw);
                    Row("  Refraction prepass", Timing.RefractionPrepass);
                    Row("  Main color pass", Timing.ColorPass);
                    ImGui::EndTable();
                }
                break;
            }
        }
        ImGui::PopItemWidth();
        return Action;
    }

#pragma endregion

    // ============================================================
    // 01. Experiment Results
    // ============================================================
#pragma region 01_Experiment_Results

    ESurfaceSimulationAction FSurfaceSimulationUI::DrawExperiments()
    {
        constexpr std::array     Modes{"CPU Reference", "Compare CPU / GPU"};
        const auto              &Style = ImGui::GetStyle();
        const float              RunWidth = ImGui::CalcTextSize("Run").x + 2.F * Style.FramePadding.x;
        ESurfaceSimulationAction Action = ESurfaceSimulationAction::None;
        ImGui::BeginDisabled(bExperimentRunning);
        ImGui::SetNextItemWidth(std::max(60.F, ImGui::GetContentRegionAvail().x - RunWidth - Style.ItemSpacing.x));
        ImGui::Combo("##Experiment", &ExperimentMode, Modes.data(), static_cast<int>(Modes.size()));
        ImGui::SameLine();
        if (ImGui::Button("Run"))
            Action = ExperimentMode == 0 ? ESurfaceSimulationAction::CpuCheck : ESurfaceSimulationAction::GpuCheck;
        ImGui::SetItemTooltip("Run the applied scene settings from time zero.");
        if (ImGui::Button("Capture State Plot"))
            Action = ESurfaceSimulationAction::Snapshot;
        ImGui::SetItemTooltip("Save a PNG of the current simulation state.");
        ImGui::EndDisabled();

        if (PlotTexture)
        {
            ImGui::SameLine();
            if (ImGui::Button("Enlarge"))
            {
                bExpandPlot = true;
                ImGui::SetWindowFocus("Experiment Plot");
            }
        }
        if (!ExperimentStatus.empty())
            ImGui::TextWrapped("%s", ExperimentStatus.c_str());
        if (PlotTexture)
        {
            const auto  Area = ImGui::GetContentRegionAvail();
            const float PreviewHeight = std::max(64.F, Area.y - ImGui::GetFrameHeightWithSpacing());
            const float Width = std::min(Area.x, PreviewHeight * PlotAspect);
            if (Width > 0.F)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (Area.x - Width) * 0.5F);
                ImGui::Image(reinterpret_cast<ImTextureID>(PlotTexture), {Width, Width / PlotAspect});
                const auto Filename = ExperimentPlot.filename().u8string();
                ImGui::SetItemTooltip("%s\n%s",
                                      ExperimentTitle.c_str(),
                                      reinterpret_cast<const char *>(Filename.c_str()));
            }
            if (ImGui::Button("Copy PNG Path"))
            {
                const auto Path = ExperimentPlot.u8string();
                ImGui::SetClipboardText(reinterpret_cast<const char *>(Path.c_str()));
            }
        }
        return Action;
    }

    void FSurfaceSimulationUI::DrawExpandedExperiment()
    {
        if (!bExpandPlot || !PlotTexture)
            return;

        const auto *Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(Viewport->GetWorkCenter(), ImGuiCond_Appearing, {0.5F, 0.5F});
        ImGui::SetNextWindowSize({Viewport->WorkSize.x * 0.80F, Viewport->WorkSize.y * 0.80F}, ImGuiCond_Appearing);
        if (ImGui::Begin("Experiment Plot",
                         &bExpandPlot,
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::Button("Close") || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                                           ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
                bExpandPlot = false;
            ImGui::SameLine();
            ImGui::TextUnformatted(ExperimentTitle.c_str());

            const auto Area = ImGui::GetContentRegionAvail();
            if (Area.x > 0.F && Area.y > 0.F)
            {
                ImGui::BeginChild("Plot details", Area);
                const auto  Space = ImGui::GetContentRegionAvail();
                const float Width = std::min(Space.x, Space.y * PlotAspect);
                if (Width > 0.F)
                    ImGui::Image(reinterpret_cast<ImTextureID>(PlotTexture), {Width, Width / PlotAspect});
                if (!ExperimentSummary.empty())
                    ImGui::TextWrapped("%s", ExperimentSummary.c_str());
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

#pragma endregion

} // namespace MDSS::SurfaceSimulation
