#pragma once
/**
 * @file DebugUI.cpp
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#include "Application/EngineConfig.h"
#include "Application/SceneFileDialog.h"
// ============================================================
// 00. Shared UI Dependencies
// ============================================================
#pragma region 00_Shared_UI_Dependencies

#include "Application/Window.h"
#include "DebugUI/Panels/RenderViewDescriptions.h"
#include "AssetManager/Core/AssetManager.h"
#include "DebugUI/Core/DebugUI.h"
#include "GPU/Vulkan/Resource/GPUResourceMemoryStats.h"
#include "GPU/Vulkan/Swapchain/Swapchain.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "GPU/Vulkan/VulkanQueue.h"
#include "InputSystem/Raycaster.h"
#include "Logger/Logger.h"
#include "Rendering/Core/Renderer.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/SceneLoader.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#pragma endregion

// ============================================================
// 01. UI Widgets and Display Helpers
// ============================================================
#pragma region 01_UI_Widgets_and_Display_Helpers

namespace MDSS
{
    namespace
    {
        ImVec4 GetLogColor(ELogLevel Level)
        {
            switch (Level)
            {
                case ELogLevel::Verbose:
                    return {0.55F, 0.55F, 0.55F, 1.0F}; // 회색
                case ELogLevel::Debug:
                    return {1.00F, 1.00F, 1.00F, 1.0F}; // 흰색
                case ELogLevel::Info:
                    return {0.28F, 0.52F, 0.86F, 1.0F}; // 파랑
                case ELogLevel::Warning:
                    return {1.00F, 0.84F, 0.25F, 1.0F}; // 노랑
                case ELogLevel::Error:
                    return {1.00F, 0.30F, 0.30F, 1.0F}; // 빨강
                case ELogLevel::Count:
                    break;
            }
            return {1.0F, 1.0F, 1.0F, 1.0F};
        }

        constexpr std::array<ELogLevel, 5> DisplayedLevels = {
            ELogLevel::Verbose,
            ELogLevel::Debug,
            ELogLevel::Info,
            ELogLevel::Warning,
            ELogLevel::Error,
        };

        constexpr std::array<std::array<float, 2>, 9> CameraViewPresetPitchYawDegrees = {{
            {{-90.0F, -90.0F}},
            {{-90.0F, 0.0F}},
            {{-90.0F, 90.0F}},
            {{0.0F, -90.0F}},
            {{0.0F, 0.0F}},
            {{0.0F, 90.0F}},
            {{90.0F, -90.0F}},
            {{90.0F, 0.0F}},
            {{90.0F, 90.0F}},
        }};

        constexpr std::array<const char*, 6> RenderViewModeNames = {
            "Lit",
            "Unlit",
            "Wireframe",
            "Vertex Normal (World Space)",
            "Normal Texture (Tangent Space)",
            "Mapped Normal (World Space)",
        };

        constexpr std::array<const char*, 6> SurfaceDebugViewNames = {
            "State Heatmap",
            "Validity",
            "Surface ID",
            "Neighbor Count",
            "UV Seam",
            "Outgoing Flux Scale",
        };

        constexpr std::array<const char*, 4> SolverTransferWeightViewNames = {
            "Combined TransferWeight",
            "DistanceWeight",
            "NormalWeight",
            "ProfileBoundaryWeight",
        };

        // 설명과 도움말은 전역 보조 텍스트 색을 사용한다.
        void TextDescriptionWrapped(const char* Text)
        {
            ImGui::PushTextWrapPos(0.0F);
            ImGui::TextDisabled("%s", Text);
            ImGui::PopTextWrapPos();
        }

        bool BeginTextTreeNode(const char* Label, ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_None)
        {
            return ImGui::TreeNodeEx(Label, Flags | ImGuiTreeNodeFlags_SpanAvailWidth);
        }

        void SetDescriptionTooltip(const char* Text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::SetTooltip("%s", Text);
            ImGui::PopStyleColor();
        }

        void DrawLegendColor(ImVec4 Color, const char* Label, const char* Tooltip = nullptr)
        {
            ImGui::PushID(Label);
            ImGui::ColorButton(
                "##Color", Color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, {12.0F, 12.0F});
            if (Tooltip != nullptr && ImGui::IsItemHovered())
            {
                SetDescriptionTooltip(Tooltip);
            }
            ImGui::SameLine(0.0F, 4.0F);
            ImGui::TextUnformatted(Label);
            if (Tooltip != nullptr && ImGui::IsItemHovered())
            {
                SetDescriptionTooltip(Tooltip);
            }
            ImGui::PopID();
            ImGui::SameLine(0.0F, 10.0F);
        }

        constexpr float LabeledControlColumnWidth = 140.0F;

        void DollyCamera(FCamera& CameraData, float Steps)
        {
            const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
            const float     Distance = glm::length(ViewDirection);
            if (Distance <= 1.0e-6F)
            {
                return;
            }

            constexpr float ZoomSensitivity = 0.12F;
            constexpr float MinimumCameraDistance = 0.05F;
            const float     ClampedSteps = std::clamp(Steps, -10.0F, 10.0F);
            const float     NewDistance =
                std::max(Distance * std::exp(-ClampedSteps * ZoomSensitivity), MinimumCameraDistance);
            const glm::vec3 Forward = ViewDirection / Distance;
            CameraData.SetPosition(CameraData.GetPosition() + Forward * (Distance - NewDistance));
        }

        // 라벨은 같은 시작점에 두고, 조절 위젯은 고정된 열에서 시작해 행을 정렬한다.
        void BeginLabeledControlRow(const char* Label)
        {
            const float RowStartX = ImGui::GetCursorPosX();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Label);
            ImGui::SameLine();
            ImGui::SetCursorPosX(RowStartX + LabeledControlColumnWidth);
        }

        bool SliderFloatWithDoubleClickInput(const char*      Label,
                                             float*           Value,
                                             float            Minimum,
                                             float            Maximum,
                                             const char*      Format,
                                             ImGuiSliderFlags Flags = ImGuiSliderFlags_AlwaysClamp)
        {
            const ImVec2 Position = ImGui::GetCursorScreenPos();
            const ImVec2 Size(ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
            const bool   bDoubleClicked =
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                ImGui::IsMouseHoveringRect(Position, ImVec2(Position.x + Size.x, Position.y + Size.y));
            ImGuiIO&   IO = ImGui::GetIO();
            const bool bWasControlDown = IO.KeyCtrl;
            if (bDoubleClicked)
            {
                // Dear ImGui's slider input mode is normally entered with Ctrl+click.
                // Route a double-click through that built-in path for this widget only.
                IO.KeyCtrl = true;
            }
            const bool bChanged = ImGui::SliderFloat(Label, Value, Minimum, Maximum, Format, Flags);
            IO.KeyCtrl = bWasControlDown;
            return bChanged;
        }

        bool SliderIntWithDoubleClickInput(const char*      Label,
                                           int*             Value,
                                           int              Minimum,
                                           int              Maximum,
                                           const char*      Format = "%d",
                                           ImGuiSliderFlags Flags = ImGuiSliderFlags_AlwaysClamp)
        {
            const ImVec2 Position = ImGui::GetCursorScreenPos();
            const ImVec2 Size(ImGui::CalcItemWidth(), ImGui::GetFrameHeight());
            const bool   bDoubleClicked =
                ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                ImGui::IsMouseHoveringRect(Position, ImVec2(Position.x + Size.x, Position.y + Size.y));
            ImGuiIO&   IO = ImGui::GetIO();
            const bool bWasControlDown = IO.KeyCtrl;
            if (bDoubleClicked)
            {
                IO.KeyCtrl = true;
            }
            const bool bChanged = ImGui::SliderInt(Label, Value, Minimum, Maximum, Format, Flags);
            IO.KeyCtrl = bWasControlDown;
            return bChanged;
        }

        bool LabeledSliderFloat(const char* Label, float* Value, float Minimum, float Maximum, const char* Format)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = SliderFloatWithDoubleClickInput("##Value", Value, Minimum, Maximum, Format);
            ImGui::PopID();
            return bChanged;
        }

        bool LabeledRangeSliderFloat(const char* Label, float* MinimumValue, float* MaximumValue)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const ImVec2 Size{ImGui::CalcItemWidth(), ImGui::GetFrameHeight()};
            ImGui::InvisibleButton("##Range", Size);
            const ImVec2    RectMin = ImGui::GetItemRectMin();
            const ImVec2    RectMax = ImGui::GetItemRectMax();
            const ImGuiID   WidgetID = ImGui::GetItemID();
            ImGuiStorage*   Storage = ImGui::GetStateStorage();
            int             ActiveHandle = Storage->GetInt(WidgetID, 0);
            constexpr float MinimumGap = 0.01F;
            constexpr float HandleRadius = 5.0F;
            constexpr float HorizontalInset = 8.0F;
            const float     TrackMinimum = RectMin.x + HorizontalInset;
            const float     TrackMaximum = std::max(TrackMinimum + 1.0F, RectMax.x - HorizontalInset);
            const float     TrackWidth = TrackMaximum - TrackMinimum;
            const auto      ToPosition = [&](float Value)
            { return TrackMinimum + std::clamp(Value, 0.0F, 1.0F) * TrackWidth; };
            const auto ToValue = [&](float Position)
            { return std::clamp((Position - TrackMinimum) / TrackWidth, 0.0F, 1.0F); };
            bool bChanged = false;
            if (ImGui::IsItemClicked())
            {
                const float MouseX = ImGui::GetIO().MousePos.x;
                ActiveHandle =
                    std::abs(MouseX - ToPosition(*MinimumValue)) <= std::abs(MouseX - ToPosition(*MaximumValue)) ? 0
                                                                                                                 : 1;
                Storage->SetInt(WidgetID, ActiveHandle);
            }
            if (ImGui::IsItemActive())
            {
                const float NewValue = ToValue(ImGui::GetIO().MousePos.x);
                if (ActiveHandle == 0)
                {
                    const float UpdatedValue = std::min(NewValue, *MaximumValue - MinimumGap);
                    bChanged = std::abs(UpdatedValue - *MinimumValue) > 1.0e-6F;
                    *MinimumValue = UpdatedValue;
                }
                else
                {
                    const float UpdatedValue = std::max(NewValue, *MinimumValue + MinimumGap);
                    bChanged = std::abs(UpdatedValue - *MaximumValue) > 1.0e-6F;
                    *MaximumValue = UpdatedValue;
                }
            }

            ImDrawList* DrawList = ImGui::GetWindowDrawList();
            const float TrackY = RectMax.y - 5.0F;
            const ImU32 FrameColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
            const ImU32 TrackColor = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
            const ImU32 RangeColor = ImGui::GetColorU32(ImGuiCol_SliderGrab);
            const ImU32 HandleColor =
                ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
            DrawList->AddRectFilled(RectMin, RectMax, FrameColor, ImGui::GetStyle().FrameRounding);
            char RangeText[32];
            std::snprintf(RangeText, sizeof(RangeText), "%.2f  –  %.2f", *MinimumValue, *MaximumValue);
            DrawList->AddText({RectMin.x + 6.0F, RectMin.y + 1.0F}, ImGui::GetColorU32(ImGuiCol_Text), RangeText);
            DrawList->AddRectFilled({TrackMinimum, TrackY - 2.0F}, {TrackMaximum, TrackY + 2.0F}, TrackColor, 2.0F);
            DrawList->AddRectFilled({ToPosition(*MinimumValue), TrackY - 2.0F},
                                    {ToPosition(*MaximumValue), TrackY + 2.0F},
                                    RangeColor,
                                    2.0F);
            DrawList->AddCircleFilled({ToPosition(*MinimumValue), TrackY}, HandleRadius, HandleColor);
            DrawList->AddCircleFilled({ToPosition(*MaximumValue), TrackY}, HandleRadius, HandleColor);
            ImGui::PopID();
            return bChanged;
        }

        bool LabeledDragFloat(const char*      Label,
                              float*           Value,
                              float            Speed,
                              float            Minimum,
                              float            Maximum,
                              const char*      Format,
                              ImGuiSliderFlags Flags = ImGuiSliderFlags_None)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = ImGui::DragFloat("##Value", Value, Speed, Minimum, Maximum, Format, Flags);
            ImGui::PopID();
            return bChanged;
        }

        bool LabeledDragFloat2(const char* Label, float* Value, float Speed)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = ImGui::DragFloat2("##Value", Value, Speed);
            ImGui::PopID();
            return bChanged;
        }

        bool LabeledDragFloat3(const char* Label, float* Value, float Speed)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = ImGui::DragFloat3("##Value", Value, Speed);
            ImGui::PopID();
            return bChanged;
        }

        bool BeginLabeledCombo(const char*     Label,
                               const char*     Preview,
                               float           Width = 0.0F,
                               ImGuiComboFlags Flags = ImGuiComboFlags_None)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(Width > 0.0F ? Width : -1.0F);
            if (ImGui::BeginCombo("##Value", Preview, Flags))
            {
                return true;
            }
            ImGui::PopID();
            return false;
        }

        void EndLabeledCombo()
        {
            ImGui::EndCombo();
            ImGui::PopID();
        }

        bool ProjectToScreen(glm::vec3 World, const glm::mat4& VP, ImVec2 Size, ImVec2& Screen, float* Depth = nullptr)
        {
            const glm::vec4 Clip = VP * glm::vec4(World, 1.0F);
            if (Clip.w <= 1.0e-5F)
                return false;
            const glm::vec3 NDC = glm::vec3(Clip) / Clip.w;
            if (Depth != nullptr)
                *Depth = NDC.z;
            Screen = {((NDC.x + 1.0F) * 0.5F) * Size.x, ((NDC.y + 1.0F) * 0.5F) * Size.y};
            return NDC.z >= 0.0F && NDC.z <= 1.0F;
        }

        float PointSegmentDistance(ImVec2 P, ImVec2 A, ImVec2 B, float& Along)
        {
            const float DX = B.x - A.x, DY = B.y - A.y;
            const float Denominator = DX * DX + DY * DY;
            if (Denominator < 1.0e-6F)
            {
                Along = 0.0F;
                return std::hypot(P.x - A.x, P.y - A.y);
            }
            Along = std::clamp(((P.x - A.x) * DX + (P.y - A.y) * DY) / Denominator, 0.0F, 1.0F);
            return std::hypot(P.x - A.x - Along * DX, P.y - A.y - Along * DY);
        }

        bool ContainsCaseInsensitive(std::string_view Text, std::string_view Search)
        {
            if (Search.empty())
            {
                return true;
            }
            return std::search(Text.begin(),
                               Text.end(),
                               Search.begin(),
                               Search.end(),
                               [](char Left, char Right)
                               {
                                   return std::tolower(static_cast<unsigned char>(Left)) ==
                                          std::tolower(static_cast<unsigned char>(Right));
                               }) != Text.end();
        }

        struct FPerformanceMemoryEstimate
        {
            double RawFluxCacheBytes = 0.0;
            double RawFluxSavingsBytes = 0.0;
            double DynamicWeightSavingsBytes = 0.0;
            double SparseSolverWorkgroupBytes = 0.0;
            double ActiveChannelMaskBytes = 0.0;
            double PerWorkgroupChannelMaskBytes = 0.0;
            double SparseAccumulationHeightBytes = 0.0;
            double SparseSimulationGeometryBytes = 0.0;
        };

        FPerformanceMemoryEstimate
        EstimatePerformanceMemory(const SurfaceState::FSurfaceGPUResourceManager& GPUResources)
        {
            FPerformanceMemoryEstimate Estimate;
            for (std::size_t Instance = 0; Instance < GPUResources.GetSceneInstanceCount(); ++Instance)
            {
                if (GPUResources.GetInstanceDescriptors(Instance) == nullptr)
                    continue;
                const double Texels = static_cast<double>(GPUResources.GetInstanceTexelCount(Instance));
                const double Channels = static_cast<double>(GPUResources.GetInstanceChannelCount(Instance));
                const double Workgroups = std::ceil(Texels / 64.0);
                // The RawOutgoing allocation always reserves eight float values per state scalar for the cache.
                Estimate.RawFluxCacheBytes += Texels * Channels * 8.0 * sizeof(float);
                // Compact FP16 storage would halve the eight directional flux values.
                Estimate.RawFluxSavingsBytes += Texels * Channels * 8.0 * sizeof(float) * 0.5;
                // Eight float TransferWeights become four packed uint words per texel.
                Estimate.DynamicWeightSavingsBytes += Texels * 16.0;
                // These are disjoint regions inside the always-reserved AccumulationHeight metadata buffer.
                Estimate.SparseSolverWorkgroupBytes += (6.0 + 4.0 * Workgroups) * sizeof(std::uint32_t);
                Estimate.ActiveChannelMaskBytes += 2.0 * sizeof(std::uint32_t);
                Estimate.PerWorkgroupChannelMaskBytes += 2.0 * Workgroups * sizeof(std::uint32_t);
                Estimate.SparseAccumulationHeightBytes += (3.0 + 2.0 * Workgroups) * sizeof(std::uint32_t);
                Estimate.SparseSimulationGeometryBytes += (3.0 + 2.0 * Workgroups) * sizeof(std::uint32_t);
            }
            return Estimate;
        }

        std::string FormatMemoryBytes(double Bytes)
        {
            constexpr double KiB = 1024.0;
            constexpr double MiB = KiB * KiB;
            char             Buffer[64];
            if (Bytes >= MiB)
                std::snprintf(Buffer, sizeof(Buffer), "%.2f MiB", Bytes / MiB);
            else if (Bytes >= KiB)
                std::snprintf(Buffer, sizeof(Buffer), "%.1f KiB", Bytes / KiB);
            else
                std::snprintf(Buffer, sizeof(Buffer), "%.0f B", Bytes);
            return Buffer;
        }
    } // 내부 네임스페이스

}

#pragma endregion
