/**
 * @file DebugUIContactProfile.cpp
 * @brief 메시 연구 실행에 사용하는 접촉 입력을 편집한다.
 */

#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Contact Input
// ============================================================
#pragma region 00_Contact_Input
    // 선택한 State와 영역·강도로 표면 접촉 입력을 생성한다.

    void FDebugUI::DrawContactInputTab()
    {
        ImGui::Checkbox("Inject mode", &bInjectMode);

        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("십자선이 가리키는 지점에 Space 키로 주입합니다.");
        }

        ImGui::TextDisabled("Crosshair hit → triangle → UV → simulation texel.");

        const SurfaceState::FSurfaceStateRegistry *InjectRegistry =
            AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
        const std::size_t InjectStateCount = InjectRegistry != nullptr ? InjectRegistry->GetStateCount() : 0;

        if (InjectStateCount == 0)
        {
            ImGui::TextDisabled("No Surface States are registered.");
            Simulation.InjectState = SurfaceState::InvalidStateId;
        }
        else
        {
            if (Simulation.InjectState >= InjectStateCount)
            {
                Simulation.InjectState = 0;
            }

            const std::string &SelectedName = InjectRegistry->GetStateName(Simulation.InjectState);
            const bool         bStateOpen = BeginLabeledCombo("State", SelectedName.c_str());

            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("주입할 Surface State 채널입니다.");
            }

            if (bStateOpen)
            {
                for (std::size_t Index = 0; Index < InjectStateCount; ++Index)
                {
                    const SurfaceState::FStateId State = static_cast<SurfaceState::FStateId>(Index);
                    const std::string           &Name = InjectRegistry->GetStateName(State);
                    const bool                   bSelected = State == Simulation.InjectState;

                    if (ImGui::Selectable(Name.c_str(), bSelected))
                    {
                        Simulation.InjectState = State;
                    }

                    if (bSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                EndLabeledCombo();
            }
        }

        LabeledSliderFloat("Radius (world)", &Simulation.InjectRadius, 0.01F, 2.0F, "%.2f");

        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("주입 접촉의 월드 공간 반경입니다.");
        }

        LabeledSliderFloat("Strength", &Simulation.InjectStrength, 0.0F, 2.0F, "%.2f");

        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("기준 면적당 입력량입니다. 총량은 영향을 받는 월드 면적에 따라 결정됩니다.");
        }

        LabeledSliderFloat("Falloff", &Simulation.InjectFalloff, 0.0F, 4.0F, "%.2f");

        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("반경 가장자리로 갈수록 주입 강도가 감소하는 정도를 조절합니다.");
        }

        const char *OperationPreview =
            Simulation.InjectOperation == SurfaceState::ESurfaceContactOperation::Remove ? "Remove" : "Add";
        if (BeginLabeledCombo("Operation", OperationPreview))
        {
            if (ImGui::Selectable("Add", Simulation.InjectOperation == SurfaceState::ESurfaceContactOperation::Add))
                Simulation.InjectOperation = SurfaceState::ESurfaceContactOperation::Add;
            if (ImGui::Selectable("Remove",
                                  Simulation.InjectOperation == SurfaceState::ESurfaceContactOperation::Remove))
                Simulation.InjectOperation = SurfaceState::ESurfaceContactOperation::Remove;
            EndLabeledCombo();
        }

        const char *ShapePreview = "Circle";

        switch (Simulation.InjectShape)
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
            if (ImGui::Selectable("Circle", Simulation.InjectShape == SurfaceState::ESurfaceContactShape::Circle))
                Simulation.InjectShape = SurfaceState::ESurfaceContactShape::Circle;
            if (ImGui::Selectable("Ellipse", Simulation.InjectShape == SurfaceState::ESurfaceContactShape::Ellipse))
                Simulation.InjectShape = SurfaceState::ESurfaceContactShape::Ellipse;
            if (ImGui::Selectable("Rectangle", Simulation.InjectShape == SurfaceState::ESurfaceContactShape::Rectangle))
                Simulation.InjectShape = SurfaceState::ESurfaceContactShape::Rectangle;
            if (ImGui::Selectable("Capsule", Simulation.InjectShape == SurfaceState::ESurfaceContactShape::Capsule))
                Simulation.InjectShape = SurfaceState::ESurfaceContactShape::Capsule;
            EndLabeledCombo();
        }

        if (Simulation.InjectShape != SurfaceState::ESurfaceContactShape::Circle)
        {
            const char *SecondaryLabel = Simulation.InjectShape == SurfaceState::ESurfaceContactShape::Capsule
                                             ? "Capsule half length"
                                             : "Second half extent";
            LabeledSliderFloat(SecondaryLabel, &Simulation.InjectSecondaryRadius, 0.01F, 2.0F, "%.2f");
            LabeledSliderFloat("Rotation (degrees)", &Simulation.InjectRotationDegrees, -180.0F, 180.0F, "%.0f");
        }

        BeginLabeledControlRow("Texel search");
        ImGui::SetNextItemWidth(-1.0F);
        SliderIntWithDoubleClickInput("##InjectTexelSearchRadius",
                                      &Simulation.InjectTexelSearchRadius,
                                      0,
                                      16,
                                      "%d texels");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("같은 삼각형 안에서 UV 대체 검색을 수행할 축별 범위입니다. 기본값은 2이며, 0이면 "
                                  "대체 검색을 사용하지 않습니다.");
        }
    }

#pragma endregion
} // namespace MDSS
