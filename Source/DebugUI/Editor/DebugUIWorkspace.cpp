/**
 * @file DebugUIWorkspace.cpp
 * @brief 공통 에디터의 씬 탐색, 속성, 시간 제어와 도구를 연결한다.
 */
#include "Application/Editor/SurfaceSimulation/SceneAsset.h"
#include "Application/Editor/SurfaceSimulation/SurfaceSimulation.h"
#include "DebugUI/Core/DebugUIShared.h"
#include "DebugUI/Panels/EditorLayoutStyle.h"
#include <span>

namespace MDSS
{
// ============================================================
// 00. Scene Library Entries
// ============================================================
#pragma region 00_Scene_Library_Entries
    // 에디터의 씬 분류와 표면 객체 표시 이름을 정의한다.
    namespace
    {
        using SurfaceSimulation::EEditorPanel;
        using SurfaceSimulation::EScene;

        enum class ESceneGroup : std::size_t
        {
            GeometryInputs,
            TransportBoundaries,
            MaterialResponse,
            Interactions,
            GeometryCoupling,
            Showcase
        };

        constexpr std::array GroupNames{"Feature: Geometry Inputs",
                                        "Feature: Transport & Boundaries",
                                        "Feature: Material Response",
                                        "Feature: Interactions",
                                        "Feature: State-Driven Geometry Coupling",
                                        "Showcase"};
        constexpr auto       SceneFilters = []
        {
            std::array<const char *, 2 + GroupNames.size()> Filters{"All", "Features"};
            std::ranges::copy(GroupNames, Filters.begin() + 2);
            return Filters;
        }();

        struct FDemoDefinition
        {
            ESceneGroup    Group;
            const char    *Asset;
            const char    *SingleName = nullptr;
            const char    *Condition = nullptr;
            const char8_t *Overview = nullptr;
        };

        // 실제 배포 씬과 선택 조건은 에셋이 소유한다. 목록은 합의한 데모만 순서대로 노출한다.
        using enum ESceneGroup;
        constexpr FDemoDefinition DemoDefinitions[]{
            {GeometryInputs, "Features/HeightMapCavities.Scene"},
            {GeometryInputs, "Features/GeometrySourceComparison.Scene"},
            {GeometryInputs, "Comparisons/WettedContact.Scene"},
            {TransportBoundaries,
                "BrickRunoff.Scene",
                "Tilt-Driven Drainage",
                "Animated Tilt",
                u8"기울기가 바뀌면 물이 홈을 따라 이동한다."},
            {TransportBoundaries, "Features/DrainageGeometryComparison.Scene"},
            {TransportBoundaries, "Features/SillOverflow.Scene"},
            {TransportBoundaries, "Features/BasinSplitting.Scene"},
            {TransportBoundaries, "Features/OutletDrainage.Scene"},
            {TransportBoundaries,
                "SteppedWaterLeveling.Scene",
                "Stepped Water Leveling",
                "Equal Initial Depth",
                u8"양쪽 수심 16 mm로 시작한다. 평형 수면은 22 mm로 같아지고 수심은 22 mm와 10 mm가 된다."},
            {MaterialResponse, "Features/SubstrateAbsorption.Scene"},
            {MaterialResponse, "Features/AbsorptionStorageDepth.Scene"},
            {MaterialResponse, "Comparisons/ThinVsThickSteel.Scene"},
            {MaterialResponse,
                "WaxPhaseVolume.Scene",
                "Wax Phase Volume",
                "Melt Cycle",
                u8"같은 왁스 12 g이 녹으면 상별 밀도 차이로 부피와 높이가 증가한다."},
            {Interactions,
                "SnowOnFabric.Scene",
                "Melting, Absorption & Drying",
                "Flat Fabric",
                u8"눈이 녹아 물막이 되고, 천에 스며든 물이 시간에 따라 마른다."},
            {Interactions, "Comparisons/CharringWood_DryVsPreWetted.Scene"},
            {Interactions, "Features/MudRewetting.Scene"},
            {GeometryCoupling, "Features/MeltGeometryFeedback.Scene"},
            {GeometryCoupling, "Features/DepositionFlowFeedback.Scene"},
            {Showcase, "IceGate.Scene", "Ice Gate Fork", nullptr, u8"얼음 마개가 녹으면 낮은 갈림길이 열린다."},
            {Showcase, "MeadowFirebreak.Scene", "Meadow Firebreak", nullptr, u8"물이 스며든 띠에서 불길이 느려진다."},
            {Showcase,
                "HeatedWaterChannel.Scene",
                "Heated Water Channel",
                nullptr,
                u8"물이 달궈진 수로를 식히고 수증기가 오른다."},
            {Showcase,
                "GlacierCut.Scene",
                "Glacier Cut",
                nullptr,
                u8"강철 수로 위의 눈이 용암에 녹고, 융해수가 하류에 고인다."},
            {Showcase,
                "CanyonCascade.Scene",
                "Canyon Cascade",
                nullptr,
                u8"물이 암반의 웅덩이를 차례로 채우고 넘는다."}};

