/** @file DebugUIViewSettings.cpp
 * @brief 활성 진단 뷰의 설정 popup과 legend를 그린다.
 */
#include "DebugUI/Core/DebugUIShared.h"
namespace MDSS
{
// ============================================================
// 00. Active View Settings
// ============================================================
#pragma region "00_Active_View_Settings"
// 선택한 view mode의 설정 popup과 공통 제어를 표시한다.
    void FDebugUI::DrawViewSettingsPopup(std::size_t                 ActiveViewportIndex,
                                         Rendering::ERenderViewMode& CurrentMode,
                                         const char*                 CurrentName)
    {
        if (!ImGui::BeginPopup("ViewSettingsPopup"))
        {
            return;
        }

        ImGui::TextColored({0.46F, 0.68F, 0.92F, 1.0F}, "%s", CurrentName);
        ImGui::Separator();
        DrawActiveViewModeSettings(ActiveViewportIndex, CurrentMode);
        ImGui::EndPopup();
    }

    void FDebugUI::DrawActiveViewModeSettings(std::size_t ActiveViewportIndex,
                                              Rendering::ERenderViewMode& CurrentMode)
    {


            // 현재 view mode에 해당하는 파라미터만 편집한다.

            switch (CurrentMode)
            {
                case Rendering::ERenderViewMode::Lit:
                    DrawViewModeContext(CurrentMode, "LIT", "재질색 × 조명(노멀 맵 포함).");
                    break;
                case Rendering::ERenderViewMode::Unlit:
                    DrawViewModeContext(CurrentMode, "UNLIT", "조명 없이 재질색만 표시.");
                    break;
                case Rendering::ERenderViewMode::Wireframe:
                {
                    DrawWireframeViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::VertexNormalWS:
                    DrawViewModeContext(CurrentMode, "VERTEX NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                    break;
                case Rendering::ERenderViewMode::NormalTextureTS:
                    DrawViewModeContext(CurrentMode, "NORMAL TEXTURE", "RGB 채널은 탄젠트 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                    break;
                case Rendering::ERenderViewMode::MappedNormalWS:
                    DrawViewModeContext(CurrentMode, "MAPPED NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                    break;
                case Rendering::ERenderViewMode::SurfaceStateHeatmap:
                {
                    DrawStateHeatmapViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::SurfaceAccumulation:
                case Rendering::ERenderViewMode::SurfaceFinalGeometry:
                {
                    DrawStateGeometryViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::TotalSimulationHeight:
                {
                    DrawViewModeContext(CurrentMode, "TOTAL SIMULATION HEIGHT",
                                     "Meso와 모든 State의 현재 높이를 적용한 형상입니다. "
                                     "높이 캐시는 시뮬레이션 상태나 메시 회전·스케일이 바뀌면 갱신됩니다.");
                    auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    ImGui::SetNextItemWidth(95.0F);

                    if (ImGui::DragFloat("Display scale",
                                         &Settings.DisplacementScale,
                                         0.1F,
                                         1e-8F,
                                         1e6F,
                                         "%.3gx",
                                         ImGuiSliderFlags_AlwaysClamp))
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    DrawHeightGridControls();
                    break;
                }

                case Rendering::ERenderViewMode::SurfaceValidity:
                    DrawViewModeContext(CurrentMode, "VALIDITY", "Surface 텍셀 유효 여부와 시뮬레이션 적용 여부.");
                    DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "유효·시뮬레이션 켜짐");
                    DrawLegendColor({0.18F, 0.48F, 0.82F, 1.0F}, "유효·시뮬레이션 꺼짐");
                    DrawLegendColor({0.86F, 0.12F, 0.08F, 1.0F}, "무효");
                    break;
                case Rendering::ERenderViewMode::SurfaceID:
                    DrawViewModeContext(CurrentMode, "SURFACE ID", "색으로 Surface를 구분합니다. 색상 자체는 수치 의미가 없습니다.");
                    break;
                case Rendering::ERenderViewMode::NeighborCount:
                    DrawViewModeContext(CurrentMode, "NEIGHBOR COUNT", "유효한 이웃 텍셀 개수.");
                    DrawLegendColor({0.08F, 0.10F, 0.18F, 1.0F}, "0개");
                    DrawLegendColor({0.95F, 0.72F, 0.12F, 1.0F}, "8개");
                    break;
                case Rendering::ERenderViewMode::SurfaceSeam:
                    DrawViewModeContext(CurrentMode, "UV SEAM", "다른 UV chart로 이어지는 이웃 관계가 있는 경계.");
                    DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "Seam 이웃 있음");
                    DrawLegendColor({0.12F, 0.16F, 0.22F, 1.0F}, "없음");
                    break;
                case Rendering::ERenderViewMode::OutgoingFluxScale:
                {
                    DrawOutgoingFluxViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::SolverTransferWeight:
                {
                    DrawTransferWeightViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::SurfaceTexelGrid:
                {
                    DrawTexelGridViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::SurfaceTexelArea:
                {
                    DrawTexelAreaViewSettings(CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::MesoHeight:
                case Rendering::ERenderViewMode::MesoOffset:
                {
                    DrawMesoGeometryViewSettings(ActiveViewportIndex, CurrentMode);
                    break;
                }

                case Rendering::ERenderViewMode::MacroGeometry:
                    DrawViewModeContext(CurrentMode, "BASE GEOMETRY",
                                     "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                    break;
            }
    }


    void FDebugUI::DrawViewModeContext(Rendering::ERenderViewMode CurrentMode,
                                       const char* Title,
                                       const char* Description)
    {
        const ImVec4 ContextTitleColor{0.24F, 0.48F, 0.82F, 1.0F};

                ImGui::TextColored(ContextTitleColor, "%s", Title);
                ImGui::TextWrapped("%s", Description);
                ImGui::PushTextWrapPos(0.0F);
                ImGui::TextDisabled("형상 기준: %s", GetViewGeometryDescription(CurrentMode));
                ImGui::PopTextWrapPos();
                ImGui::Spacing();
                }

    void FDebugUI::DrawHeightGridControls()
    {

                auto        Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                int         GridMode = static_cast<int>(Settings.HeightGridMode);
                const char* Modes[] = {"Off", "Overlay", "Grid only"};
                bool        bChanged = ImGui::Combo("Height grid", &GridMode, Modes, 3);
                Settings.HeightGridMode = static_cast<std::uint32_t>(GridMode);
                ImGui::SameLine();
                int BlockSize = static_cast<int>(Settings.HeightGridBlockSize);
                ImGui::SetNextItemWidth(130.0F);
                bChanged |= SliderIntWithDoubleClickInput("Cell texels", &BlockSize, 1, 64);
                Settings.HeightGridBlockSize = static_cast<std::uint32_t>(BlockSize);

                if (bChanged)
                {
                    FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                }
                }

    void FDebugUI::DrawWireframeViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "WIREFRAME", "삼각형 와이어프레임으로 형상을 표시.");
                    bool bUniformWhite = FrameRenderer->IsWireframeUniformWhite();

                    if (ImGui::Checkbox("Uniform white lines", &bUniformWhite))
                    {
                        FrameRenderer->SetWireframeUniformWhite(bUniformWhite);
                    }

                    if (FrameRenderer->SupportsWireframeLineWidth())
                    {
                        ImGui::SameLine(0.0F, 16.0F);
                        float Width = FrameRenderer->GetWireframeLineWidth();
                        ImGui::SetNextItemWidth(180.0F);

                        if (SliderFloatWithDoubleClickInput("Line width",
                                                            &Width,
                                                            FrameRenderer->GetWireframeLineWidthMin(),
                                                            FrameRenderer->GetWireframeLineWidthMax(),
                                                            "%.1f px"))
                        {
                            FrameRenderer->SetWireframeLineWidth(Width);
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Line width adjustment is unsupported by this GPU.");
                    }

                        }


    void FDebugUI::DrawStateHeatmapViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "STATE HEATMAP",
                                     "프로파일 용량 대비 State 비율을 표시합니다. 100% 초과는 주황색이며, 실제 "
                                     "양은 Texel Inspector에서 확인할 수 있습니다.");
                    bool bReliefShadingEnabled = FrameRenderer->IsStateHeatmapReliefShadingEnabled();

                    if (ImGui::Checkbox("Relief Shading", &bReliefShadingEnabled))
                    {
                        FrameRenderer->SetStateHeatmapReliefShadingEnabled(bReliefShadingEnabled);
                    }

                    ImGui::NewLine();
                    DrawLegendColor({0.075F, 0.090F, 0.260F, 1.0F}, "0");
                    DrawLegendColor({0.990F, 0.900F, 0.200F, 1.0F}, "100% 용량");
                    DrawLegendColor({1.0F, 0.25F, 0.05F, 1.0F}, "> 100%");
                    DrawLegendColor({0.42F, 0.42F, 0.45F, 1.0F}, "State 미지원");
                    DrawLegendColor({0.18F, 0.20F, 0.24F, 1.0F}, "프로파일 미할당");
                    DrawLegendColor({1, 0, 1, 1}, "Invalid value / area");
                        }


    void FDebugUI::DrawStateGeometryViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    const bool bFinal = CurrentMode == Rendering::ERenderViewMode::SurfaceFinalGeometry;
                    DrawViewModeContext(CurrentMode, bFinal ? "MESO + STATE GEOMETRY" : "STATE HEIGHT HEATMAP",
                                     bFinal ? "Meso와 선택 State의 높이를 적용한 형상입니다. 선택 State "
                                              "미리보기이며 모든 State의 최종 합성이 아닙니다."
                                            : "형상에는 Meso와 선택 State 전체 높이를 적용합니다. 색은 선택한 적층 "
                                              "성분을 표시하며, UV chart 경계는 열려 있습니다.");
                    auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    bool bChanged = false;
                    ImGui::NewLine();
                    ImGui::SetNextItemWidth(95.0F);
                    bChanged |= ImGui::DragFloat("Display scale",
                                                 &Settings.DisplacementScale,
                                                 0.1F,
                                                 1e-8F,
                                                 1e6F,
                                                 "%.3gx",
                                                 ImGuiSliderFlags_AlwaysClamp);
                    if (bFinal)
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("Meso + selected State; scale affects display only");
                    }
                    else
                    {
                        ImGui::SameLine();
                        ImGui::TextDisabled("형상 배율; 색상 기준은 유지");

                        if (Settings.AccumulationComponent < 3)
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled("프로파일 용량 기준");
                            DrawLegendColor({1, 0.25F, 0.05F, 1}, "> 용량 기준");
                        }
                        else
                        {
                            ImGui::SameLine();
                            ImGui::TextDisabled(Settings.AccumulationComponent == 3
                                                    ? "Cavity Fill: 0-100%%; 초과 높이는 Following에 포함"
                                                    : "채워진 cavity만 표시");
                        }
                    }

                    if (bChanged)
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    ImGui::NewLine();
                    DrawHeightGridControls();

                    if (!bFinal)
                    {
                        if (Settings.AccumulationComponent == 4)
                        {
                            DrawLegendColor({0.12F, 0.13F, 0.15F, 1}, "채워진 cavity 없음");
                        }
                        else
                        {
                            DrawLegendColor({0.075F, 0.090F, 0.260F, 1}, "0");
                        }

                        DrawLegendColor({0.990F, 0.900F, 0.200F, 1},
                                        Settings.AccumulationComponent == 4
                                            ? "채워진 cavity"
                                            : (Settings.AccumulationComponent == 3 ? "100%" : "용량 기준"));
                        DrawLegendColor({1, 0, 1, 1}, "Invalid value / area");
                    }

                        }


    void FDebugUI::DrawOutgoingFluxViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "OUTGOING FLUX SCALE", "선택 State 채널의 유출 제한값: 0은 제한, 1은 제한 없음.");
                    const SurfaceState::FSurfaceStateRegistry* Registry =
                        AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                    if (Registry != nullptr && Registry->GetStateCount() > 0)
                    {
                        const std::size_t StateCount = Registry->GetStateCount();
                        Simulation.DebugState = std::min<SurfaceState::FStateId>(
                            Simulation.DebugState, static_cast<SurfaceState::FStateId>(StateCount - 1));
                        ImGui::AlignTextToFramePadding();
                        ImGui::TextUnformatted("State");
                        ImGui::SameLine(0.0F, 6.0F);
                        ImGui::SetNextItemWidth(125.0F);

                        if (ImGui::BeginCombo("##OutgoingFluxState",
                                              Registry->GetStateName(Simulation.DebugState).c_str()))
                        {
                            for (std::size_t Index = 0; Index < StateCount; ++Index)
                            {
                                const SurfaceState::FStateId State = static_cast<SurfaceState::FStateId>(Index);
                                const bool                   bSelected = State == Simulation.DebugState;

                                if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                                {
                                    Simulation.DebugState = State;
                                    FrameRenderer->SetDebugStateChannel(Simulation.DebugState);
                                }

                                if (bSelected)
                                {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }
                    }

                    DrawLegendColor({0.76F, 0.08F, 0.10F, 1.0F}, "0 제한");
                    DrawLegendColor({1.00F, 0.70F, 0.10F, 1.0F}, "0.5 부분 제한");
                    DrawLegendColor({0.34F, 0.86F, 0.28F, 1.0F}, "1 제한 없음");
                    DrawLegendColor({0.42F, 0.42F, 0.45F, 1.0F}, "시뮬레이션 꺼짐");
                        }


    void FDebugUI::DrawTransferWeightViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "TRANSFER WEIGHT", "선택 가중치 평균: 0은 차단, 1은 완전 전달.");
                    int SelectedWeightView = static_cast<int>(FrameRenderer->GetSolverTransferWeightView());
                    ImGui::SetNextItemWidth(205.0F);

                    if (ImGui::Combo("##TransferWeightComponent",
                                     &SelectedWeightView,
                                     SolverTransferWeightViewNames.data(),
                                     static_cast<int>(SolverTransferWeightViewNames.size())))
                    {
                        FrameRenderer->SetSolverTransferWeightView(
                            static_cast<Rendering::ESolverTransferWeightView>(SelectedWeightView));
                    }

                    ImGui::SameLine(0.0F, 10.0F);
                    DrawLegendColor({0.16F, 0.035F, 0.24F, 1.0F}, "0 차단");
                    DrawLegendColor({0.30F, 0.24F, 0.78F, 1.0F}, "0.5 중간");
                    DrawLegendColor({0.18F, 0.94F, 0.98F, 1.0F}, "1 허용");
                        }


    void FDebugUI::DrawTexelGridViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "TEXEL GRID", "시뮬레이션 UV 격자. 확대하면 개별 텍셀 경계가 나타납니다.");
                    const std::uint32_t                  BlockSize = FrameRenderer->GetTexelGridBlockSize();
                    int                                  SelectedBlock = BlockSize == 16U ? 1 : 0;
                    constexpr std::array<const char*, 2> BlockNames = {"8 x 8 texels", "16 x 16 texels"};
                    ImGui::SetNextItemWidth(150.0F);

                    if (ImGui::Combo(
                            "##TexelGridBlock", &SelectedBlock, BlockNames.data(), static_cast<int>(BlockNames.size())))
                    {
                        FrameRenderer->SetTexelGridBlockSize(SelectedBlock == 0 ? 8U : 16U);
                    }

                    ImGui::SameLine(0.0F, 10.0F);
                    ImGui::Text("Resolution: %u x %u",
                                FrameRenderer->GetSimulationResolution(),
                                FrameRenderer->GetSimulationResolution());
                    DrawLegendColor({0.42F, 0.46F, 0.51F, 1.0F}, "1 텍셀");
                    DrawLegendColor({0.72F, 0.88F, 0.98F, 1.0F}, "묶음 경계");
                        }


