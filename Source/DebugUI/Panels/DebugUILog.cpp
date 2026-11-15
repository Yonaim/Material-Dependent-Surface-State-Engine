/**
 * @file DebugUILog.cpp
 * @brief Logger 기록의 필터·검색·복사와 로그 패널 표시를 처리한다.
 */

#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
    void FDebugUI::DrawLogWindow()
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize({std::max(Viewport->WorkSize.x * 0.70F, 720.0F), LogState.WindowHeight},
                                 ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Log"))
        {
            const std::uint64_t CurrentRevision = FLogger::GetRevision();

            if (CurrentRevision != LogState.LastSeenRevision)
            {
                LogState.Entries = FLogger::GetEntries();
                LogState.bScrollToBottom = true;
                LogState.LastSeenRevision = CurrentRevision;
            }

            if (ImGui::Button("Clear"))
            {
                FLogger::Clear();
                LogState.bScrollToBottom = true;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(260.0F);
            ImGui::InputTextWithHint("##LogSearch", "Search logs...", LogState.Search.data(), LogState.Search.size());
            ImGui::SameLine();

            if (ImGui::Button("Copy Visible"))
            {
                std::string ClipboardText;

                for (const FLogEntry& Entry : LogState.Entries)
                {
                    const std::size_t Index = static_cast<std::size_t>(Entry.Level);

                    if (Index < LogState.LevelFilters.size() && LogState.LevelFilters[Index] &&
                        ContainsCaseInsensitive(Entry.Formatted, LogState.Search.data()))
                    {
                        ClipboardText += Entry.Formatted;
                        ClipboardText.push_back('\n');
                    }
                }

                ImGui::SetClipboardText(ClipboardText.c_str());
            }

            ImGui::SameLine();

            if (ImGui::Button("All"))
            {
                LogState.LevelFilters.fill(true);
            }

            ImGui::SameLine();

            if (ImGui::Button("None"))
            {
                LogState.LevelFilters.fill(false);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted("Filter:");

            for (const ELogLevel Level : DisplayedLevels)
            {
                ImGui::SameLine();
                const std::size_t Index = static_cast<std::size_t>(Level);
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Level));
                ImGui::Checkbox(FLogger::GetLevelName(Level), &LogState.LevelFilters[Index]);
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            ImGui::BeginChild(
                "LogMessages", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            for (const FLogEntry& Entry : LogState.Entries)
            {
                const std::size_t Index = static_cast<std::size_t>(Entry.Level);

                if (Index >= LogState.LevelFilters.size() || !LogState.LevelFilters[Index])
                {
                    continue;
                }

                if (!ContainsCaseInsensitive(Entry.Formatted, LogState.Search.data()))
                {
                    continue;
                }

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

        ImGui::End();
    }
} // namespace MDSS