        struct FSceneEntry
        {
            const FDemoDefinition                             *Definition;
            std::string                                        Name;
            std::filesystem::path                              Path;
            std::optional<SurfaceSimulation::FSceneComparison> Comparison;
        };

        std::vector<FSceneEntry> BuildSceneLibrary()
        {
            const auto               Root = SurfaceSimulation::BundledScenePath(EScene::Runoff).parent_path();
            std::vector<FSceneEntry> Entries;
            Entries.reserve(std::size(DemoDefinitions));
            for (const auto &Definition : DemoDefinitions)
            {
                auto Path = (Root / Definition.Asset).lexically_normal();
                try
                {
                    if (Definition.SingleName)
                        Entries.push_back({&Definition, Definition.SingleName, std::move(Path), {}});
                    else if (auto Comparison = SurfaceSimulation::ReadSceneComparison(Path))
                        Entries.push_back({&Definition, Comparison->Title, std::move(Path), std::move(Comparison)});
                    else
                        FLogger::Error("TDebugUI", "Missing comparison asset: " + Path.string());
                }
                catch (const std::exception &Error)
                {
                    FLogger::Error("TDebugUI", Error.what());
                }
            }
            return Entries;
        }

        const std::vector<FSceneEntry> &SceneLibrary()
        {
            static const auto Scenes = BuildSceneLibrary();
            return Scenes;
        }

        bool MatchesScenePath(const FSceneEntry &Entry, const std::filesystem::path &Path)
        {
            return Entry.Path == Path || (Entry.Comparison && std::ranges::find(Entry.Comparison->Scenes, Path) !=
                                                                  Entry.Comparison->Scenes.end());
        }

        const FSceneEntry *FindSceneEntry(const std::filesystem::path &Path, const std::filesystem::path &Selected = {})
        {
            const auto &Scenes = SceneLibrary();
            const auto  Key = Path.lexically_normal();
            if (!Selected.empty())
            {
                const auto Entry = std::ranges::find(Scenes, Selected, &FSceneEntry::Path);
                if (Entry != Scenes.end() && MatchesScenePath(*Entry, Key))
                    return &*Entry;
            }

            // 직접 연 파일은 단일 카탈로그 항목을 우선한다. 비교의 개별 조건은 선택한 묶음에 남는다.
            const auto Exact = std::ranges::find(Scenes, Key, &FSceneEntry::Path);
            if (Exact != Scenes.end())
                return &*Exact;
            const auto Condition =
                std::ranges::find_if(Scenes, [&](const auto &Entry) { return MatchesScenePath(Entry, Key); });
            return Condition == Scenes.end() ? nullptr : &*Condition;
        }

        const char8_t *Overview(const FSceneEntry &Entry)
        {
            return Entry.Comparison ? reinterpret_cast<const char8_t *>(Entry.Comparison->Description.c_str())
                                    : Entry.Definition->Overview;
        }

        SurfaceSimulation::FSceneAssetText SceneAssetText(const std::filesystem::path &Path,
                                                          const std::filesystem::path &Selected)
        {
            const auto *Entry = FindSceneEntry(Path, Selected);
            return Entry ? SurfaceSimulation::FSceneAssetText{Entry->Name, Overview(*Entry)}
                         : SurfaceSimulation::FSceneAssetText{};
        }

