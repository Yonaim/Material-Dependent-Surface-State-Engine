#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Viewport Panels
// ============================================================
#pragma region 00_Viewport_Panels

    void FDebugUI::DrawViewportPanels(FScene&)
    {
        HoveredViewportId = 0;
        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F || ViewportPanels.empty())
            return;

        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2         WorkspacePos{MainViewport->Pos.x + ViewportWorkspaceRectNormalized.x * DisplaySize.x,
                                  MainViewport->Pos.y + ViewportWorkspaceRectNormalized.y * DisplaySize.y};
        const ImVec2         WorkspaceSize{ViewportWorkspaceRectNormalized.z * DisplaySize.x,
                                   ViewportWorkspaceRectNormalized.w * DisplaySize.y};
        if (WorkspaceSize.x < 1.0F || WorkspaceSize.y < 1.0F)
            return;

        const std::size_t Count = ViewportPanels.size();
        const std::size_t Columns = ViewportLayout == 1 ? Count
                                    : ViewportLayout == 2
                                        ? 1U
                                        : static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<float>(Count))));
        const std::size_t Rows = (Count + Columns - 1U) / Columns;
        // Keep viewport input windows behind the profiling and render-state overlays after a canvas click.
        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus;
        const auto GetViewportModeName = [](Rendering::ERenderViewMode Mode) -> const char*
        {
            const auto Value = static_cast<std::uint32_t>(Mode);
            if (Value < RenderViewModeNames.size())
                return RenderViewModeNames[Value];
            if (Mode >= Rendering::ERenderViewMode::SurfaceStateHeatmap &&
                Mode <= Rendering::ERenderViewMode::OutgoingFluxScale)
                return SurfaceDebugViewNames[Value - static_cast<std::uint32_t>(
                                                         Rendering::ERenderViewMode::SurfaceStateHeatmap)];
            switch (Mode)
            {
                case Rendering::ERenderViewMode::SolverTransferWeight:
                    return "Solver Transfer Weights";
                case Rendering::ERenderViewMode::MesoHeight:
                case Rendering::ERenderViewMode::MesoOffset:
                    return "Meso Geometry";
                case Rendering::ERenderViewMode::MacroGeometry:
                    return "Base Geometry";
                case Rendering::ERenderViewMode::SurfaceTexelGrid:
                    return "Texel Grid";
                case Rendering::ERenderViewMode::SurfaceTexelArea:
                    return "Texel Area Heatmap";
                case Rendering::ERenderViewMode::SurfaceAccumulation:
                    return "State Height Heatmap";
                case Rendering::ERenderViewMode::SurfaceFinalGeometry:
                    return "Meso + State Geometry";
                case Rendering::ERenderViewMode::TotalSimulationHeight:
                    return "Total Simulation Height";
                default:
                    return "Lit";
            }
        };
        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            FViewportPanel&   Panel = ViewportPanels[Index];
            const std::size_t Column = Index % Columns;
            const std::size_t Row = Index / Columns;
            const float       Left =
                WorkspacePos.x + WorkspaceSize.x * static_cast<float>(Column) / static_cast<float>(Columns);
            const float Right =
                WorkspacePos.x + WorkspaceSize.x * static_cast<float>(Column + 1U) / static_cast<float>(Columns);
            const float Top = WorkspacePos.y + WorkspaceSize.y * static_cast<float>(Row) / static_cast<float>(Rows);
            const float Bottom =
                WorkspacePos.y + WorkspaceSize.y * static_cast<float>(Row + 1U) / static_cast<float>(Rows);
            const ImVec2      PanelSize{std::max(Right - Left, 1.0F), std::max(Bottom - Top, 1.0F)};
            const std::string WindowName = "Viewport " + std::to_string(Panel.Id) + "##ViewportPanel";
            ImGui::SetNextWindowPos({Left, Top}, ImGuiCond_Always);
            ImGui::SetNextWindowSize(PanelSize, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0F, 0.0F});
            if (ImGui::Begin(WindowName.c_str(), nullptr, Flags))
            {
                const float  CanvasTop = Top;
                const ImVec2 CanvasSize{PanelSize.x, std::max(Bottom - CanvasTop, 0.0F)};
                Panel.RectNormalized = {(Left - MainViewport->Pos.x) / DisplaySize.x,
                                        (CanvasTop - MainViewport->Pos.y) / DisplaySize.y,
                                        CanvasSize.x / DisplaySize.x,
                                        CanvasSize.y / DisplaySize.y};
                if (CanvasSize.x >= 1.0F && CanvasSize.y >= 1.0F)
                {
                    ImGui::SetCursorScreenPos({Left, CanvasTop});
                    ImGui::InvisibleButton(
                        "##Canvas", CanvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                    if (ImGui::IsItemHovered())
                    {
                        HoveredViewportId = Panel.Id;
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                            ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                            ActiveViewportId = Panel.Id;
                    }
                }
                const ImVec2 PanelMinimum{Left, Top};
                const ImVec2 PanelMaximum{Right, Bottom};
                const bool   bActive = Panel.Id == ActiveViewportId;
                const ImU32  BorderColor = ImGui::GetColorU32(bActive ? ImVec4{0.35F, 0.78F, 1.0F, 0.95F}
                                                                     : ImVec4{0.55F, 0.60F, 0.67F, 0.55F});
                ImDrawList*  DrawList = ImGui::GetWindowDrawList();
                DrawList->AddRect(PanelMinimum, PanelMaximum, BorderColor, 0.0F, 0, bActive ? 2.0F : 1.0F);
                const std::string Label = "View " + std::to_string(Index + 1U) + "  ·  " +
                                          GetViewportModeName(FrameRenderer->GetRenderViewMode(Index));
                const ImVec2 LabelMinimum{Left + 8.0F, Top + 8.0F};
                const ImVec2 LabelTextSize = ImGui::CalcTextSize(Label.c_str());
                const ImVec2 LabelMaximum{LabelMinimum.x + LabelTextSize.x + 12.0F,
                                          LabelMinimum.y + LabelTextSize.y + 8.0F};
                DrawList->AddRectFilled(
                    LabelMinimum, LabelMaximum, ImGui::GetColorU32({0.04F, 0.05F, 0.07F, 0.74F}), 3.0F);
                DrawList->AddText(
                    {LabelMinimum.x + 6.0F, LabelMinimum.y + 4.0F},
                    ImGui::GetColorU32(bActive ? ImVec4{0.72F, 0.88F, 1.0F, 1.0F} : ImVec4{0.86F, 0.88F, 0.91F, 1.0F}),
                    Label.c_str());
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
    }

#pragma endregion

// ============================================================
// 01. Mesh Notices
// ============================================================
#pragma region 01_Mesh_Notices

    void FDebugUI::DrawBrickCubeTexelMeshNotice(const FScene& SceneData)
    {
        if (FrameRenderer == nullptr || AssetManager == nullptr)
            return;
        const auto& Instances = SceneData.GetStaticMeshInstances();
        bool        bBrickCubeTexelMesh = false;
        for (std::size_t Index = 0; Index < Instances.size(); ++Index)
        {
            const auto Mesh = Instances[Index].GetMesh();
            if (Mesh != Asset::InvalidAssetHandle && AssetManager->GetMesh(Mesh).GetName() == "BrickCube" &&
                FrameRenderer->IsLitTexelMeshBaseRendered(SceneData, Index))
            {
                bBrickCubeTexelMesh = true;
                break;
            }
        }
        if (!bBrickCubeTexelMesh)
            return;

        constexpr const char* Message = "LIT 뷰: BrickCube를 텍셀 메시로 렌더링 중";
        const ImVec2          DisplaySize = ImGui::GetIO().DisplaySize;
        const glm::vec4       Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F || Rect.z <= 0.0F || Rect.w <= 0.0F ||
            Rect.z * DisplaySize.x < ImGui::CalcTextSize(Message).x + 36.0F)
            return;
        const ImVec2 Origin = ImGui::GetMainViewport()->Pos;
        const ImVec2 TopRight{Origin.x + (Rect.x + Rect.z) * DisplaySize.x - 10.0F,
                              Origin.y + Rect.y * DisplaySize.y + 10.0F};
        ImGui::SetNextWindowPos(TopRight, ImGuiCond_Always, {1.0F, 0.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("BrickCube Texel Mesh##ViewportNotice", nullptr, Flags))
        {
            ImGui::TextUnformatted(Message);
            if (ImGui::IsItemHovered())
                SetDescriptionTooltip("원본 메시와 비교하려면 Base Geometry 뷰를 선택하세요.");
            bMouseOverViewportOverlay |= ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        }
        ImGui::End();
    }

#pragma endregion

// ============================================================
// 02. Render Settings
// ============================================================
#pragma region 02_Render_Settings

    void FDebugUI::DrawRenderSettingsWindow(FScene& SceneData)
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        if (ImGui::Begin("Render Debug###Render Settings"))
        {
            if (ImGui::BeginTabBar("RenderSettingsTabs"))
            {
                if (ImGui::BeginTabItem("Visual"))
                {
                    if (ImGui::BeginChild(
                            "VisualSettingsContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
                        bool bEffectsChanged = false;
                        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{8.0F, 7.0F});
                        const auto Bindings =
                            Rendering::ResolveDemoSurfaceStates(SurfaceDataManager->GetSurfaceStateRegistry());
                        const auto DrawRamp = [&](Rendering::FDemoStateColorMapping& Mapping)
                        {
                            bEffectsChanged |=
                                LabeledRangeSliderFloat("Color ramp", &Mapping.RampStart, &Mapping.RampEnd);
                        };
                        const auto DrawEffectToggle =
                            [&](const char* Label, bool& bEnabled, bool bAvailable, const char* Description)
                        {
                            ImGui::PushID("Toggle");
                            ImGui::PushID(Label);
                            ImGui::BeginDisabled(!bAvailable);
                            bEffectsChanged |= ImGui::Checkbox(Label, &bEnabled);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip(Description);
                            ImGui::EndDisabled();
                            ImGui::PopID();
                            ImGui::PopID();
                        };
                        const auto DrawEffectSettings = [&](const char* Label,
                                                            bool&       bEnabled,
                                                            bool        bAvailable,
                                                            const char* Description,
                                                            auto&&      DrawSettings)
                        {
                            if (!bEnabled || !bAvailable)
                                return;
                            ImGui::PushID("Details");
                            ImGui::PushID(Label);
                            if (ImGui::CollapsingHeader(Label))
                            {
                                ImGui::Indent();
                                TextDescriptionWrapped(Description);
                                DrawSettings();
                                ImGui::Unindent();
                            }
                            ImGui::PopID();
                            ImGui::PopID();
                            ImGui::Dummy({0.0F, 3.0F});
                        };

                        // Keep all effect toggles together above their optional detail panels.
                        DrawEffectToggle("Heat",
                                         Effects.bHeatLayer,
                                         Bindings.Heat != SurfaceState::InvalidStateId,
                                         "Heat 포화도에 따라 표면 기본색을 색상 램프와 섞습니다. 현재 Heat 프로필은 "
                                         "높이를 쌓지 않습니다.");
                        DrawEffectToggle("WaterFilm",
                                         Effects.bWaterFilmDisplacement,
                                         Bindings.WaterFilm != SurfaceState::InvalidStateId,
                                         "공통 accumulation 윗면에서 아래 표면이 비치는 얇은 막으로 합성합니다.");
                        DrawEffectToggle("Mud",
                                         Effects.bMudDisplacement,
                                         Bindings.Mud != SurfaceState::InvalidStateId,
                                         "공통 불투명 적층 윗면과 경계 옆면을 갈색 진흙 재질로 그립니다.");
                        DrawEffectToggle(
                            "Ice",
                            Effects.bIceDisplacement,
                            Bindings.Ice != SurfaceState::InvalidStateId,
                            "Ice 적층을 푸른 산란색과 서리 표면으로 표시합니다. 녹은 물은 같은 표면 위에 합성됩니다.");
                        DrawEffectToggle("Lava",
                                         Effects.bLavaDisplacement,
                                         Bindings.Lava != SurfaceState::InvalidStateId,
                                         "공통 불투명 적층 윗면과 경계 옆면에 라바색과 자체 발광을 적용합니다.");

                        ImGui::Dummy({0.0F, 5.0F});
                        DrawEffectSettings("Heat",
                                           Effects.bHeatLayer,
                                           Bindings.Heat != SurfaceState::InvalidStateId,
                                           "Heat 포화도에 따라 표면 기본색을 색상 램프와 섞습니다. 현재 Heat 프로필은 "
                                           "높이를 쌓지 않습니다.",
                                           [&]()
                                           {
                                               bEffectsChanged |= LabeledSliderFloat(
                                                   "Strength", &Effects.HeatStrength, 0.0F, 1.0F, "%.2f");
                                               DrawRamp(Effects.HeatColorMap);
                                           });
                        DrawEffectSettings(
                            "WaterFilm",
                            Effects.bWaterFilmDisplacement,
                            Bindings.WaterFilm != SurfaceState::InvalidStateId,
                            "공통 accumulation 윗면에서 아래 표면이 비치는 얇은 막으로 합성합니다.",
                            [&]()
                            {
                                bEffectsChanged |=
                                    ImGui::Checkbox("Teal visibility mode", &Effects.bEmphasizeWaterFilm);
                                if (ImGui::IsItemHovered())
                                    SetDescriptionTooltip("WaterFilm의 색과 불투명도만 강조합니다. 시뮬레이션 상태와 "
                                                          "높이는 바꾸지 않습니다.");
                                bEffectsChanged |=
                                    LabeledSliderFloat("Opacity", &Effects.WaterFilmOpacity, 0.0F, 1.0F, "%.2f");
                                DrawRamp(Effects.WaterFilmColorMap);
                            });
                        DrawEffectSettings(
                            "Mud",
                            Effects.bMudDisplacement,
                            Bindings.Mud != SurfaceState::InvalidStateId,
                            "공통 불투명 적층 윗면과 경계 옆면을 갈색 진흙 재질로 그립니다.",
                            [&]()
                            {
                                bEffectsChanged |=
                                    LabeledSliderFloat("Roughness", &Effects.MudRoughness, 0.05F, 1.0F, "%.2f");
                                bEffectsChanged |=
                                    LabeledSliderFloat("Noise amount", &Effects.MudNoiseAmount, 0.0F, 1.0F, "%.2f");
                                if (ImGui::IsItemHovered())
                                    SetDescriptionTooltip("머드 표면의 절차적 미세요철 강도를 조절합니다. 0이면 "
                                                          "매끈하고 1이면 요철이 가장 강합니다.");
                                DrawRamp(Effects.MudColorMap);
                            });
                        DrawEffectSettings("Lava",
                                           Effects.bLavaDisplacement,
                                           Bindings.Lava != SurfaceState::InvalidStateId,
                                           "공통 불투명 적층 윗면과 경계 옆면에 라바색과 자체 발광을 적용합니다.",
                                           [&]()
                                           {
                                               bEffectsChanged |= LabeledSliderFloat(
                                                   "Lava threshold", &Effects.LavaThreshold, 0.0F, 1.0F, "%.2f");
                                               if (ImGui::IsItemHovered())
                                                   SetDescriptionTooltip(
                                                       "포화도가 이 값 이하이면 Lava 색과 발광을 적용하지 않습니다.");
                                               DrawRamp(Effects.LavaColorMap);
                                           });

                        ImGui::Dummy({0.0F, 4.0F});
                        bEffectsChanged |= LabeledSliderFloat(
                            "Height fade threshold", &Effects.LowAmountHeightFade, 0.0F, 0.5F, "%.2f");
                        if (ImGui::IsItemHovered())
                            SetDescriptionTooltip(
                                "0이면 기존 높이를 그대로 표시합니다. 기준값을 높이면 그보다 농도가 낮은 쌓임 효과의 "
                                "화면 높이를 부드럽게 줄입니다. 시뮬레이션 상태와 이동에는 영향을 주지 않습니다.");

                        ImGui::Dummy({0.0F, 6.0F});
                        if (ImGui::CollapsingHeader("Shading"))
                        {
                            ImGui::Indent();
                            float AmbientLight = FrameRenderer->GetAmbientLight();
                            if (LabeledSliderFloat("Ambient / direct mix", &AmbientLight, 0.0F, 1.0F, "%.2f"))
                                FrameRenderer->SetAmbientLight(AmbientLight);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("0은 직접광 중심, 1은 표면색 기반 주변광 중심으로 섞습니다. 광원 "
                                                      "방향과 색은 현재 고정입니다.");

                            bEffectsChanged |=
                                LabeledSliderFloat("Dry surface roughness", &Effects.DryRoughness, 0.05F, 1.0F, "%.2f");

                            float NormalStrength = FrameRenderer->GetNormalStrength();
                            if (LabeledSliderFloat("Normal Strength", &NormalStrength, 0.0F, 4.0F, "%.2f"))
                                FrameRenderer->SetNormalStrength(NormalStrength);

                            bool bFlipNormalY = FrameRenderer->GetFlipNormalY();
                            if (ImGui::Checkbox("Flip Y", &bFlipNormalY))
                                FrameRenderer->SetFlipNormalY(bFlipNormalY);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("화면의 Normal Map 조명만 바꿉니다. Meso 형상은 기본 Flip Y "
                                                      "켜짐으로 전처리됩니다.");
                            ImGui::Unindent();
                        }
                        ImGui::PopStyleVar();
                        if (bEffectsChanged)
                            FrameRenderer->SetDemoSurfaceEffectSettings(Effects);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Display"))
                {
                    if (ImGui::BeginChild(
                            "DebugSettingsContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        bool bOverlayOnly = FrameRenderer->IsOverlayOnlyDebugEnabled();
                        if (ImGui::Checkbox("Overlay only", &bOverlayOnly))
                        {
                            FrameRenderer->SetOverlayOnlyDebugEnabled(bOverlayOnly);
                            ResetProfilingAverages();
                        }
                        if (ImGui::IsItemHovered())
                            SetDescriptionTooltip("Lit 뷰에서 Base Surface를 숨기고 생성된 State Overlay만 표시합니다. "
                                                  "켜면 표시 모드를 선택할 수 있습니다.");
                        ImGui::BeginDisabled(!bOverlayOnly);
                        {
                            int OverlayDisplayMode = static_cast<int>(FrameRenderer->GetOverlayDisplayMode());
                            ImGui::SetNextItemWidth(180.0F);
                            if (ImGui::Combo("Overlay display", &OverlayDisplayMode, "Both\0Top only\0Sides only\0"))
                            {
                                FrameRenderer->SetOverlayDisplayMode(
                                    static_cast<Rendering::EOverlayDisplayMode>(OverlayDisplayMode));
                                ResetProfilingAverages();
                            }
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("Overlay 전체, 윗면만, 또는 옆면만 표시합니다.");

                            int ShadingDiagnostic = static_cast<int>(FrameRenderer->GetOverlayShadingDiagnostic());
                            ImGui::SetNextItemWidth(180.0F);
                            if (ImGui::Combo("Overlay shading diagnostic",
                                             &ShadingDiagnostic,
                                             "Lit\0Fixed normal\0Normal RGB\0Front/back faces\0Mesh normal\0Vertex "
                                             "normal (old)\0Lit opaque\0State layers\0Mud diffuse\0Mud specular x4\0"))
                                FrameRenderer->SetOverlayShadingDiagnostic(
                                    static_cast<Rendering::EOverlayShadingDiagnostic>(ShadingDiagnostic));
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip(
                                    "Fixed normal은 조명 노멀을 일정하게 유지합니다. Normal RGB는 보간 노멀을, "
                                    "Front/back faces는 삼각형 앞뒷면을, Mesh normal은 원본 메쉬 노멀을, "
                                    "Lit은 texel 노멀을 픽셀별로 보간합니다. Vertex normal (old)는 이전 정점 보간 "
                                    "결과입니다.");
                        }
                        ImGui::EndDisabled();

                        ImGui::Dummy({0.0F, 8.0F});
                        bool bBaseMeshDraw = FrameRenderer->IsBaseMeshDrawEnabled();
                        if (ImGui::Checkbox("Base Mesh draw", &bBaseMeshDraw))
                        {
                            FrameRenderer->SetBaseMeshDrawEnabled(bBaseMeshDraw);
                            bFrameTimeResetRequested = true;
                            ResetProfilingAverages();
                        }
                        bool bOverlayTopDraw = FrameRenderer->IsOverlayTopDrawEnabled();
                        if (ImGui::Checkbox("Overlay Top draw", &bOverlayTopDraw))
                        {
                            FrameRenderer->SetOverlayTopDrawEnabled(bOverlayTopDraw);
                            bFrameTimeResetRequested = true;
                            ResetProfilingAverages();
                        }
                        bool bOverlaySidesDraw = FrameRenderer->IsOverlaySidesDrawEnabled();
                        if (ImGui::Checkbox("Overlay Sides draw", &bOverlaySidesDraw))
                        {
                            FrameRenderer->SetOverlaySidesDrawEnabled(bOverlaySidesDraw);
                            bFrameTimeResetRequested = true;
                            ResetProfilingAverages();
                        }

                        float LitHeightDisplayScale = SceneData.GetLitHeightDisplayScale();
                        if (LabeledSliderFloat("Lit height display scale", &LitHeightDisplayScale, 0.0F, 10.0F, "%.2f"))
                            FrameRenderer->SetSceneLitHeightDisplayScale(SceneData, LitHeightDisplayScale);
                        if (ImGui::IsItemHovered())
                            SetDescriptionTooltip(
                                "Lit 뷰에 표시하는 적층 높이 배율입니다. 시뮬레이션 데이터는 바꾸지 않습니다.");
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Performance"))
                {
                    if (ImGui::BeginChild("RenderPerformanceContent",
                                          {0.0F, 0.0F},
                                          ImGuiChildFlags_None,
                                          ImGuiWindowFlags_NoBackground))
                    {
                        ImGui::Dummy({0.0F, 12.0F});
                        DrawPerformanceTab(true);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

#pragma endregion

} // namespace MDSS