    void FDebugUI::DrawTexelAreaViewSettings(Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "TEXEL AREA", "텍셀당 월드 면적. 해상도 변경에도 동일 색 기준.");
                    float Reference = FrameRenderer->GetTexelAreaReference();
                    ImGui::SetNextItemWidth(150.0F);

                    if (ImGui::DragFloat("Reference",
                                         &Reference,
                                         Reference * 0.05F,
                                         1.0e-12F,
                                         1.0e12F,
                                         "%.6g",
                                         ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp))
                    {
                        FrameRenderer->SetTexelAreaReference(Reference);
                    }

                    ImGui::SameLine(0.0F, 10.0F);
                    ImGui::TextDisabled("world units^2 / texel");
                    DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "1/4x 이하 (촘촘)");
                    DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "1x 기준");
                    DrawLegendColor({0.92F, 0.18F, 0.12F, 1.0F}, "4x 이상 (성김)");
                    DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "면적 계산 불가");
                        }


    void FDebugUI::DrawMesoGeometryViewSettings(std::size_t ActiveViewportIndex, Rendering::ERenderViewMode& CurrentMode)
    {
                    DrawViewModeContext(CurrentMode, "MESO GEOMETRY", "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.");
                    int Representation = CurrentMode == Rendering::ERenderViewMode::MesoHeight ? 0 : 1;
                    ImGui::SetNextItemWidth(150.0F);

                    if (ImGui::Combo("Representation", &Representation, "Height Color\0Displacement\0"))
                    {
                        CurrentMode = Representation == 0 ? Rendering::ERenderViewMode::MesoHeight
                                                          : Rendering::ERenderViewMode::MesoOffset;
                        FrameRenderer->SetRenderViewMode(ActiveViewportIndex, CurrentMode);
                    }

                    if (CurrentMode == Rendering::ERenderViewMode::MesoHeight)
                    {
                        ImGui::SameLine(0.0F, 10.0F);
                        DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "음수");
                        DrawLegendColor({0.12F, 0.13F, 0.17F, 1.0F}, "0 기준");
                        DrawLegendColor({1.0F, 0.42F, 0.10F, 1.0F}, "양수");
                    }

                    DrawHeightGridControls();
                        }


#pragma endregion
}