        std::filesystem::path DrawSceneConditions(const FSceneEntry &Entry, const std::filesystem::path &Active)
        {
            if (!Entry.Comparison)
            {
                ImGui::TextUnformatted(Entry.Definition->Condition);
                return {};
            }
            const auto           &Cases = *Entry.Comparison;
            const auto            Case = std::ranges::find(Cases.Scenes, Active);
            const bool            bCompare = Case == Cases.Scenes.end();
            const auto           *Label = bCompare ? "Compare All" : Cases.Labels[Case - Cases.Scenes.begin()].c_str();
            std::filesystem::path Requested;
            ImGui::SetNextItemWidth(-1.F);
            if (ImGui::BeginCombo("##DemoCondition", Label))
            {
                for (std::size_t Index = 0; Index < Cases.Scenes.size(); ++Index)
                {
                    const bool bActive = Cases.Scenes[Index] == Active;
                    if (ImGui::Selectable(Cases.Labels[Index].c_str(), bActive) && !bActive)
                        Requested = Cases.Scenes[Index];
                    if (bActive)
                        ImGui::SetItemDefaultFocus();
                }
                if (ImGui::Selectable("Compare All", bCompare) && !bCompare)
                    Requested = Entry.Path;
                if (bCompare)
                    ImGui::SetItemDefaultFocus();
                ImGui::EndCombo();
            }
            return Requested;
        }

        void DrawSceneOverviewValues(std::span<const std::string_view> Values, const char *Empty)
        {
            if (Values.empty())
            {
                ImGui::TextUnformatted(Empty);
                return;
            }

            for (const auto Value : Values)
            {
                if (Values.size() > 1)
                    ImGui::Bullet();
                ImGui::TextUnformatted(Value.data(), Value.data() + Value.size());
            }
        }

        std::filesystem::path DrawSceneOverview(const SurfaceSimulation::FEditorSession &Session,
                                                const FSceneEntry                       *Entry,
                                                ImFont                                  *BoldLabelFont)
        {
            const auto ActiveTitle = Session.GetComparisonTitle();
            const auto Title = Entry                 ? Entry->Name
                               : ActiveTitle.empty() ? Session.GetScenePath().stem().string()
                                                     : std::string(ActiveTitle);
            ImGui::PushFont(nullptr, EditorLayout::TitleSize);
            ImGui::TextWrapped("%s", Title.c_str());
            ImGui::PopFont();
            ImGui::Separator();
            if (Entry)
                if (const auto *Description = Overview(*Entry))
                    ImGui::TextWrapped("%s", reinterpret_cast<const char *>(Description));

            const auto  Values = Session.GetSceneOverviewValues();
            const char *MaterialLabel = Values.Materials.size() > 1 ? "Surface Materials" : "Surface Material";
            constexpr const char *LayerLabel = "Layer States";
            constexpr const char *SubstrateLabel = "Substrate States";
            ImGui::PushFont(BoldLabelFont, 0.F);
            const float LabelWidth = std::min(std::max({ImGui::CalcTextSize(MaterialLabel).x,
                                                        ImGui::CalcTextSize(LayerLabel).x,
                                                        ImGui::CalcTextSize(SubstrateLabel).x}),
                                              ImGui::GetContentRegionAvail().x * 0.57F);
            ImGui::PopFont();

            // 설명과 모든 속성은 같은 폭을 쓴다. 값이 여러 개면 불릿마다 개별 줄을 차지한다.
            std::filesystem::path Requested;
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {4.F, 1.F});
            if (ImGui::BeginTable("Scene overview", 2, ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                const auto DrawLabel = [&](const char *Label, bool bAlignToFrame)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushFont(BoldLabelFont, 0.F);
                    if (bAlignToFrame)
                        ImGui::AlignTextToFramePadding();
                    ImGui::TextWrapped("%s", Label);
                    ImGui::PopFont();
                    ImGui::TableSetColumnIndex(1);
                };

                DrawLabel(MaterialLabel, false);
                DrawSceneOverviewValues(Values.Materials, "Unspecified");
                DrawLabel(LayerLabel, false);
                DrawSceneOverviewValues(Values.LayerStates, "None");
                DrawLabel(SubstrateLabel, false);
                DrawSceneOverviewValues(Values.SubstrateStates, "None");
                if (Entry && (Entry->Comparison || Entry->Definition->Condition))
                {
                    DrawLabel("Condition", Entry->Comparison.has_value());
                    Requested = DrawSceneConditions(*Entry, Session.GetScenePath());
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
            return Requested;
        }

        void DrawSceneTooltip(const FSceneEntry &Entry)
        {
            if (!ImGui::BeginItemTooltip())
                return;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 30.F * ImGui::GetFontSize());
            if (Entry.Comparison)
                ImGui::TextWrapped("%s (%s)", Entry.Name.c_str(), Entry.Comparison->KoreanTitle.c_str());
            else
                ImGui::TextWrapped("%s", Entry.Name.c_str());
            if (const auto *Description = Overview(Entry))
            {
                ImGui::Spacing();
                ImGui::TextWrapped("%s", reinterpret_cast<const char *>(Description));
            }
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    } // namespace
#pragma endregion

// ============================================================
// 01. Workspace Navigation
// ============================================================
#pragma region 01_Workspace_Navigation
    // 파일 또는 목록에서 씬을 열고 메뉴와 뷰포트 개수를 관리한다.
    void FDebugUI::OpenWorkspaceScene(const std::filesystem::path &Path)
    {
        if (!SurfaceSimulation::IsSurfaceSceneAsset(Path))
            throw std::runtime_error("Expected a SurfaceScene .Scene asset.");
        Simulation.bFrameTimeResetRequested = true;
        SetSurfaceSimulationEnabled(true);
        EditorLab->OpenScene(Path);
        ViewportState.SceneLibrarySelection.clear();
        ResetProfilingAverages();
        ViewportState.SceneStatus = "Loaded: " + Path.filename().string();
        FLogger::Info("TDebugUI", ViewportState.SceneStatus);
    }

