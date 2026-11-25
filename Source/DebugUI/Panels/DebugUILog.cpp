/**
 * @file DebugUILog.cpp
 * @brief Logger 기록의 필터·검색·복사와 로그 패널 표시를 처리한다.
 */

#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
    // ============================================================
    // 00. Log Panel
    // ============================================================
    #pragma region 00_Log_Panel
        void FDebugUI::DrawLogContents()
        {
            const std::uint64_t CurrentRevision = FLogger::GetRevision();

            if (CurrentRevision != LogState.LastSeenRevision)
            {
                LogState.Entries = FLogger::GetEntries();
                LogState.bScrollToBottom = true;
                LogState.LastSeenRevision = CurrentRevision;
            }

            const auto IsVisible = [&](const FLogEntry &Entry)
            {
                const auto Index = static_cast<std::size_t>(Entry.Level);
                return Index < LogState.LevelFilters.size() && LogState.LevelFilters[Index] &&
                       ContainsCaseInsensitive(Entry.Formatted, LogState.Search.data());
            };
            const auto &Style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {5.F, Style.FramePadding.y});
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4.F, Style.ItemSpacing.y});
            const auto ButtonWidth = [&](const char *Label)
            { return ImGui::CalcTextSize(Label).x + 2.F * Style.FramePadding.x; };
            const auto LevelLabel = [](ELogLevel Level)
            { return Level == ELogLevel::Warning ? "WARN" : FLogger::GetLevelName(Level); };
            const auto LevelWidth = [&](ELogLevel Level)
            { return ImGui::GetFrameHeight() + Style.ItemInnerSpacing.x + ImGui::CalcTextSize(LevelLabel(Level)).x; };
            const float Available = ImGui::GetContentRegionAvail().x;
            const float RowEnd = ImGui::GetCursorScreenPos().x + Available;
            // 패널이 좁아져도 개별 필터와 동작을 다음 줄에서 바로 누를 수 있게 한다.
            const auto PlaceControl = [&](float Width)
            {
                if (ImGui::GetItemRectMax().x + Style.ItemSpacing.x + Width <= RowEnd)
                    ImGui::SameLine();
            };

            float ControlsWidth = ButtonWidth("All") + ButtonWidth("None") + ButtonWidth("Copy") + ButtonWidth("Clear") +
                                  (static_cast<float>(DisplayedLevels.size()) + 4.F) * Style.ItemSpacing.x;
            for (const ELogLevel Level : DisplayedLevels)
                ControlsWidth += LevelWidth(Level);
            ImGui::SetNextItemWidth(std::min(Available, std::max(88.F, Available - ControlsWidth)));
            ImGui::InputTextWithHint("##LogSearch", "Search logs...", LogState.Search.data(), LogState.Search.size());

            PlaceControl(ButtonWidth("All"));
            if (ImGui::Button("All"))
                LogState.LevelFilters.fill(true);
            ImGui::SetItemTooltip("Show all log levels");
            PlaceControl(ButtonWidth("None"));
            if (ImGui::Button("None"))
                LogState.LevelFilters.fill(false);
            ImGui::SetItemTooltip("Hide all log levels");
            for (const ELogLevel Level : DisplayedLevels)
            {
                PlaceControl(LevelWidth(Level));
                const auto Index = static_cast<std::size_t>(Level);
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Level));
                ImGui::Checkbox(LevelLabel(Level), &LogState.LevelFilters[Index]);
                ImGui::PopStyleColor();
            }

            PlaceControl(ButtonWidth("Copy"));
            if (ImGui::Button("Copy"))
            {
                std::string ClipboardText;
                for (const FLogEntry &Entry : LogState.Entries)
                    if (IsVisible(Entry))
                    {
                        ClipboardText += Entry.Formatted;
                        ClipboardText.push_back('\n');
                    }
                ImGui::SetClipboardText(ClipboardText.c_str());
            }
            ImGui::SetItemTooltip("Copy visible log entries");
            PlaceControl(ButtonWidth("Clear"));
            if (ImGui::Button("Clear"))
            {
                FLogger::Clear();
                LogState.bScrollToBottom = true;
            }

            ImGui::PopStyleVar(2);
            ImGui::Separator();

            ImGui::BeginChild("LogMessages",
                              ImVec2(0.0F, 0.0F),
                              ImGuiChildFlags_None,
                              ImGuiWindowFlags_HorizontalScrollbar);
            for (const FLogEntry &Entry : LogState.Entries)
            {
                if (!IsVisible(Entry))
                    continue;

                ImGui::PushID(static_cast<int>(Entry.Sequence));
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Entry.Level));
                ImGui::Selectable(Entry.Formatted.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
                ImGui::PopStyleColor();

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    ImGui::SetClipboardText(Entry.Formatted.c_str());
                }

                if (ImGui::BeginPopupContextItem("LogLineContext"))
                {
                    if (ImGui::MenuItem("Copy Line"))
                    {
                        ImGui::SetClipboardText(Entry.Formatted.c_str());
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }

            if (LogState.bScrollToBottom)
            {
                ImGui::SetScrollHereY(1.0F);
                LogState.bScrollToBottom = false;
            }

            ImGui::EndChild();
        }
    #pragma endregion
} // namespace MDSS
