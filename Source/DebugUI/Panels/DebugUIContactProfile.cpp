#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
    // ============================================================
    // 00. Contact Input
    // ============================================================
    #pragma region 00_Contact_Input

    void TDebugUI::DrawContactInputTab()
    {
        ImGui::Checkbox("Inject mode", &bInjectMode);
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("십자선이 가리키는 지점에 Space 키로 주입합니다.");
        }
        ImGui::TextDisabled("Crosshair hit → triangle → UV → simulation texel.");

        const SurfaceState::TSurfaceStateRegistry* InjectRegistry =
            AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
        const std::size_t InjectStateCount = InjectRegistry != nullptr ? InjectRegistry->GetStateCount() : 0;
        if (InjectStateCount == 0)
        {
            ImGui::TextDisabled("No Surface States are registered.");
            InjectState = SurfaceState::InvalidStateId;
        }
        else
        {
            if (InjectState >= InjectStateCount)
            {
                InjectState = 0;
            }
            const std::string& SelectedName = InjectRegistry->GetStateName(InjectState);
            const bool         bStateOpen = BeginLabeledCombo("State", SelectedName.c_str());
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("주입할 Surface State 채널입니다.");
            }
            if (bStateOpen)
            {
                for (std::size_t Index = 0; Index < InjectStateCount; ++Index)
                {
                    const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                    const std::string&           Name = InjectRegistry->GetStateName(State);
                    const bool                   bSelected = State == InjectState;
                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        InjectState = State;
                    }
                    if (bSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                EndLabeledCombo();
            }
        }

        LabeledSliderFloat("Radius (world)", &InjectRadius, 0.01F, 2.0F, "%.2f");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("주입 접촉의 월드 공간 반경입니다.");
        }
        LabeledSliderFloat("Strength", &InjectStrength, 0.0F, 2.0F, "%.2f");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("기준 면적당 입력량입니다. 총량은 영향을 받는 월드 면적에 따라 결정됩니다.");
        }
        LabeledSliderFloat("Falloff", &InjectFalloff, 0.0F, 4.0F, "%.2f");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("반경 가장자리로 갈수록 주입 강도가 감소하는 정도를 조절합니다.");
        }
        const char* OperationPreview =
            InjectOperation == SurfaceState::ESurfaceContactOperation::Remove ? "Remove" : "Add";
        if (BeginLabeledCombo("Operation", OperationPreview))
        {
            if (ImGui::Selectable("Add", InjectOperation == SurfaceState::ESurfaceContactOperation::Add))
                InjectOperation = SurfaceState::ESurfaceContactOperation::Add;
            if (ImGui::Selectable("Remove", InjectOperation == SurfaceState::ESurfaceContactOperation::Remove))
                InjectOperation = SurfaceState::ESurfaceContactOperation::Remove;
            EndLabeledCombo();
        }

        const char* ShapePreview = "Circle";
        switch (InjectShape)
        {
            case SurfaceState::ESurfaceContactShape::Ellipse:
                ShapePreview = "Ellipse";
                break;
            case SurfaceState::ESurfaceContactShape::Rectangle:
                ShapePreview = "Rectangle";
                break;
            case SurfaceState::ESurfaceContactShape::Capsule:
                ShapePreview = "Capsule";
                break;
            case SurfaceState::ESurfaceContactShape::Circle:
            default:
                break;
        }
        if (BeginLabeledCombo("Shape", ShapePreview))
        {
            if (ImGui::Selectable("Circle", InjectShape == SurfaceState::ESurfaceContactShape::Circle))
                InjectShape = SurfaceState::ESurfaceContactShape::Circle;
            if (ImGui::Selectable("Ellipse", InjectShape == SurfaceState::ESurfaceContactShape::Ellipse))
                InjectShape = SurfaceState::ESurfaceContactShape::Ellipse;
            if (ImGui::Selectable("Rectangle", InjectShape == SurfaceState::ESurfaceContactShape::Rectangle))
                InjectShape = SurfaceState::ESurfaceContactShape::Rectangle;
            if (ImGui::Selectable("Capsule", InjectShape == SurfaceState::ESurfaceContactShape::Capsule))
                InjectShape = SurfaceState::ESurfaceContactShape::Capsule;
            EndLabeledCombo();
        }
        if (InjectShape != SurfaceState::ESurfaceContactShape::Circle)
        {
            const char* SecondaryLabel = InjectShape == SurfaceState::ESurfaceContactShape::Capsule
                                             ? "Capsule half length"
                                             : "Second half extent";
            LabeledSliderFloat(SecondaryLabel, &InjectSecondaryRadius, 0.01F, 2.0F, "%.2f");
            LabeledSliderFloat("Rotation (degrees)", &InjectRotationDegrees, -180.0F, 180.0F, "%.0f");
        }
        BeginLabeledControlRow("Texel search");
        ImGui::SetNextItemWidth(-1.0F);
        SliderIntWithDoubleClickInput("##InjectTexelSearchRadius", &InjectTexelSearchRadius, 0, 16, "%d texels");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("같은 삼각형 안에서 UV 대체 검색을 수행할 축별 범위입니다. 기본값은 2이며, 0이면 "
                                  "대체 검색을 사용하지 않습니다.");
        }
    }

    #pragma endregion

    // ============================================================
    // 01. Profile Tuning
    // ============================================================
    #pragma region 01_Profile_Tuning

    void TDebugUI::DrawProfileTuningTab(TScene& SceneData)
    {
        std::vector<Asset::TSRProfileAssetHandle> SceneProfiles;
        for (const TStaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
        {
            const SurfaceState::TSurfaceRuntimeDataHandle SurfaceData = Instance.GetSurfaceData();
            if (!SurfaceDataManager->HasSurfaceData(SurfaceData))
            {
                continue;
            }
            for (Asset::TSRProfileAssetHandle Profile : SurfaceDataManager->GetSurfaceProfileTable(SurfaceData))
            {
                if (std::find(SceneProfiles.begin(), SceneProfiles.end(), Profile) == SceneProfiles.end())
                {
                    SceneProfiles.push_back(Profile);
                }
            }
        }

        if (SceneProfiles.empty())
        {
            ImGui::TextDisabled("The current scene has no Surface Profiles.");
            return;
        }
        if (std::find(SceneProfiles.begin(), SceneProfiles.end(), DebugParameterProfile) == SceneProfiles.end())
        {
            DebugParameterProfile = SceneProfiles.front();
            ParameterDraftKey = {Asset::InvalidAssetHandle, SurfaceState::InvalidStateId};
        }

        const auto GetProfileName = [this](Asset::TSRProfileAssetHandle Handle) -> const char*
        { return AssetManager->GetSRProfile(Handle).GetName().c_str(); };
        const bool bProfileComboOpen = BeginLabeledCombo("Profile", GetProfileName(DebugParameterProfile));
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("현재 씬에 사용 중인 Surface Profile 중 조정할 프로필을 선택합니다.");
        if (bProfileComboOpen)
        {
            for (Asset::TSRProfileAssetHandle Profile : SceneProfiles)
            {
                const bool bSelected = Profile == DebugParameterProfile;
                if (ImGui::Selectable(GetProfileName(Profile), bSelected))
                {
                    DebugParameterProfile = Profile;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            EndLabeledCombo();
        }

        const auto& ReactionRules = AssetManager->GetSRProfile(DebugParameterProfile).GetData().Reactions;
        if (!ReactionRules.empty() && ImGui::TreeNode("ReactionRules", "Reaction rules (%zu)", ReactionRules.size()))
        {
            ImGui::TextWrapped("Conditions use fixed reference-area amounts. Edit the .SRProfile and reload the scene "
                               "to change rules.");
            for (const auto& R : ReactionRules)
            {
                ImGui::PushID(R.ID.c_str());
                if (ImGui::TreeNode("Rule", "%s [%s]", R.ID.c_str(), R.Enabled ? "enabled" : "disabled"))
                {
                    for (const auto& T : R.Consumes)
                        ImGui::Text("Consume %s x %.4g", T.State.c_str(), T.Coefficient);
                    for (const auto& T : R.Produces)
                        ImGui::Text("Produce %s x %.4g", T.State.c_str(), T.Coefficient);
                    if (R.Mode == SurfaceState::TReactionMode::Continuous)
                        ImGui::Text("Rate coefficient: %.4g", R.RateCoefficient);
                    else
                        ImGui::Text("Fresh inflow fraction: %.4g", R.Fraction);
                    for (const auto& M : R.Modifiers)
                        ImGui::Text(
                            "Condition %s / threshold %.4g / scale %.4g", M.State.c_str(), M.Threshold, M.Scale);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        const SurfaceState::TSurfaceStateRegistry& Registry = SurfaceDataManager->GetSurfaceStateRegistry();
        const std::size_t                          StateCount = Registry.GetStateCount();
        if (StateCount == 0)
        {
            ImGui::TextDisabled("No State channels are registered.");
            return;
        }
        if (DebugParameterState >= StateCount)
        {
            DebugParameterState = 0;
        }
        const std::string& StateName = Registry.GetStateName(DebugParameterState);
        const bool         bStateComboOpen = BeginLabeledCombo("State", StateName.c_str());
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("선택한 프로필에서 조정할 State 채널을 선택합니다.");
        if (bStateComboOpen)
        {
            for (std::size_t Index = 0; Index < StateCount; ++Index)
            {
                const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                const std::string&           Name = Registry.GetStateName(State);
                const bool                   bSelected = State == DebugParameterState;
                if (ImGui::Selectable(Name.c_str(), bSelected))
                {
                    DebugParameterState = State;
                }
                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            EndLabeledCombo();
        }

        const std::pair<Asset::TSRProfileAssetHandle, SurfaceState::TStateId> Key{DebugParameterProfile,
                                                                                  DebugParameterState};
        if (ParameterDraftKey != Key)
        {
            ParameterStatus.clear();
            if (bParameterDraftAvailable)
            {
                ParameterDrafts[ParameterDraftKey] = ParameterDraft;
            }
            ParameterDraftKey = Key;
            bParameterDraftDirty = DirtyParameterDrafts.contains(Key);
            bParameterDraftAvailable = false;

            const auto SavedDraft = ParameterDrafts.find(Key);
            if (SavedDraft != ParameterDrafts.end())
            {
                ParameterDraft = SavedDraft->second;
                bParameterDraftAvailable = true;
            }
            else
            {
                const SurfaceState::TRegisteredSurfaceResponseProfileData Resolved =
                    Registry.ResolveProfile(AssetManager->GetSRProfile(DebugParameterProfile).GetData());
                if (DebugParameterState < Resolved.States.size() && Resolved.States[DebugParameterState].has_value())
                {
                    const auto Override = RuntimeProfileOverrides.find(Key);
                    ParameterDraft = Override != RuntimeProfileOverrides.end() ? Override->second
                                                                               : *Resolved.States[DebugParameterState];
                    ParameterDrafts[Key] = ParameterDraft;
                    bParameterDraftAvailable = true;
                }
            }
        }

        const SurfaceState::TRegisteredSurfaceResponseProfileData Resolved =
            Registry.ResolveProfile(AssetManager->GetSRProfile(DebugParameterProfile).GetData());
        const bool bProfileSupportsState =
            DebugParameterState < Resolved.States.size() && Resolved.States[DebugParameterState].has_value();
        if (!bProfileSupportsState || !bParameterDraftAvailable)
        {
            ImGui::TextDisabled("This Profile does not define the selected State.");
            return;
        }

        ImGui::Separator();
        ImGui::TextDisabled("Runtime only. Apply updates the current scene; source files stay unchanged.");
        ImGui::TextDisabled("Runtime parameters; accumulation factors also drive the debug preview.");
        bool bChanged = false;
        bChanged |= LabeledDragFloat("Capacity", &ParameterDraft.StateCapacity, 0.01F, 0.001F, 1000.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("기준 면적당 포화 용량입니다. 텍셀별 용량은 실제 월드 면적에 비례해 조정됩니다.");
        bChanged |= LabeledDragFloat("Input factor", &ParameterDraft.InputFactor, 0.01F, 0.0F, 100.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "접촉 입력으로 이 State가 추가되는 양의 배율입니다. 값이 클수록 같은 입력에서 더 많이 쌓입니다.");
        DrawSectionHeader("Transport: flow", 6.0F, 2.0F);
        bChanged |=
            LabeledSliderFloat("Saturation spread", &ParameterDraft.SaturationTransferFactor, 0.0F, 1.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("포화도가 높은 곳에서 낮은 곳으로 State가 이동하는 정도를 조절합니다.");
        bChanged |= LabeledSliderFloat("Gravity flow", &ParameterDraft.GeometryTransferFactor, 0.0F, 1.0F, "%.4f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("월드 높이 차이와 표면 방향으로 투영된 중력에 따른 흐름의 세기를 조절합니다.");
        DrawSectionHeader("Transport: resistance", 6.0F, 2.0F);
        bChanged |= LabeledSliderFloat(
            "Cavity exit resistance", &ParameterDraft.CavityTransportRetentionFactor, 0.0F, 1.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("더 오목하지 않은 이웃 방향으로 이동할 때 수송을 줄입니다. 홈이 차오를수록 "
                                  "저항은 감소하고, 완전히 차면 적용되지 않습니다.");
        DrawSectionHeader("Decay", 6.0F, 2.0F);
        bChanged |= LabeledDragFloat("Decay /s", &ParameterDraft.DecayRate, 0.01F, 0.0F, 100.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("기준 면적당 초당 감소량입니다. 각 텍셀의 실제 월드 면적에 비례해 적용됩니다.");
        bChanged |=
            LabeledSliderFloat("Cavity decay protection", &ParameterDraft.CavityRetentionFactor, 0.0F, 1.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("오목한 텍셀에서 State의 감소를 줄입니다. 홈이 차오를수록 보호 효과가 "
                                  "감소하며 수송 속도에는 직접 영향을 주지 않습니다.");
        DrawSectionHeader("Accumulation", 6.0F, 2.0F);
        bChanged |= LabeledDragFloat("Accumulation", &ParameterDraft.AccumulationFactor, 0.01F, 0.0F, 1000.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("이 State의 누적량 배율입니다. 값이 클수록 같은 조건에서 더 많이 쌓이며 디버그 "
                                  "미리보기에도 반영됩니다.");
        bChanged |= LabeledSliderFloat("Cavity fill", &ParameterDraft.CavityFillFactor, 0.0F, 1.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "오목한 영역을 채우는 누적 효과의 세기를 조절합니다. 0이면 효과가 없고 1이면 최대입니다.");
        bChanged |=
            LabeledDragFloat("Thickness per amount", &ParameterDraft.ThicknessPerAmount, 0.001F, 0.0F, 100.0F, "%.4f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "기준 면적당 누적량을 표면 위 적층 두께(월드 길이)로 환산합니다. 0이면 표면 위 적층과 cavity 초과분의 "
                "두께는 0이지만, 기존 cavity를 채우는 높이에는 영향을 주지 않습니다.");
        if (bChanged)
        {
            bParameterDraftDirty = true;
            DirtyParameterDrafts.insert(Key);
            ParameterDrafts[Key] = ParameterDraft;
        }

        if (bParameterDraftDirty)
        {
            ImGui::TextColored({1.0F, 0.78F, 0.24F, 1.0F}, "Changes not applied");
        }
        else if (RuntimeProfileOverrides.contains(Key))
        {
            ImGui::TextColored({0.35F, 0.85F, 0.55F, 1.0F}, "Runtime override active");
        }
        else
        {
            ImGui::TextDisabled("Using Profile asset values.");
        }

        const bool bApplyOverride = ImGui::Button("Apply Override");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("현재 조정값을 이 실행 중인 씬에 적용합니다. Profile 원본 파일은 변경되지 않습니다.");
        if (bApplyOverride && bParameterDraftDirty)
        {
            bFrameTimeResetRequested = true;
            try
            {
                SurfaceState::TSurfaceResponseProfileData ValidationData;
                ValidationData.States.emplace(Registry.GetStateName(DebugParameterState), ParameterDraft);
                ValidateSurfaceResponseProfileData(ValidationData);
                FrameRenderer->SetDebugProfileParameters(DebugParameterProfile, DebugParameterState, ParameterDraft);
                ResetProfilingAverages();
                RuntimeProfileOverrides[Key] = ParameterDraft;
                DirtyParameterDrafts.erase(Key);
                bParameterDraftDirty = false;
                ParameterDrafts[Key] = ParameterDraft;
                ParameterStatus.clear();
            }
            catch (const std::exception& Exception)
            {
                ParameterStatus = Exception.what();
                TLogger::Warning("TDebugUI", "Could not apply Profile override: " + ParameterStatus);
            }
        }
        ImGui::SameLine();
        const bool bRestoreProfileValues = ImGui::Button("Restore Profile Values");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("선택한 Profile의 원래 값으로 되돌리고 해당 런타임 오버라이드를 해제합니다.");
        if (bRestoreProfileValues)
        {
            bFrameTimeResetRequested = true;
            const SurfaceState::TSurfaceStateParameters& Original = *Resolved.States[DebugParameterState];
            if (RuntimeProfileOverrides.contains(Key))
            {
                FrameRenderer->SetDebugProfileParameters(DebugParameterProfile, DebugParameterState, Original, false);
                ResetProfilingAverages();
                RuntimeProfileOverrides.erase(Key);
            }
            ParameterDraft = Original;
            ParameterDrafts[Key] = Original;
            DirtyParameterDrafts.erase(Key);
            bParameterDraftDirty = false;
            ParameterStatus.clear();
        }
        if (!ParameterStatus.empty())
        {
            ImGui::TextWrapped("%s", ParameterStatus.c_str());
        }
    }

    #pragma endregion

} // namespace MDSS