    void FDebugUI::BrowseWorkspaceScene()
    {
        Simulation.bFrameTimeResetRequested = true;
        const auto Path = FSceneFileDialog::OpenScene();
        if (!Path)
            return;
        try
        {
            OpenWorkspaceScene(*Path);
        }
        catch (const std::exception &Error)
        {
            ViewportState.SceneStatus = std::string("Load failed: ") + Error.what();
            FLogger::Error("TDebugUI", ViewportState.SceneStatus);
        }
    }

    void FDebugUI::DrawWorkspaceMenu()
    {
        if (!ImGui::BeginMainMenuBar())
            return;
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene..."))
                BrowseWorkspaceScene();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Scenes", nullptr, &ViewportState.bShowLibrary);
            ImGui::MenuItem("Camera", nullptr, &ViewportState.bShowCamera);
            ImGui::MenuItem("Scene Setup", nullptr, &ViewportState.bShowSceneSetup);
            ImGui::MenuItem("Render Options", nullptr, &ViewportState.bShowRenderOptions);
            ImGui::MenuItem("Simulation", nullptr, &ViewportState.bShowDetails);
            if (UsesSurfaceSimulation())
                ImGui::MenuItem("Experiments", nullptr, &ViewportState.bShowExperiments);
            ImGui::MenuItem("Log", nullptr, &ViewportState.bShowLog);
            ImGui::MenuItem("Profiling", nullptr, &ViewportState.bShowProfiling);
            if (!UsesSurfaceSimulation())
                ImGui::MenuItem("Cell Inspector", nullptr, &ViewportState.bShowCellInspector);
            if (ImGui::MenuItem("Reset Workspace Layout"))
            {
                ViewportState.bShowLibrary = true;
                ViewportState.bShowCamera = true;
                ViewportState.bShowSceneSetup = true;
                ViewportState.bShowRenderOptions = true;
                ViewportState.bShowDetails = true;
                ViewportState.bShowExperiments = true;
                ViewportState.bShowLog = true;
                ViewportState.bShowProfiling = true;
                ViewportState.bResetWorkspaceLayout = true;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Capture"))
        {
            if (ImGui::MenuItem("Presentation", "F10"))
                TogglePresentationMode();
            if (ImGui::MenuItem("Active View PNG", nullptr, false, UsesSurfaceSimulation()))
                EditorLab->QueueCapture(SurfaceSimulation::EEditorCapture::ActiveView);
            if (ImGui::MenuItem("All Views PNG", nullptr, false, UsesSurfaceSimulation()))
                EditorLab->QueueCapture(SurfaceSimulation::EEditorCapture::AllViews);
            if (ImGui::MenuItem("State Data & Plot", nullptr, false, UsesSurfaceSimulation()))
                EditorLab->QueueCapture(SurfaceSimulation::EEditorCapture::StateAndPlot);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    void FDebugUI::DrawSceneLibrary(FScene &SceneData)
    {
        if (ViewportState.bShowLibrary)
        {
            if (ImGui::Begin("Scenes", &ViewportState.bShowLibrary))
                DrawSceneChooser();
            ImGui::End();
        }
        if (ViewportState.bShowCamera)
        {
            if (ImGui::Begin("Camera", &ViewportState.bShowCamera))
            {
                if (UsesSurfaceSimulation())
                {
                    EditorLab->DrawPanel(EEditorPanel::Camera);
                }
                else
                {
                    DrawCameraDetails(SceneData);
                    if (SceneData.HasDemoAnimation())
                    {
                        DrawSectionHeader("Animation Track");
                        DrawAnimationControls(SceneData);
                    }
                }
            }
            ImGui::End();
        }
    }

    void FDebugUI::DrawSceneChooser()
    {
        const FSceneEntry    *Active = nullptr;
        const FSceneEntry    *Requested = nullptr;
        std::filesystem::path RequestPath;
        if (UsesSurfaceSimulation())
        {
            const auto &CurrentPath = EditorLab->GetScenePath();
            Active = FindSceneEntry(CurrentPath, ViewportState.SceneLibrarySelection);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16.F, 14.F});
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.F);
            ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
            const bool bDrawSummary =
                ImGui::BeginChild("Scene summary",
                                  {0.F, 0.F},
                                  ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
            if (bDrawSummary)
            {
                RequestPath = DrawSceneOverview(*EditorLab, Active, ViewportState.BoldLabelFont);
                if (!RequestPath.empty())
                    Requested = Active;
            }
            ImGui::EndChild();
            ImGui::Spacing();
        }

        ImGui::SetNextItemWidth(-1.F);
        ImGui::Combo("##SceneCategory",
                     &ViewportState.SceneCategory,
                     SceneFilters.data(),
                     static_cast<int>(SceneFilters.size()));
        if (ViewportState.SceneStatus.starts_with("Load failed:"))
            ImGui::TextWrapped("%s", ViewportState.SceneStatus.c_str());

        if (ImGui::BeginChild("Scene choices"))
        {
            const int Filter = ViewportState.SceneCategory;
            bool      bPreviousGroup = false;
            for (std::size_t Index = 0; Index < GroupNames.size(); ++Index)
            {
                const auto Group = static_cast<ESceneGroup>(Index);
                const bool bShowGroup =
                    Filter == 0 || (Filter == 1 && Group != Showcase) || Filter == static_cast<int>(Index) + 2;
                if (!bShowGroup)
                    continue;
                bool bHeading = false;
                for (const auto &Entry : SceneLibrary())
                {
                    if (Entry.Definition->Group != Group)
                        continue;
                    if (!bHeading)
                    {
                        if (bPreviousGroup)
                            ImGui::Separator();
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_CheckMark));
                        ImGui::TextWrapped("%s", GroupNames[Index]);
                        ImGui::PopStyleColor();
                        bHeading = true;
                        bPreviousGroup = true;
                    }
                    if (ImGui::Selectable(Entry.Name.c_str(), &Entry == Active) && &Entry != Active)
                    {
                        Requested = &Entry;
                        RequestPath = Entry.Path;
                    }
                    DrawSceneTooltip(Entry);
                }
            }
        }
        ImGui::EndChild();
        if (!Requested)
            return;

