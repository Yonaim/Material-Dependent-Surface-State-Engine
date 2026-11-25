/**
 * @file EditorLayoutStyle.h
 * @brief 에디터 공통 제목 및 여백 규칙.
 */
#pragma once

#include <imgui.h>
#include <numbers>

namespace MDSS::EditorLayout
{
    inline constexpr float TitleSize = 18.F;
    inline constexpr float SectionGap = 6.F;

    enum class EPlaybackIcon
    {
        Play,
        Pause,
        Step,
        Restart
    };

    inline float PlaybackSpeedWidth()
    {
        return ImGui::CalcTextSize("0.25x").x + ImGui::GetFrameHeight() + 2.F * ImGui::GetStyle().FramePadding.x;
    }

    inline float PlaybackIconWidth()
    {
        return 3.F * ImGui::GetFrameHeight() + 2.F * ImGui::GetStyle().ItemSpacing.x;
    }

    inline float PlaybackReadoutWidth(bool bHasSpeed)
    {
        const float Spacing = ImGui::GetStyle().ItemSpacing.x;
        const float ClockWidth = ImGui::CalcTextSize("0000.000 s").x;
        return Spacing + ClockWidth + (bHasSpeed ? Spacing + PlaybackSpeedWidth() : 0.F);
    }

    inline bool PlaybackButton(const char *Id, EPlaybackIcon Icon, const char *Tooltip)
    {
        const float  Size = ImGui::GetFrameHeight();
        const bool   bPressed = ImGui::Button(Id, {Size, Size});
        const auto   ItemMin = ImGui::GetItemRectMin();
        const auto   ItemMax = ImGui::GetItemRectMax();
        const ImVec2 Center{(ItemMin.x + ItemMax.x) * 0.5F, (ItemMin.y + ItemMax.y) * 0.5F};
        const float  Radius = Size * 0.23F;
        const auto   Color = ImGui::GetColorU32(ImGuiCol_Text);
        auto        &Draw = *ImGui::GetWindowDrawList();
        switch (Icon)
        {
        case EPlaybackIcon::Play:
            Draw.AddTriangleFilled({Center.x - Radius * 0.6F, Center.y - Radius},
                                   {Center.x + Radius, Center.y},
                                   {Center.x - Radius * 0.6F, Center.y + Radius},
                                   Color);
            break;
        case EPlaybackIcon::Pause:
            Draw.AddRectFilled({Center.x - Radius * 0.78F, Center.y - Radius},
                               {Center.x - Radius * 0.26F, Center.y + Radius},
                               Color);
            Draw.AddRectFilled({Center.x + Radius * 0.26F, Center.y - Radius},
                               {Center.x + Radius * 0.78F, Center.y + Radius},
                               Color);
            break;
        case EPlaybackIcon::Step:
            Draw.AddTriangleFilled({Center.x - Radius, Center.y - Radius},
                                   {Center.x + Radius * 0.4F, Center.y},
                                   {Center.x - Radius, Center.y + Radius},
                                   Color);
            Draw.AddRectFilled({Center.x + Radius * 0.63F, Center.y - Radius},
                               {Center.x + Radius, Center.y + Radius},
                               Color);
            break;
        case EPlaybackIcon::Restart:
            Draw.PathArcTo(Center, Radius * 0.9F, -0.25F * std::numbers::pi_v<float>, std::numbers::pi_v<float>, 18);
            Draw.PathStroke(Color, 1.5F);
            Draw.AddTriangleFilled({Center.x - Radius * 0.9F, Center.y - Radius * 0.65F},
                                   {Center.x - Radius * 1.45F, Center.y + Radius * 0.12F},
                                   {Center.x - Radius * 0.35F, Center.y + Radius * 0.12F},
                                   Color);
            break;
        }
        ImGui::SetItemTooltip("%s", Tooltip);
        return bPressed;
    }

    inline void SectionTitle(const char *Title, float TopPadding = SectionGap, float BottomPadding = SectionGap)
    {
        if (TopPadding > 0.F)
            ImGui::Dummy({0.F, TopPadding});
        ImGui::PushFont(nullptr, TitleSize);
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, {0.5F, 0.5F});
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, {0.F, 0.F});
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.F);
        ImGui::SeparatorText(Title);
        if (ImGui::IsItemHovered() && ImGui::CalcTextSize(Title).x > ImGui::GetItemRectSize().x)
            ImGui::SetTooltip("%s", Title);
        ImGui::PopStyleVar(3);
        ImGui::PopFont();
        if (BottomPadding > 0.F)
            ImGui::Dummy({0.F, BottomPadding});
    }
} // namespace MDSS::EditorLayout