        try
        {
            OpenWorkspaceScene(RequestPath);
            ViewportState.SceneLibrarySelection = Requested->Path;
            ViewportState.SceneStatus = "Loaded: " + Requested->Name;
        }
        catch (const std::exception &Error)
        {
            ViewportState.SceneStatus = std::string("Load failed: ") + Error.what();
            FLogger::Error("TDebugUI", ViewportState.SceneStatus);
        }
    }
#pragma endregion

// ============================================================
// 02. Workspace Details
// ============================================================
#pragma region 02_Workspace_Details
    void FDebugUI::DrawWorkspaceTimeline(FScene &SceneData)
    {
        if (UsesSurfaceSimulation())
        {
            EditorLab->DrawPanel(EEditorPanel::Timeline);
            return;
        }
        using enum EditorLayout::EPlaybackIcon;
        const bool bPaused = Simulation.bSimulationPaused;
        if (EditorLayout::PlaybackButton("##PlayPause", bPaused ? Play : Pause, bPaused ? "Play" : "Pause"))
        {
            Simulation.bSimulationPaused = !Simulation.bSimulationPaused;
            if (Simulation.bSimulationPaused)
                SceneData.PauseDemoAnimation();
            else
                SceneData.PlayDemoAnimation();
            Simulation.bFrameTimeResetRequested = true;
        }
        ImGui::SameLine();
        if (EditorLayout::PlaybackButton("##Step", Step, "Advance one simulation step"))
        {
            Simulation.bSimulationPaused = true;
            SceneData.PauseDemoAnimation();
            Simulation.bSolverStepRequested = true;
        }
        ImGui::SameLine();
        if (EditorLayout::PlaybackButton("##Restart", Restart, "Restart simulation"))
            RestartScene(SceneData);
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%.2f s", FrameRenderer->GetSimulatedSeconds());
    }

    void FDebugUI::DrawWorkspaceDetails(FScene &SceneData)
    {
        using FTab = std::pair<const char *, EEditorPanel>;
        struct FPanel
        {
            const char           *Title;
            bool                 &Visible;
            std::span<const FTab> Tabs;
        };
        constexpr std::array SolverTabs{FTab{"Solver", EEditorPanel::Simulation}, FTab{"Tools", EEditorPanel::Tools}};
        constexpr std::array SetupTabs{FTab{"Assets", EEditorPanel::Assets},
                                       FTab{"Geometry", EEditorPanel::Object},
                                       FTab{"Conditions", EEditorPanel::Conditions}};
        const std::array     Panels{FPanel{"Simulation", ViewportState.bShowDetails, SolverTabs},
                                    FPanel{"Scene Setup", ViewportState.bShowSceneSetup, SetupTabs}};
        const bool           bSurface = UsesSurfaceSimulation();
        for (const auto &Window : Panels)
        {
            if (!Window.Visible)
                continue;
            if (ImGui::Begin(Window.Title, &Window.Visible) && ImGui::BeginTabBar("SettingsTabs"))
            {
                for (const auto &[Label, Panel] : Window.Tabs)
                {
                    if ((!bSurface && (Panel == EEditorPanel::Assets || Panel == EEditorPanel::Conditions)) ||
                        !ImGui::BeginTabItem(Label))
                        continue;

                    const bool  bDrawApply = bSurface && Panel != EEditorPanel::Tools;
                    const float Footer = bDrawApply ? ImGui::GetFrameHeightWithSpacing() * 2.F : 0.F;
                    if (ImGui::BeginChild(Label, {0.F, -Footer}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        if (bSurface && Panel == EEditorPanel::Assets)
                            EditorLab->DrawPanel(
                                Panel,
                                SceneAssetText(EditorLab->GetScenePath(), ViewportState.SceneLibrarySelection));
                        else if (bSurface)
                            EditorLab->DrawPanel(Panel);
                        else if (Panel == EEditorPanel::Simulation)
                        {
                            DrawSimulationSettingsTab(SceneData);
                            DrawSolverTab();
                            DrawSimulationCommonControls();
                        }
                        else if (Panel == EEditorPanel::Object)
                        {
                            const auto &Objects = SceneData.GetStaticMeshInstances();
                            const auto  Name = [](const auto &Object)
                            { return Object.GetId().empty() ? Object.GetMeshPath().stem().string() : Object.GetId(); };
                            const auto Preview = Gizmo.SelectedObject && *Gizmo.SelectedObject < Objects.size()
                                                     ? Name(Objects[*Gizmo.SelectedObject])
                                                     : std::string("Select a mesh");
                            if (ImGui::BeginCombo("Mesh", Preview.c_str()))
                            {
                                for (std::size_t Index = 0; Index < Objects.size(); ++Index)
                                {
                                    ImGui::PushID(static_cast<int>(Index));
                                    if (ImGui::Selectable(Name(Objects[Index]).c_str(), Gizmo.SelectedObject == Index))
                                        Gizmo.SelectedObject = Index;
                                    ImGui::PopID();
                                }
                                ImGui::EndCombo();
                            }
                            if (Gizmo.SelectedObject)
                                DrawSelectedObjectDetails(SceneData);
                        }
                        else
                            DrawContactInputTab();
                    }
                    ImGui::EndChild();
                    if (bDrawApply)
                        EditorLab->DrawPanel(EEditorPanel::ApplySettings);
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        if (!UsesSurfaceSimulation() && ViewportState.bShowCellInspector)
        {
            if (ImGui::Begin("Cell Inspector", &ViewportState.bShowCellInspector))
                DrawTexelInspectorTab();
            ImGui::End();
        }
    }
#pragma endregion

// ============================================================
// 03. Experiments and Log
// ============================================================
#pragma region 03_Experiments_and_Log
    void FDebugUI::DrawWorkspaceTools()
    {
        if (UsesSurfaceSimulation() && ViewportState.bShowExperiments)
        {
            if (ImGui::Begin("Experiments", &ViewportState.bShowExperiments))
            {
                EditorLab->DrawPanel(EEditorPanel::Experiments);
            }
            ImGui::End();
        }
        if (ViewportState.bShowLog)
        {
            if (ImGui::Begin("Log", &ViewportState.bShowLog))
                DrawLogContents();
            ImGui::End();
        }
        if (UsesSurfaceSimulation())
            EditorLab->DrawExperimentPlot();
    }
#pragma endregion
} // namespace MDSS
