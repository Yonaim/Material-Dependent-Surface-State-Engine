/**
 * @file DebugUI.cpp
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#include "DebugUI/DebugUI.h"

#include "Application/EngineConfig.h"
#include "Application/SceneFileDialog.h"
#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "GPU/Vulkan/Swapchain/Swapchain.h"
#include "GPU/Vulkan/VulkanContext.h"
#include "GPU/Vulkan/VulkanQueue.h"
#include "InputSystem/Raycaster.h"
#include "Logger/Logger.h"
#include "Rendering/Renderer.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/SceneLoader.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Preprocessing/SurfaceDataManager.h"

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

namespace MDSS
{
    namespace
    {
        ImVec4 GetLogColor(TLogLevel Level)
        {
            switch (Level)
            {
                case TLogLevel::Verbose:
                    return {0.55F, 0.55F, 0.55F, 1.0F}; // 회색
                case TLogLevel::Debug:
                    return {1.00F, 1.00F, 1.00F, 1.0F}; // 흰색
                case TLogLevel::Info:
                    return {0.28F, 0.52F, 0.86F, 1.0F}; // 파랑
                case TLogLevel::Warning:
                    return {1.00F, 0.84F, 0.25F, 1.0F}; // 노랑
                case TLogLevel::Error:
                    return {1.00F, 0.30F, 0.30F, 1.0F}; // 빨강
                case TLogLevel::Count:
                    break;
            }
            return {1.0F, 1.0F, 1.0F, 1.0F};
        }

        constexpr std::array<TLogLevel, 5> DisplayedLevels = {
            TLogLevel::Verbose,
            TLogLevel::Debug,
            TLogLevel::Info,
            TLogLevel::Warning,
            TLogLevel::Error,
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

        void DollyCamera(TCamera& CameraData, float Steps)
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
    } // 내부 네임스페이스

    TDebugUI::TDebugUI(const GPU::TVulkanContext&         Context,
                       const TWindow&                     TWindow,
                       Rendering::TRenderer&              Renderer,
                       Asset::TAssetManager&              Assets,
                       SurfaceState::TSurfaceDataManager& SurfaceData)
        : Device(Context.GetDevice()), NativeWindow(TWindow.GetNativeHandle()), FrameRenderer(&Renderer),
          AssetManager(&Assets), SurfaceDataManager(&SurfaceData)
    {
        ResetProfilingAverages();
        bProfiledRawFluxCacheEnabled = FrameRenderer->IsRawFluxCacheEnabled();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        std::filesystem::create_directories(GetEngineConfigDirectory());
        EditorLayoutPath = (GetEngineConfigDirectory() / "EditorLayout.ini").string();
        IO.IniFilename = EditorLayoutPath.c_str();

#if defined(_WIN32)
        constexpr const char* FontCandidates[] = {
            "C:/Windows/Fonts/malgun.ttf", "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"};
#elif defined(__APPLE__)
        constexpr const char* FontCandidates[] = {"/System/Library/Fonts/AppleSDGothicNeo.ttc",
                                                  "/System/Library/Fonts/Supplemental/Arial.ttf",
                                                  "/System/Library/Fonts/Supplemental/Verdana.ttf"};
#else
        constexpr const char* FontCandidates[] = {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                                                  "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
                                                  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                                  "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"};
#endif
        for (const char* FontPath : FontCandidates)
        {
            if (std::filesystem::exists(FontPath))
            {
                if (ImFont* Font =
                        IO.Fonts->AddFontFromFileTTF(FontPath, 14.0F, nullptr, IO.Fonts->GetGlyphRangesKorean()))
                {
                    IO.FontDefault = Font;
                    SectionHeaderFont =
                        IO.Fonts->AddFontFromFileTTF(FontPath, 17.0F, nullptr, IO.Fonts->GetGlyphRangesKorean());
                    break;
                }
            }
        }
        if (IO.FontDefault == nullptr)
        {
            ImFontConfig DefaultFontConfig{};
            DefaultFontConfig.SizePixels = 14.0F;
            IO.FontDefault = IO.Fonts->AddFontDefault(&DefaultFontConfig);
        }
        if (SectionHeaderFont == nullptr)
        {
            ImFontConfig HeaderFontConfig{};
            HeaderFontConfig.SizePixels = 17.0F;
            SectionHeaderFont = IO.Fonts->AddFontDefault(&HeaderFontConfig);
        }

        ImGui::StyleColorsDark();
        ImGuiStyle& Style = ImGui::GetStyle();
        Style.WindowPadding = {12.0F, 10.0F};
        Style.FramePadding = {8.0F, 5.0F};
        Style.ItemSpacing = {8.0F, 6.0F};
        Style.WindowRounding = 3.0F;
        Style.FrameRounding = 3.0F;
        Style.PopupRounding = 3.0F;
        Style.GrabRounding = 3.0F;
        Style.TabRounding = 3.0F;
        Style.Colors[ImGuiCol_Text] = {0.91F, 0.93F, 0.97F, 1.0F};
        Style.Colors[ImGuiCol_TextDisabled] = {0.46F, 0.50F, 0.57F, 1.0F};
        Style.Colors[ImGuiCol_CheckMark] = {0.35F, 0.78F, 1.0F, 1.0F};
        // 창 배경은 패널과 내부 영역에 두 단계의 더 어두운 회색을 쓴다.
        Style.Colors[ImGuiCol_WindowBg] = {0.075F, 0.082F, 0.10F, 0.99F};
        Style.Colors[ImGuiCol_ChildBg] = {0.058F, 0.065F, 0.082F, 0.94F};
        Style.Colors[ImGuiCol_PopupBg] = {0.075F, 0.082F, 0.10F, 0.99F};
        Style.Colors[ImGuiCol_MenuBarBg] = {0.058F, 0.065F, 0.082F, 1.0F};
        Style.Colors[ImGuiCol_DockingEmptyBg] = {0.065F, 0.072F, 0.09F, 1.0F};
        Style.Colors[ImGuiCol_Border] = {0.20F, 0.24F, 0.31F, 0.75F};
        Style.Colors[ImGuiCol_FrameBg] = {0.14F, 0.17F, 0.22F, 1.0F};
        Style.Colors[ImGuiCol_FrameBgHovered] = {0.18F, 0.29F, 0.44F, 1.0F};
        Style.Colors[ImGuiCol_FrameBgActive] = {0.12F, 0.34F, 0.60F, 1.0F};
        Style.Colors[ImGuiCol_TitleBg] = {0.048F, 0.053F, 0.068F, 1.0F};
        Style.Colors[ImGuiCol_TitleBgActive] = {0.060F, 0.086F, 0.13F, 1.0F};
        Style.Colors[ImGuiCol_SliderGrab] = {0.10F, 0.36F, 0.70F, 1.0F};
        Style.Colors[ImGuiCol_SliderGrabActive] = {0.12F, 0.48F, 0.88F, 1.0F};
        Style.Colors[ImGuiCol_Button] = {0.12F, 0.27F, 0.47F, 1.0F};
        Style.Colors[ImGuiCol_ButtonHovered] = {0.15F, 0.36F, 0.61F, 1.0F};
        Style.Colors[ImGuiCol_ButtonActive] = {0.09F, 0.31F, 0.59F, 1.0F};
        Style.Colors[ImGuiCol_Header] = {0.12F, 0.27F, 0.47F, 0.82F};
        Style.Colors[ImGuiCol_HeaderHovered] = {0.15F, 0.36F, 0.61F, 0.92F};
        Style.Colors[ImGuiCol_HeaderActive] = {0.10F, 0.32F, 0.58F, 1.0F};
        Style.Colors[ImGuiCol_Tab] = {0.062F, 0.105F, 0.17F, 1.0F};
        Style.Colors[ImGuiCol_TabHovered] = {0.13F, 0.35F, 0.62F, 1.0F};
        Style.Colors[ImGuiCol_TabSelected] = {0.11F, 0.29F, 0.51F, 1.0F};
        Style.Colors[ImGuiCol_TabSelectedOverline] = {0.08F, 0.34F, 0.70F, 1.0F};

        if (!ImGui_ImplGlfw_InitForVulkan(TWindow.GetNativeHandle(), true))
        {
            ImGui::DestroyContext();
            throw std::runtime_error("Failed to initialize Dear ImGui GLFW backend.");
        }

        const std::uint32_t ImageCount = static_cast<std::uint32_t>(Renderer.GetSwapchain().GetImages().size());
        const GPU::TSwapchainSupportDetails Support =
            GPU::TSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
        const std::uint32_t MinImageCount = std::max(2U, Support.Capabilities.minImageCount);

        ImGui_ImplVulkan_InitInfo InitInfo{};
        InitInfo.ApiVersion = VK_API_VERSION_1_2;
        InitInfo.Instance = Context.GetInstance();
        InitInfo.PhysicalDevice = Context.GetPhysicalDevice();
        InitInfo.Device = Context.GetDevice();
        InitInfo.QueueFamily = Context.GetQueues().GetFamilyIndices().GraphicsFamily.value();
        InitInfo.Queue = Context.GetQueues().GetGraphics();
        InitInfo.DescriptorPool = VK_NULL_HANDLE;
        InitInfo.DescriptorPoolSize = 64;
        InitInfo.MinImageCount = MinImageCount;
        InitInfo.ImageCount = ImageCount;
        InitInfo.PipelineCache = VK_NULL_HANDLE;
        InitInfo.PipelineInfoMain.RenderPass = Renderer.GetRenderPassHandle();
        InitInfo.PipelineInfoMain.Subpass = 0;
        InitInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        InitInfo.UseDynamicRendering = false;
        InitInfo.Allocator = nullptr;
        InitInfo.CheckVkResultFn = nullptr;
        InitInfo.MinAllocationSize = 1024 * 1024;

        if (!ImGui_ImplVulkan_Init(&InitInfo))
        {
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            throw std::runtime_error("Failed to initialize Dear ImGui Vulkan backend.");
        }

        ResetSurfaceStateSettings();
        TLogger::Info("TDebugUI", "Dear ImGui initialized with GLFW/Vulkan backends.");
        TLogger::Debug("TDebugUI",
                       "Camera/render controls, injection controls, normal debug views, and log filtering are active.");
        CachedLogEntries = TLogger::GetEntries();
        LastSeenLogRevision = TLogger::GetRevision();
    }

    TDebugUI::~TDebugUI()
    {
        if (NativeWindow != nullptr && bRotatingCamera)
        {
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        if (Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Device);
        }

        TLogger::Verbose("TDebugUI", "Shutting down Dear ImGui backends.");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void TDebugUI::BeginFrame(TScene& SceneData)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        bMouseOverViewportOverlay = false;

        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        ImGui::GetIO().FontGlobalScale = DisplaySize.x >= 1280.0F && DisplaySize.y >= 800.0F ? 1.15F : 1.0F;
        SetupDockspace();

        // Render the viewport windows before the other editor windows.  The
        // viewport panels are transparent overlays, so creating them last
        // would also make them the topmost ImGui hit-test target when another
        // window is visually above the viewport.
        DrawRenderOptionsWindow(SceneData);
        DrawViewportPanels(SceneData);
        DrawBrickCubeTexelMeshNotice(SceneData);
        DrawSceneWindow(SceneData);
        DrawAnimationWindow(SceneData);
        DrawCameraWindow(SceneData);
        DrawSelectedObjectWindow(SceneData);
        DrawRenderSettingsWindow(SceneData);
        DrawViewportStatsOverlay();
        DrawTotalHeightCacheOverlay();
        if (bMouseOverViewportOverlay)
            HoveredViewportId = 0;
        DrawSimulationDebugWindow(SceneData);
        DrawLogWindow();
        ProcessCameraInput(SceneData);
        ProcessSelectionAndGizmo(SceneData);

        if (bInjectMode && GetViewportRectNormalized(GetActiveViewportIndex()).z > 0.0F)
        {
            ImGuiViewport*  Viewport = ImGui::GetMainViewport();
            const ImVec2    DisplaySize = ImGui::GetIO().DisplaySize;
            const glm::vec4 Rect = GetViewportRectNormalized(GetActiveViewportIndex());
            const ImVec2    Center{Viewport->Pos.x + (Rect.x + Rect.z * 0.5F) * DisplaySize.x,
                                Viewport->Pos.y + (Rect.y + Rect.w * 0.5F) * DisplaySize.y};
            constexpr float CrosshairHalfSize = 9.0F;
            constexpr ImU32 CrosshairColor = IM_COL32(255, 255, 255, 230);
            ImDrawList*     DrawList = ImGui::GetForegroundDrawList();
            DrawList->AddLine({Center.x - CrosshairHalfSize, Center.y},
                              {Center.x + CrosshairHalfSize, Center.y},
                              CrosshairColor,
                              2.0F);
            DrawList->AddLine({Center.x, Center.y - CrosshairHalfSize},
                              {Center.x, Center.y + CrosshairHalfSize},
                              CrosshairColor,
                              2.0F);
        }

        ImGui::Render();
    }

    void TDebugUI::DrawSectionHeader(const char* Title, float TopPadding, float BottomPadding) const
    {
        ImGui::Dummy({0.0F, TopPadding});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.91F, 0.93F, 0.97F, 1.0F});
        if (SectionHeaderFont != nullptr)
        {
            ImGui::PushFont(SectionHeaderFont);
        }
        ImGui::TextUnformatted(Title);
        if (SectionHeaderFont != nullptr)
        {
            ImGui::PopFont();
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Dummy({0.0F, BottomPadding});
    }

    void TDebugUI::Render(VkCommandBuffer CommandBuffer) const
    {
        ImDrawData* DrawData = ImGui::GetDrawData();
        if (DrawData != nullptr && DrawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(DrawData, CommandBuffer);
        }
    }

    void TDebugUI::OnSwapchainRecreated(const GPU::TVulkanContext& Context, const Rendering::TRenderer& Renderer)
    {
        const GPU::TSwapchainSupportDetails Support =
            GPU::TSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
        const std::uint32_t MinImageCount = std::max(2U, Support.Capabilities.minImageCount);
        ImGui_ImplVulkan_SetMinImageCount(MinImageCount);

        TLogger::Debug("TDebugUI",
                       "ImGui Vulkan backend updated after swapchain recreation (images=" +
                           std::to_string(Renderer.GetSwapchain().GetImages().size()) + ").");
    }

    bool TDebugUI::IsInjectModeEnabled() const noexcept
    {
        return bInjectMode;
    }

    SurfaceState::TStateId TDebugUI::GetInjectState() const noexcept
    {
        return InjectState;
    }

    float TDebugUI::GetInjectStrength() const noexcept
    {
        return InjectStrength;
    }

    float TDebugUI::GetInjectRadius() const noexcept
    {
        return InjectRadius;
    }

    float TDebugUI::GetInjectFalloff() const noexcept
    {
        return InjectFalloff;
    }

    std::uint32_t TDebugUI::GetInjectTexelSearchRadius() const noexcept
    {
        return static_cast<std::uint32_t>(InjectTexelSearchRadius);
    }

    float TDebugUI::GetAnimationTimeScale() const noexcept
    {
        return AnimationTimeScale;
    }

    float TDebugUI::GetSimulationTimeScale() const noexcept
    {
        return SimulationTimeScale;
    }

    bool TDebugUI::IsSimulationPaused() const noexcept
    {
        return bSimulationPaused;
    }

    bool TDebugUI::ConsumeSolverStepRequest() noexcept
    {
        const bool bRequested = bSolverStepRequested;
        bSolverStepRequested = false;
        return bRequested;
    }

    bool TDebugUI::ConsumeSolverResetRequest() noexcept
    {
        const bool bRequested = bSolverResetRequested;
        bSolverResetRequested = false;
        return bRequested;
    }

    bool TDebugUI::ConsumeFrameTimeResetRequest() noexcept
    {
        const bool bRequested = bFrameTimeResetRequested;
        bFrameTimeResetRequested = false;
        return bRequested;
    }

    std::size_t TDebugUI::GetViewportCount() const noexcept
    {
        return ViewportPanels.size();
    }

    glm::vec4 TDebugUI::GetViewportRectNormalized(std::size_t Index) const noexcept
    {
        return Index < ViewportPanels.size() ? ViewportPanels[Index].RectNormalized : glm::vec4(0.0F);
    }

    const TCamera& TDebugUI::GetViewportCamera(const TScene& SceneData, std::size_t Index) const noexcept
    {
        return Index == 0 || Index >= ViewportPanels.size() ? SceneData.GetMainCamera() : ViewportPanels[Index].Camera;
    }

    TCamera& TDebugUI::GetViewportCamera(TScene& SceneData, std::size_t Index) noexcept
    {
        return Index == 0 || Index >= ViewportPanels.size() ? SceneData.GetMainCamera() : ViewportPanels[Index].Camera;
    }

    std::size_t TDebugUI::GetActiveViewportIndex() const noexcept
    {
        for (std::size_t Index = 0; Index < ViewportPanels.size(); ++Index)
            if (ViewportPanels[Index].Id == ActiveViewportId)
                return Index;
        return 0;
    }

    void TDebugUI::ApplyCameraViewPreset(TScene& SceneData, std::size_t Index)
    {
        if (!SelectedObject || *SelectedObject >= SceneData.GetStaticMeshInstances().size() ||
            Index >= CameraViewPresetPitchYawDegrees.size())
        {
            return;
        }

        TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
        glm::vec3   Target = Transform.Position;
        if (AssetManager != nullptr)
        {
            const Asset::TMeshAsset& Mesh =
                AssetManager->GetMesh(SceneData.GetStaticMeshInstances()[*SelectedObject].GetMesh());
            const auto& Vertices = Mesh.GetVertices();
            if (!Vertices.empty())
            {
                glm::vec3 Minimum(std::numeric_limits<float>::max());
                glm::vec3 Maximum(std::numeric_limits<float>::lowest());
                for (const Asset::TVertex& Vertex : Vertices)
                {
                    Minimum = glm::min(Minimum, Vertex.Position);
                    Maximum = glm::max(Maximum, Vertex.Position);
                }
                const glm::vec3 LocalBoundsCenter = (Minimum + Maximum) * 0.5F;
                Target = glm::vec3(Transform.GetMatrix() * glm::vec4(LocalBoundsCenter, 1.0F));
            }
        }

        TCamera&        Camera = GetViewportCamera(SceneData, GetActiveViewportIndex());
        const glm::vec3 ViewOffset = Camera.GetPosition() - Camera.GetTarget();
        const float     OffsetLength = glm::length(ViewOffset);
        const float     Distance = OffsetLength > 1.0e-6F ? OffsetLength : 3.0F;
        const float     Pitch = glm::radians(CameraViewPresetPitchYawDegrees[Index][0]);
        const float     Yaw = glm::radians(CameraViewPresetPitchYawDegrees[Index][1]);
        const glm::vec3 Forward{std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch)};
        Camera.SetPosition(Target - Forward * Distance);
        Camera.SetTarget(Target);
    }

    const TCamera& TDebugUI::GetActiveViewportCamera(const TScene& SceneData) const noexcept
    {
        return GetViewportCamera(SceneData, GetActiveViewportIndex());
    }

    std::optional<std::size_t> TDebugUI::GetSelectedObject() const noexcept
    {
        return SelectedObject;
    }

    int TDebugUI::GetHoveredGizmoAxis() const noexcept
    {
        return HoveredGizmoAxis;
    }

    bool TDebugUI::IsRotationGizmoMode() const noexcept
    {
        return bRotationGizmoMode;
    }

    SurfaceState::TStateId TDebugUI::GetDebugState() const noexcept
    {
        return DebugState;
    }

    bool TDebugUI::ShouldSuppressDebugHotkey() const noexcept
    {
        const ImGuiIO& IO = ImGui::GetIO();
        return bMouseOverViewportOverlay || IO.WantTextInput ||
               (HoveredViewportId == 0 && (IO.WantCaptureKeyboard || ImGui::IsAnyItemActive() ||
                                           ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)));
    }

    void TDebugUI::SetupDockspace()
    {
        ImGuiViewport*               Viewport = ImGui::GetMainViewport();
        constexpr ImGuiDockNodeFlags DockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        constexpr ImGuiID            StableDockspaceID = 0x4D445356U;
        DockspaceID = ImGui::DockSpaceOverViewport(StableDockspaceID, Viewport, DockFlags);
        ImGuiDockNode* DockspaceNode = ImGui::DockBuilderGetNode(DockspaceID);

        if (bDockLayoutInitialized)
        {
            return;
        }
        if (DockspaceNode == nullptr)
        {
            return;
        }
        if (DockspaceNode->IsSplitNode())
        {
            bDockLayoutInitialized = true;
            return;
        }

        ImGui::DockBuilderRemoveNode(DockspaceID);
        ImGui::DockBuilderAddNode(DockspaceID, ImGuiDockNodeFlags_DockSpace | DockFlags);
        ImGui::DockBuilderSetNodeSize(DockspaceID, Viewport->WorkSize);

        ImGuiID LogID = 0;
        ImGuiID WorkspaceID = 0;
        ImGui::DockBuilderSplitNode(DockspaceID, ImGuiDir_Down, 0.36F, &LogID, &WorkspaceID);

        ImGuiID LeftID = 0;
        ImGuiID MainID = 0;
        ImGui::DockBuilderSplitNode(WorkspaceID, ImGuiDir_Left, 0.23F, &LeftID, &MainID);
        ImGuiID RightID = 0;
        ImGuiID CenterID = 0;
        ImGui::DockBuilderSplitNode(MainID, ImGuiDir_Right, 0.30F, &RightID, &CenterID);

        ImGuiID SceneID = 0;
        ImGuiID LeftUpperID = 0;
        ImGui::DockBuilderSplitNode(LeftID, ImGuiDir_Up, 0.20F, &SceneID, &LeftUpperID);
        ImGuiID CameraID = 0;
        ImGuiID LeftLowerID = 0;
        ImGui::DockBuilderSplitNode(LeftUpperID, ImGuiDir_Up, 0.43F, &CameraID, &LeftLowerID);
        ImGuiID TransformID = 0;
        ImGuiID RenderSettingsID = 0;
        ImGui::DockBuilderSplitNode(LeftLowerID, ImGuiDir_Up, 0.62F, &TransformID, &RenderSettingsID);

        ImGui::DockBuilderDockWindow("Scene", SceneID);
        ImGui::DockBuilderDockWindow("Animation", SceneID);
        ImGui::DockBuilderDockWindow("Camera", CameraID);
        ImGui::DockBuilderDockWindow("Selected Object##SceneTools", TransformID);
        ImGui::DockBuilderDockWindow("Render Settings", RenderSettingsID);
        ImGui::DockBuilderDockWindow("Simulation Debug", RightID);
        ImGui::DockBuilderDockWindow("Log", LogID);
        ImGui::DockBuilderFinish(DockspaceID);
        bDockLayoutInitialized = true;
    }

    void TDebugUI::ResetSurfaceStateSettings()
    {
        const bool bHasStates = SurfaceDataManager->GetSurfaceStateRegistry().GetStateCount() != 0;
        InjectState = bHasStates ? 0 : SurfaceState::InvalidStateId;
        DebugState = InjectState;
        DebugParameterState = InjectState;
        DebugParameterProfile = Asset::InvalidAssetHandle;
        ParameterDraftKey = {Asset::InvalidAssetHandle, SurfaceState::InvalidStateId};
        ParameterDraft = {};
        bParameterDraftAvailable = false;
        bParameterDraftDirty = false;
        ParameterDrafts.clear();
        DirtyParameterDrafts.clear();
        RuntimeProfileOverrides.clear();
        ParameterStatus.clear();
        bSolverStepRequested = false;
        bSolverResetRequested = false;
        if (bHasStates)
        {
            FrameRenderer->SetDebugStateChannel(DebugState);
        }
    }

    void TDebugUI::ReplaceSceneFromPath(TScene& SceneData, const std::filesystem::path& Path)
    {
        TScene Loaded = TSceneLoader::Load(Path, *AssetManager, *SurfaceDataManager);
        TScene PreviousScene = SceneData;
        SceneData = std::move(Loaded);
        try
        {
            // Solver가 임시 Loaded가 아니라 수명이 유지되는 SceneData를 참조하게 한다.
            FrameRenderer->ReloadSceneResources(SceneData);
        }
        catch (...)
        {
            SceneData = std::move(PreviousScene);
            throw;
        }
        ResetSurfaceStateSettings();
        ResetProfilingAverages();
        SelectedObject.reset();
        ActiveGizmoAxis = -1;
    }

    void TDebugUI::RestartScene(TScene& SceneData)
    {
        const std::filesystem::path Path = SceneData.GetSourcePath();
        const TCamera               CurrentCamera = SceneData.GetMainCamera();
        bFrameTimeResetRequested = true;
        SceneData.RestoreInitialState();
        SceneData.GetMainCamera() = CurrentCamera;
        FrameRenderer->RestartSimulationState();
        ResetProfilingAverages();
        SelectedObject.reset();
        ActiveGizmoAxis = -1;
        SceneStatus = Path.empty() ? "Restarted: Scene" : "Restarted: " + Path.filename().string();
    }

    void TDebugUI::DrawSceneWindow(TScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 10.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 115.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Scene", nullptr, Flags);
        const auto& SourcePath = SceneData.GetSourcePath();
        ImGui::TextWrapped("Current Scene: %s",
                           SourcePath.empty() ? "(unsaved)" : SourcePath.filename().string().c_str());
        if (!SourcePath.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", SourcePath.string().c_str());
        }
        if (ImGui::Button("Load Scene"))
        {
            // 취소하거나 로딩에 실패해도 대화상자 대기 시간은 시뮬레이션에서 제외한다.
            bFrameTimeResetRequested = true;
            if (const auto Path = TSceneFileDialog::OpenScene())
            {
                try
                {
                    ReplaceSceneFromPath(SceneData, *Path);
                    SceneStatus = "Loaded: " + Path->filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Load failed: ") + Error.what();
                    TLogger::Error("TDebugUI", SceneStatus);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Scene"))
        {
            bFrameTimeResetRequested = true;
            if (const auto Path = TSceneFileDialog::SaveScene())
            {
                try
                {
                    std::filesystem::path SavePath = *Path;
                    if (SavePath.extension() != ".Scene")
                    {
                        SavePath += ".Scene";
                    }
                    TSceneLoader::Save(SceneData, SavePath);
                    SceneData.SetSourcePath(std::filesystem::absolute(SavePath).lexically_normal());
                    SceneData.CaptureInitialState();
                    SceneStatus = "Saved: " + SavePath.filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Save failed: ") + Error.what();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Restart Scene"))
        {
            try
            {
                RestartScene(SceneData);
            }
            catch (const std::exception& Error)
            {
                SceneStatus = std::string("Restart failed: ") + Error.what();
                TLogger::Error("TDebugUI", SceneStatus);
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("초기 Scene 상태로 복원하고 리소스를 다시 불러오지 않은 채 접촉 입력을 재생합니다.");

        const std::filesystem::path SceneDirectory = std::filesystem::path(MDSS_ASSET_DIR) / "Scenes";
        const auto                  DrawDemoSceneButton = [&](const char* Label, const char* FileName)
        {
            if (ImGui::Button(Label))
            {
                bFrameTimeResetRequested = true;
                const std::filesystem::path DemoScenePath = SceneDirectory / FileName;
                try
                {
                    ReplaceSceneFromPath(SceneData, DemoScenePath);
                    SceneStatus = "Loaded: " + DemoScenePath.filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Load failed: ") + Error.what();
                    TLogger::Error("TDebugUI", SceneStatus);
                }
            }
        };

        DrawDemoSceneButton("Mountain", "Mountain.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("BrickCube", "BrickCube.Scene");
        ImGui::SameLine();
        DrawDemoSceneButton("Bunny", "Bunny.Scene");
        if (!SceneStatus.empty())
        {
            ImGui::TextWrapped("%s", SceneStatus.c_str());
        }
        ImGui::End();
    }

    void TDebugUI::DrawAnimationWindow(TScene& SceneData)
    {
        if (!SceneData.HasDemoAnimation())
        {
            return;
        }

        // 기존 EditorLayout.ini에서도 Scene과 같은 탭 그룹으로 처음 배치한다.
        if (const ImGuiWindow* SceneWindow = ImGui::FindWindowByName("Scene");
            SceneWindow != nullptr && SceneWindow->DockId != 0)
        {
            ImGui::SetNextWindowDockID(SceneWindow->DockId, ImGuiCond_FirstUseEver);
        }
        ImGui::SetNextWindowSize({330.0F, 90.0F}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Animation"))
        {
            const float ControlColumnX = ImGui::GetCursorPosX() + LabeledControlColumnWidth;
            const auto  ContinueControlRow = [ControlColumnX](float NextItemWidth)
            {
                const float RightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
                if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + NextItemWidth <= RightEdge)
                {
                    ImGui::SameLine();
                }
                else
                {
                    ImGui::SetCursorPosX(ControlColumnX);
                }
            };
            const auto RadioWidth = [](const char* Label)
            { return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(Label).x; };

            const bool bAnimationPlaying = SceneData.IsDemoAnimationPlaying();
            BeginLabeledControlRow("Playback");
            if (ImGui::RadioButton("Running", bAnimationPlaying))
            {
                SceneData.PlayDemoAnimation();
            }
            ContinueControlRow(RadioWidth("Paused"));
            if (ImGui::RadioButton("Paused", !bAnimationPlaying))
            {
                SceneData.PauseDemoAnimation();
            }

            BeginLabeledControlRow("Speed");
            constexpr std::array<std::pair<const char*, float>, 4> SpeedPresets = {
                {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F}, {"2x", 2.0F}}};
            bool bFirstPreset = true;
            for (const auto& [Label, Scale] : SpeedPresets)
            {
                if (!bFirstPreset)
                {
                    ContinueControlRow(RadioWidth(Label));
                }
                if (ImGui::RadioButton(Label, std::abs(AnimationTimeScale - Scale) < 0.001F))
                {
                    AnimationTimeScale = Scale;
                }
                bFirstPreset = false;
            }
            ContinueControlRow(160.0F);
            ImGui::SetNextItemWidth(-1.0F);
            SliderFloatWithDoubleClickInput("##AnimationSpeed", &AnimationTimeScale, 0.05F, 4.0F, "%.2fx");
        }
        ImGui::End();
    }

    void TDebugUI::ProcessSelectionAndGizmo(TScene& SceneData)
    {
        if (bInjectMode)
        {
            ActiveGizmoAxis = -1;
            HoveredGizmoAxis = -1;
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Space) && !ShouldSuppressDebugHotkey())
        {
            bRotationGizmoMode = !bRotationGizmoMode;
            ActiveGizmoAxis = -1;
            HoveredGizmoAxis = -1;
        }

        if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size() &&
            !ShouldSuppressDebugHotkey())
        {
            constexpr std::array<ImGuiKey, 9> NumpadKeys = {
                ImGuiKey_Keypad1,
                ImGuiKey_Keypad2,
                ImGuiKey_Keypad3,
                ImGuiKey_Keypad4,
                ImGuiKey_Keypad5,
                ImGuiKey_Keypad6,
                ImGuiKey_Keypad7,
                ImGuiKey_Keypad8,
                ImGuiKey_Keypad9,
            };
            for (std::size_t Index = 0; Index < NumpadKeys.size(); ++Index)
            {
                if (ImGui::IsKeyPressed(NumpadKeys[Index]))
                {
                    ApplyCameraViewPreset(SceneData, Index);
                    break;
                }
            }
        }

        ImGuiIO&     IO = ImGui::GetIO();
        const ImVec2 DisplaySize = IO.DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const glm::vec4      Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + Rect.x * DisplaySize.x,
                                    MainViewport->Pos.y + Rect.y * DisplaySize.y};
        const ImVec2         ViewportSize{Rect.z * DisplaySize.x, Rect.w * DisplaySize.y};
        if (ViewportSize.x <= 0.0F || ViewportSize.y <= 0.0F)
        {
            return;
        }
        const float     Aspect = ViewportSize.x / ViewportSize.y;
        const TCamera&  Camera = GetActiveViewportCamera(SceneData);
        const glm::mat4 VP = Camera.GetViewProjectionMatrix(Aspect);
        const glm::vec3 Origin = Camera.GetPosition();
        const ImVec2    Mouse = IO.MousePos;
        const bool      bMouseInViewport = Mouse.x >= ViewportOrigin.x && Mouse.y >= ViewportOrigin.y &&
                                      Mouse.x < ViewportOrigin.x + ViewportSize.x &&
                                      Mouse.y < ViewportOrigin.y + ViewportSize.y;

        auto GetAxisScreenSegment = [&](glm::vec3 Position, int Axis, ImVec2& A, ImVec2& B, float& WorldScale)
        {
            const glm::vec3 AxisVector = Axis == 0   ? glm::vec3(1, 0, 0)
                                         : Axis == 1 ? glm::vec3(0, 1, 0)
                                                     : glm::vec3(0, 0, 1);
            WorldScale = glm::length(Origin - Position) * 0.18F;
            if (WorldScale <= 0.01F || !ProjectToScreen(Position, VP, ViewportSize, A) ||
                !ProjectToScreen(Position + AxisVector * WorldScale, VP, ViewportSize, B))
            {
                return false;
            }
            A.x += ViewportOrigin.x;
            A.y += ViewportOrigin.y;
            B.x += ViewportOrigin.x;
            B.y += ViewportOrigin.y;
            return true;
        };

        auto GetRotationRingScreenPoint =
            [&](glm::vec3 Position, int Axis, float Angle, float WorldScale, ImVec2& Screen)
        {
            const glm::vec3 U = Axis == 0 ? glm::vec3(0, 1, 0) : Axis == 1 ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0);
            const glm::vec3 V = Axis == 0 ? glm::vec3(0, 0, 1) : Axis == 1 ? glm::vec3(1, 0, 0) : glm::vec3(0, 1, 0);
            const glm::vec3 Point = Position + WorldScale * 0.9F * (U * std::cos(Angle) + V * std::sin(Angle));
            if (!ProjectToScreen(Point, VP, ViewportSize, Screen))
            {
                return false;
            }
            Screen.x += ViewportOrigin.x;
            Screen.y += ViewportOrigin.y;
            return true;
        };

        auto GetRotationRingAngle =
            [&](glm::vec3 Position, int Axis, float WorldScale, ImVec2 MousePosition, float& Angle, float& Distance)
        {
            constexpr int Segments = 96;
            Distance = std::numeric_limits<float>::max();
            bool bFound = false;
            for (int Segment = 0; Segment < Segments; ++Segment)
            {
                const float Angle0 = glm::two_pi<float>() * static_cast<float>(Segment) / Segments;
                const float Angle1 = glm::two_pi<float>() * static_cast<float>(Segment + 1) / Segments;
                ImVec2      A{}, B{};
                if (!GetRotationRingScreenPoint(Position, Axis, Angle0, WorldScale, A) ||
                    !GetRotationRingScreenPoint(Position, Axis, Angle1, WorldScale, B))
                {
                    continue;
                }
                float       Along = 0.0F;
                const float SegmentDistance = PointSegmentDistance(MousePosition, A, B, Along);
                if (SegmentDistance < Distance)
                {
                    Distance = SegmentDistance;
                    Angle = Angle0 + (Angle1 - Angle0) * Along;
                    bFound = true;
                }
            }
            return bFound;
        };

        if (ActiveGizmoAxis >= 0)
        {
            HoveredGizmoAxis = ActiveGizmoAxis;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ActiveGizmoAxis = -1;
            }
            else if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
            {
                if (bRotationGizmoMode)
                {
                    const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse;
                    if (glm::length(MouseDelta) >= 3.0F && GizmoDragWorldScale > 0.0F)
                    {
                        TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
                        float       CurrentAngle = 0.0F, Distance = 0.0F;
                        if (GetRotationRingAngle(Transform.Position,
                                                 ActiveGizmoAxis,
                                                 GizmoDragWorldScale,
                                                 Mouse,
                                                 CurrentAngle,
                                                 Distance))
                        {
                            GizmoDragAccumulatedAngle +=
                                std::remainder(CurrentAngle - GizmoDragLastAngle, glm::two_pi<float>());
                            GizmoDragLastAngle = CurrentAngle;
                            glm::mat4 StartRotation(1.0F);
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.x), glm::vec3(1.0F, 0.0F, 0.0F));
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.y), glm::vec3(0.0F, 1.0F, 0.0F));
                            StartRotation = glm::rotate(
                                StartRotation, glm::radians(GizmoDragStartRotation.z), glm::vec3(0.0F, 0.0F, 1.0F));
                            const glm::vec3 Axis = ActiveGizmoAxis == 0   ? glm::vec3(1, 0, 0)
                                                   : ActiveGizmoAxis == 1 ? glm::vec3(0, 1, 0)
                                                                          : glm::vec3(0, 0, 1);
                            const glm::quat WorldRotation = glm::angleAxis(GizmoDragAccumulatedAngle, Axis) *
                                                            glm::quat_cast(glm::mat3(StartRotation));
                            const glm::mat3 Rotated = glm::mat3_cast(WorldRotation);
                            const float     SinY = std::clamp(Rotated[2][0], -1.0F, 1.0F);
                            const float     Y = std::asin(SinY);
                            const float     CosY = std::cos(Y);
                            float           X = 0.0F, Z = 0.0F;
                            if (std::abs(CosY) > 1.0e-5F)
                            {
                                X = std::atan2(-Rotated[2][1], Rotated[2][2]);
                                Z = std::atan2(-Rotated[1][0], Rotated[0][0]);
                            }
                            else
                            {
                                X = std::atan2(SinY * Rotated[0][1], Rotated[1][1]);
                            }
                            Transform.RotationDegrees = glm::degrees(glm::vec3(X, Y, Z));
                        }
                    }
                }
                else if (glm::length(glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse) >= 3.0F &&
                         GizmoDragPixelLength > 1.0F && GizmoDragWorldScale > 0.0F)
                {
                    const glm::vec2 MouseDelta = glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse;
                    const float     WorldDelta =
                        glm::dot(MouseDelta, GizmoDragScreenAxis) * (GizmoDragWorldScale / GizmoDragPixelLength);
                    const glm::vec3 AxisVector = ActiveGizmoAxis == 0   ? glm::vec3(1, 0, 0)
                                                 : ActiveGizmoAxis == 1 ? glm::vec3(0, 1, 0)
                                                                        : glm::vec3(0, 0, 1);
                    SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position =
                        GizmoDragStartPosition + AxisVector * WorldDelta;
                }
            }
            return;
        }

        HoveredGizmoAxis = -1;
        if (bMouseInViewport && !bMouseOverViewportOverlay && SelectedObject &&
            *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            float           WorldScale = glm::length(Origin - Position) * 0.18F;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;
                if (bRotationGizmoMode)
                {
                    bProjected =
                        WorldScale > 0.01F && GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);
                    if (bProjected)
                    {
                        Distance = PointSegmentDistance(Mouse, A, B, Along);
                    }
                }
                if (bProjected)
                {
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        HoveredGizmoAxis = Axis;
                    }
                }
            }
        }

        const ImGuiWindow* HoveredWindow = GImGui->HoveredWindow;
        const std::string ActiveViewportWindowName = "Viewport " + std::to_string(ActiveViewportId) + "##ViewportPanel";
        const bool        bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;
        if (bMouseOverViewportOverlay || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            HoveredViewportId != ActiveViewportId || !bMouseOverActiveViewportWindow ||
            (bInjectMode && !ImGui::GetIO().KeyShift))
        {
            return;
        }
        if (!bMouseInViewport)
        {
            return;
        }

        if (!ImGui::GetIO().KeyShift && SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            int             BestAxis = -1;
            float           BestAngle = 0.0F;
            const float     WorldScale = glm::length(Origin - Position) * 0.18F;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                float Distance = 0.0F, Angle = 0.0F;
                bool  bProjected = false;
                if (bRotationGizmoMode)
                {
                    bProjected =
                        WorldScale > 0.01F && GetRotationRingAngle(Position, Axis, WorldScale, Mouse, Angle, Distance);
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F, Along = 0.0F;
                    bProjected = GetAxisScreenSegment(Position, Axis, A, B, AxisWorldScale);
                    if (bProjected)
                    {
                        Distance = PointSegmentDistance(Mouse, A, B, Along);
                    }
                }
                if (bProjected)
                {
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        BestAxis = Axis;
                        BestAngle = Angle;
                    }
                }
            }
            if (BestAxis >= 0)
            {
                ActiveGizmoAxis = BestAxis;
                GizmoDragStartMouse = {Mouse.x, Mouse.y};
                TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
                GizmoDragStartPosition = Transform.Position;
                GizmoDragWorldScale = WorldScale;
                if (bRotationGizmoMode)
                {
                    GizmoDragLastAngle = BestAngle;
                    GizmoDragAccumulatedAngle = 0.0F;
                    GizmoDragStartRotation = Transform.RotationDegrees;
                }
                else
                {
                    ImVec2 A{}, B{};
                    float  AxisWorldScale = 0.0F;
                    if (GetAxisScreenSegment(Transform.Position, BestAxis, A, B, AxisWorldScale))
                    {
                        const glm::vec2 ScreenAxis{B.x - A.x, B.y - A.y};
                        GizmoDragPixelLength = glm::length(ScreenAxis);
                        GizmoDragScreenAxis =
                            GizmoDragPixelLength > 0.0F ? ScreenAxis / GizmoDragPixelLength : glm::vec2(0.0F);
                        GizmoDragWorldScale = AxisWorldScale;
                    }
                }
                return;
            }
        }

        const glm::vec2 NDC{(Mouse.x - ViewportOrigin.x) / ViewportSize.x * 2.0F - 1.0F,
                            (Mouse.y - ViewportOrigin.y) / ViewportSize.y * 2.0F - 1.0F};
        const glm::mat4 InvVP = glm::inverse(VP);
        glm::vec4       FarPoint = InvVP * glm::vec4(NDC, 1.0F, 1.0F);
        if (std::abs(FarPoint.w) <= 1.0e-6F)
        {
            SelectedObject.reset();
            return;
        }
        FarPoint /= FarPoint.w;
        const TSurfaceRayHit Hit = TRaycaster::Cast(SceneData, *AssetManager, Origin, glm::vec3(FarPoint) - Origin);
        if (ImGui::GetIO().KeyShift)
        {
            if (Hit.Hit)
                (void)FrameRenderer->InspectTexel(SceneData, Hit.InstanceIndex, Hit.TriangleID, Hit.SimulationUV);
            else
                FrameRenderer->ClearInspectedTexel();
            return;
        }
        if (Hit.Hit)
        {
            SelectedObject = Hit.InstanceIndex;
        }
        else
        {
            SelectedObject.reset();
        }
    }

    void TDebugUI::DrawSelectedObjectWindow(TScene& SceneData)
    {
        if (!SelectedObject || *SelectedObject >= SceneData.GetStaticMeshInstances().size())
        {
            return;
        }
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const ImVec2   Position{Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 135.0F};
        ImGui::SetNextWindowPos(Position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 420.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Selected Object##SceneTools", nullptr, Flags);
        TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
        ImGui::Text("Object %zu", *SelectedObject);
        DrawSectionHeader("Transform");
        if (ImGui::RadioButton("Translate", !bRotationGizmoMode))
        {
            bRotationGizmoMode = false;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", bRotationGizmoMode))
        {
            bRotationGizmoMode = true;
        }
        LabeledDragFloat3("Position", &Transform.Position.x, 0.02F);
        LabeledDragFloat3("Rotation", &Transform.RotationDegrees.x, 0.25F);
        LabeledDragFloat3("Scale", &Transform.Scale.x, 0.02F);
        ImGui::TextDisabled(bRotationGizmoMode ? "Drag a colored ring to rotate around that axis."
                                               : "Drag a colored axis arrow to move along that axis.");
        DrawSectionHeader("Camera View Presets");
        ImGui::TextDisabled("Numpad 1-9 or buttons; rows = pitch, columns = yaw. Focuses the selected mesh.");
        for (std::size_t Index = 0; Index < CameraViewPresetPitchYawDegrees.size(); ++Index)
        {
            if (Index % 3 != 0)
            {
                ImGui::SameLine();
            }
            const std::string Label = std::to_string(Index + 1) + "###CameraViewPreset" + std::to_string(Index + 1);
            if (ImGui::Button(Label.c_str(), {78.0F, 28.0F}))
            {
                ApplyCameraViewPreset(SceneData, Index);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("카메라 피치: %.0f도, 요: %.0f도",
                                  CameraViewPresetPitchYawDegrees[Index][0],
                                  CameraViewPresetPitchYawDegrees[Index][1]);
            }
        }
        ImGui::End();
    }

    void TDebugUI::ProcessCameraInput(TScene& SceneData)
    {
        ImGuiIO& IO = ImGui::GetIO();

        const bool bWindowFocused =
            NativeWindow != nullptr && glfwGetWindowAttrib(NativeWindow, GLFW_FOCUSED) == GLFW_TRUE;
        if (!bWindowFocused)
        {
            if (bRotatingCamera)
            {
                bRotatingCamera = false;
                glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            return;
        }

        const bool         bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const ImGuiWindow* HoveredWindow = GImGui->HoveredWindow;
        const std::string ActiveViewportWindowName = "Viewport " + std::to_string(ActiveViewportId) + "##ViewportPanel";
        const bool        bMouseOverActiveViewportWindow =
            HoveredWindow != nullptr && std::string_view(HoveredWindow->Name) == ActiveViewportWindowName;

        if (!bRotatingCamera && bRightMouseDown && HoveredViewportId != 0 && bMouseOverActiveViewportWindow)
        {
            bRotatingCamera = true;
            ActiveViewportId = HoveredViewportId;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (bRotatingCamera && !bRightMouseDown)
        {
            bRotatingCamera = false;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        TCamera& CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

        if (bRotatingCamera)
        {
            constexpr float MouseSensitivity = 0.12F;
            glm::vec2       RotationDegrees = CameraData.GetRotationDegrees();
            RotationDegrees.x -= IO.MouseDelta.y * MouseSensitivity;
            RotationDegrees.y += IO.MouseDelta.x * MouseSensitivity;
            CameraData.SetRotationDegrees(RotationDegrees);
        }

        if (HoveredViewportId == ActiveViewportId && bMouseOverActiveViewportWindow && IO.MouseWheel != 0.0F)
        {
            const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
            if (glm::dot(ViewDirection, ViewDirection) > 1.0e-12F)
            {
                DollyCamera(CameraData, IO.MouseWheel * (CameraZoomSpeed / 12.0F));
            }
        }

        if (!bRotatingCamera && ShouldSuppressDebugHotkey())
        {
            return;
        }
        if (ActiveGizmoAxis >= 0)
        {
            return;
        }

        // Main-row Minus and Equal keys work on tenkeyless keyboards; Equal also
        // covers the shifted Plus character on the same physical key.
        const float ZoomSteps = CameraZoomSpeed * IO.DeltaTime;
        if (ImGui::IsKeyDown(ImGuiKey_Minus))
        {
            DollyCamera(CameraData, -ZoomSteps);
        }
        if (ImGui::IsKeyDown(ImGuiKey_Equal))
        {
            DollyCamera(CameraData, ZoomSteps);
        }

        const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
        if (glm::dot(ViewDirection, ViewDirection) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Forward = glm::normalize(ViewDirection);
        const glm::vec3 WorldUp{0.0F, 0.0F, 1.0F};
        glm::vec3       FlatForward = Forward - WorldUp * glm::dot(Forward, WorldUp);
        if (glm::dot(FlatForward, FlatForward) <= 1.0e-12F)
        {
            FlatForward = {1.0F, 0.0F, 0.0F};
        }
        FlatForward = glm::normalize(FlatForward);
        const glm::vec3 RightVector = glm::cross(FlatForward, WorldUp);
        if (glm::dot(RightVector, RightVector) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Right = glm::normalize(RightVector);
        glm::vec3       MoveDirection{0.0F};

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            MoveDirection += FlatForward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            MoveDirection -= FlatForward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_D))
        {
            MoveDirection += Right;
        }
        if (ImGui::IsKeyDown(ImGuiKey_A))
        {
            MoveDirection -= Right;
        }

        if (glm::dot(MoveDirection, MoveDirection) <= 1.0e-12F)
        {
            return;
        }

        constexpr float FastMoveMultiplier = 3.0F;
        const float     Speed = CameraMoveSpeed * (IO.KeyShift ? FastMoveMultiplier : 1.0F);
        const glm::vec3 Delta = glm::normalize(MoveDirection) * Speed * IO.DeltaTime;
        CameraData.SetPosition(CameraData.GetPosition() + Delta);
        CameraData.SetTarget(CameraData.GetTarget() + Delta);
    }

    void TDebugUI::DrawCameraWindow(TScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    CameraWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{CameraWidth, 275.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + 10.0F, Viewport->WorkPos.y + 10.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({240.0F, 275.0F},
                                            {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()});

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;

        if (ImGui::Begin("Camera", nullptr, Flags))
        {
            TCamera& CameraData = GetViewportCamera(SceneData, GetActiveViewportIndex());

            ImGui::TextDisabled("Viewport %u", ActiveViewportId);

            if (ImGui::Button("Reset Camera View"))
            {
                CameraData = SceneData.GetInitialCamera();
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("시작 시 카메라 위치, 시선 대상, FOV로 되돌립니다.");
            }

            glm::vec3 Position = CameraData.GetPosition();
            if (LabeledDragFloat3("Position", &Position.x, 0.05F))
            {
                const glm::vec3 Delta = Position - CameraData.GetPosition();
                const glm::vec3 ShiftedTarget = CameraData.GetTarget() + Delta;
                CameraData.SetPosition(Position);
                CameraData.SetTarget(ShiftedTarget);
            }

            glm::vec2 RotationDegrees = CameraData.GetRotationDegrees();
            if (LabeledDragFloat2("Angle (Pitch/Yaw)", &RotationDegrees.x, 0.25F))
            {
                CameraData.SetRotationDegrees(RotationDegrees);
            }

            float FieldOfViewDegrees = CameraData.GetVerticalFieldOfViewDegrees();
            if (LabeledSliderFloat("FOV", &FieldOfViewDegrees, 20.0F, 120.0F, "%.1f deg"))
            {
                CameraData.SetVerticalFieldOfViewDegrees(FieldOfViewDegrees);
            }

            LabeledSliderFloat("Zoom speed", &CameraZoomSpeed, 4.0F, 36.0F, "%.0f steps/s");
            LabeledSliderFloat("Move speed", &CameraMoveSpeed, 0.5F, 20.0F, "%.1f units/s");

            ImGui::Separator();
            ImGui::TextDisabled("RMB + Mouse: look  |  WASD: move");
            ImGui::TextDisabled("Shift: 3x move  |  Wheel / hold - / = (+): zoom");
        }
        ImGui::End();
    }

    void TDebugUI::DrawRenderOptionsWindow(TScene& SceneData)
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }
        ImGuiDockNode* ViewportNode = DockspaceID != 0 ? ImGui::DockBuilderGetCentralNode(DockspaceID) : nullptr;
        if (ViewportNode == nullptr)
        {
            return;
        }
        constexpr float ToolbarHeight = 48.0F;
        ImGui::SetNextWindowPos(ViewportNode->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ViewportNode->Size.x, ToolbarHeight}, ImGuiCond_Always);
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2         DisplaySize = ImGui::GetIO().DisplaySize;
        if (DisplaySize.x > 0.0F && DisplaySize.y > 0.0F)
        {
            ViewportWorkspaceRectNormalized = {(ViewportNode->Pos.x - MainViewport->Pos.x) / DisplaySize.x,
                                               (ViewportNode->Pos.y + ToolbarHeight - MainViewport->Pos.y) /
                                                   DisplaySize.y,
                                               ViewportNode->Size.x / DisplaySize.x,
                                               std::max(ViewportNode->Size.y - ToolbarHeight, 1.0F) / DisplaySize.y};
        }
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNavFocus;
        Rendering::TRenderViewMode CurrentMode = FrameRenderer->GetRenderViewMode();
        const auto                 GetViewGeometryDescription = [](Rendering::TRenderViewMode Mode) -> const char*
        {
            switch (Mode)
            {
                case Rendering::TRenderViewMode::Lit:
                    return "기본적으로 원본 메시를 렌더링합니다. Mud/WaterFilm/Lava 적층 레이어가 활성화된 물체는 "
                           "베이스 표면도 텍셀 메시로 그립니다.";
                case Rendering::TRenderViewMode::Unlit:
                    return "원본 메시 위치에 조명 없이 재질색을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::Wireframe:
                    return "원본 메시 삼각형을 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::VertexNormalWS:
                    return "원본 메시 정점 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::NormalTextureTS:
                    return "원본 메시의 노멀 텍스처를 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::MappedNormalWS:
                    return "원본 메시의 매핑된 노멀을 표시합니다. 형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceStateHeatmap:
                    return "원본 메시 위에 State 데이터를 색으로 표시합니다. Relief 음영은 Meso 노멀을 쓸 수 있지만 "
                           "형상 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceValidity:
                case Rendering::TRenderViewMode::SurfaceID:
                case Rendering::TRenderViewMode::NeighborCount:
                case Rendering::TRenderViewMode::SurfaceSeam:
                case Rendering::TRenderViewMode::OutgoingFluxScale:
                case Rendering::TRenderViewMode::SolverTransferWeight:
                case Rendering::TRenderViewMode::SurfaceTexelGrid:
                case Rendering::TRenderViewMode::SurfaceTexelArea:
                    return "원본 메시 위에 진단 데이터를 표시합니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::MesoHeight:
                case Rendering::TRenderViewMode::MesoOffset:
                    return "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.";
                case Rendering::TRenderViewMode::MacroGeometry:
                    return "원본 메시 형상입니다. Meso와 State 적층 변위는 없습니다.";
                case Rendering::TRenderViewMode::SurfaceAccumulation:
                    return "Meso + 선택 State 전체 높이를 형상에 적용합니다. 색은 선택한 적층 성분을 표시합니다.";
                case Rendering::TRenderViewMode::SurfaceFinalGeometry:
                    return "Meso + 선택 State 적층을 적용한 형상입니다. 모든 State를 합친 최종 형상은 아닙니다.";
                case Rendering::TRenderViewMode::TotalSimulationHeight:
                    return "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.";
            }
            return "원본 메시 형상입니다.";
        };

        if (ImGui::Begin("Render Options##ViewportToolbar", nullptr, Flags))
        {
            const int   CurrentIndex = static_cast<int>(CurrentMode);
            const char* CurrentName = "Solver Transfer Weights";
            if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(RenderViewModeNames.size()))
            {
                CurrentName = RenderViewModeNames[CurrentIndex];
            }
            else if (CurrentMode >= Rendering::TRenderViewMode::SurfaceStateHeatmap &&
                     CurrentMode < Rendering::TRenderViewMode::SolverTransferWeight)
            {
                CurrentName = SurfaceDebugViewNames[CurrentIndex -
                                                    static_cast<int>(Rendering::TRenderViewMode::SurfaceStateHeatmap)];
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation)
                CurrentName = "State Height Heatmap";
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry)
                CurrentName = "Meso + State Geometry";
            else if (CurrentMode == Rendering::TRenderViewMode::TotalSimulationHeight)
                CurrentName = "Total Simulation Height";
            else if (CurrentMode == Rendering::TRenderViewMode::MesoHeight ||
                     CurrentMode == Rendering::TRenderViewMode::MesoOffset)
            {
                CurrentName = "Meso Geometry";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::MacroGeometry)
            {
                CurrentName = "Base Geometry";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceTexelGrid)
            {
                CurrentName = "Texel Grid";
            }
            else if (CurrentMode == Rendering::TRenderViewMode::SurfaceTexelArea)
            {
                CurrentName = "Texel Area Heatmap";
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("View");
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::SetNextItemWidth(185.0F);
            ImGui::PushID("ViewportViewCombo");
            const bool bViewComboOpen = ImGui::BeginCombo("##View", CurrentName, ImGuiComboFlags_HeightLarge);
            if (bViewComboOpen)
            {
                const auto DrawSelectionMark = [&](bool bSelected)
                {
                    if (!bSelected)
                    {
                        return;
                    }
                    const ImVec2 Minimum = ImGui::GetItemRectMin();
                    const ImVec2 Maximum = ImGui::GetItemRectMax();
                    const float  Size = ImGui::GetFontSize() * 0.62F;
                    const float  X = Maximum.x - ImGui::GetStyle().FramePadding.x - Size;
                    const float  Y = (Minimum.y + Maximum.y) * 0.5F;
                    const ImU32  Color = ImGui::GetColorU32({0.35F, 0.78F, 1.0F, 1.0F});
                    ImDrawList*  DrawList = ImGui::GetWindowDrawList();
                    DrawList->AddLine({X, Y}, {X + Size * 0.34F, Y + Size * 0.32F}, Color, 2.0F);
                    DrawList->AddLine({X + Size * 0.34F, Y + Size * 0.32F}, {X + Size, Y - Size * 0.42F}, Color, 2.0F);
                };
                DrawSectionHeader("RENDER", 0.0F, 2.0F);
                const auto SelectDisplayView = [&](int Index, const char* Description = nullptr)
                {
                    const auto Mode = static_cast<Rendering::TRenderViewMode>(Index);
                    const bool bSelected = CurrentMode == Mode;
                    const bool bClicked = ImGui::MenuItem(RenderViewModeNames[Index]);
                    DrawSelectionMark(bSelected);
                    if (bClicked)
                    {
                        CurrentMode = Mode;
                        FrameRenderer->SetRenderViewMode(Mode);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        SetDescriptionTooltip(Description != nullptr ? Description : GetViewGeometryDescription(Mode));
                    }
                };
                for (int Index = 0; Index < 3; ++Index)
                {
                    SelectDisplayView(Index);
                }
                if (ImGui::BeginMenu("Normal"))
                {
                    SelectDisplayView(3, "원본 메시의 정점 노멀을 월드 공간 RGB로 표시합니다.");
                    SelectDisplayView(4, "원본 메시의 노멀 텍스처를 탄젠트 공간 RGB로 표시합니다.");
                    SelectDisplayView(5, "원본 메시의 매핑된 노멀을 월드 공간 RGB로 표시합니다.");
                    ImGui::EndMenu();
                }
                DrawSectionHeader("DEBUG", 12.0F, 2.0F);
                const auto SelectDebugView =
                    [&](const char* Name, Rendering::TRenderViewMode Mode, const char* Description = nullptr)
                {
                    const bool bMesoGeometry = Mode == Rendering::TRenderViewMode::MesoHeight &&
                                               (CurrentMode == Rendering::TRenderViewMode::MesoHeight ||
                                                CurrentMode == Rendering::TRenderViewMode::MesoOffset);
                    const bool bSelected = CurrentMode == Mode || bMesoGeometry;
                    const bool bClicked = ImGui::MenuItem(Name);
                    DrawSelectionMark(bSelected);
                    if (bClicked)
                    {
                        if (!bMesoGeometry)
                        {
                            CurrentMode = Mode;
                        }
                        FrameRenderer->SetRenderViewMode(CurrentMode);
                    }
                    if (ImGui::IsItemHovered())
                    {
                        SetDescriptionTooltip(Description != nullptr ? Description : GetViewGeometryDescription(Mode));
                    }
                };
                SelectDebugView(
                    SurfaceDebugViewNames[0],
                    Rendering::TRenderViewMode::SurfaceStateHeatmap,
                    "원본 메시 위에 State/용량 비율을 색으로 표시합니다. Meso와 적층 변위는 적용하지 않습니다.");
                if (ImGui::BeginMenu("Surface"))
                {
                    SelectDebugView(SurfaceDebugViewNames[1],
                                    Rendering::TRenderViewMode::SurfaceValidity,
                                    "원본 메시 위에 텍셀의 유효성과 시뮬레이션 활성 상태를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[2],
                                    Rendering::TRenderViewMode::SurfaceID,
                                    "원본 메시 위에 Surface ID를 색으로 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[3],
                                    Rendering::TRenderViewMode::NeighborCount,
                                    "원본 메시 위에 텍셀 이웃 수를 표시합니다.");
                    SelectDebugView(SurfaceDebugViewNames[4],
                                    Rendering::TRenderViewMode::SurfaceSeam,
                                    "원본 메시 위에 UV chart 사이의 텍셀 이웃 경계를 표시합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Texel"))
                {
                    SelectDebugView("Texel Grid",
                                    Rendering::TRenderViewMode::SurfaceTexelGrid,
                                    "원본 메시 표면에 시뮬레이션 텍셀 격자를 표시합니다.");
                    SelectDebugView("Texel Area Heatmap",
                                    Rendering::TRenderViewMode::SurfaceTexelArea,
                                    "원본 메시 표면에 텍셀별 월드 면적을 표시합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Geometry"))
                {
                    SelectDebugView("Base Geometry",
                                    Rendering::TRenderViewMode::MacroGeometry,
                                    "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                    SelectDebugView("Meso Geometry",
                                    Rendering::TRenderViewMode::MesoHeight,
                                    "Meso 높이만 형상에 적용합니다. 표시 방식은 View Settings에서 선택합니다.");
                    SelectDebugView(
                        "State Height Heatmap",
                        Rendering::TRenderViewMode::SurfaceAccumulation,
                        "Meso + 선택 State 전체 높이로 형상을 변위합니다. 색은 선택한 적층 성분을 표시합니다.");
                    SelectDebugView("Meso + State Geometry",
                                    Rendering::TRenderViewMode::SurfaceFinalGeometry,
                                    "Meso + 선택 State 적층을 적용한 형상입니다. 선택 State 미리보기이며 모든 State의 "
                                    "최종 합성이 아닙니다.");
                    SelectDebugView("Total Simulation Height",
                                    Rendering::TRenderViewMode::TotalSimulationHeight,
                                    "현재 시뮬레이션 상태의 모든 State 높이를 합쳐 형상에 적용합니다.");
                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Solver"))
                {
                    SelectDebugView(SurfaceDebugViewNames[5],
                                    Rendering::TRenderViewMode::OutgoingFluxScale,
                                    "원본 메시 위에 State별 유출 제한값을 표시합니다.");
                    SelectDebugView("Solver Transfer Weights",
                                    Rendering::TRenderViewMode::SolverTransferWeight,
                                    "원본 메시 위에 선택한 Solver 전달 가중치를 표시합니다.");
                    ImGui::EndMenu();
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            if (!bViewComboOpen && ImGui::IsItemHovered())
            {
                SetDescriptionTooltip(GetViewGeometryDescription(CurrentMode));
            }

            ImGui::SameLine(0.0F, 10.0F);
            ImGui::TextDisabled("|");

            if (CurrentMode == Rendering::TRenderViewMode::SurfaceStateHeatmap ||
                CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation ||
                CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry)
            {
                ImGui::SameLine(0.0F, 8.0F);
                const SurfaceState::TSurfaceStateRegistry* Registry =
                    AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;
                if (StateCount == 0)
                {
                    ImGui::TextDisabled("State: none");
                }
                else
                {
                    if (DebugState >= StateCount)
                    {
                        DebugState = 0;
                    }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("State");
                    ImGui::SameLine(0.0F, 6.0F);
                    ImGui::SetNextItemWidth(145.0F);
                    const char* StateComboId = CurrentMode == Rendering::TRenderViewMode::SurfaceStateHeatmap
                                                   ? "##ToolbarHeatmapState"
                                                   : "##ToolbarHeightState";
                    if (ImGui::BeginCombo(StateComboId, Registry->GetStateName(DebugState).c_str()))
                    {
                        for (std::size_t Index = 0; Index < StateCount; ++Index)
                        {
                            const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                            const bool                   bSelected = State == DebugState;
                            if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                            {
                                DebugState = State;
                                FrameRenderer->SetDebugStateChannel(DebugState);
                            }
                            if (bSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                if (CurrentMode == Rendering::TRenderViewMode::SurfaceAccumulation)
                {
                    ImGui::SameLine(0.0F, 8.0F);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted("Color");
                    ImGui::SameLine(0.0F, 6.0F);
                    auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    int  Component = static_cast<int>(Settings.AccumulationComponent);
                    ImGui::SetNextItemWidth(185.0F);
                    if (ImGui::Combo(
                            "##ToolbarHeightComponent",
                            &Component,
                            "Total Height\0Cavity Height\0Following Height\0Cavity Fill\0Filled Cavities Only\0"))
                    {
                        Settings.AccumulationComponent = static_cast<std::uint32_t>(Component);
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    }
                }
            }

            ImGui::SameLine(0.0F, 6.0F);
            const float  GearButtonSize = ImGui::GetFrameHeight();
            const bool   bOpenViewSettings = ImGui::Button("##ViewSettingsButton", {GearButtonSize, GearButtonSize});
            const ImVec2 GearMinimum = ImGui::GetItemRectMin();
            const ImVec2 GearMaximum = ImGui::GetItemRectMax();
            const ImVec2 GearCenter{(GearMinimum.x + GearMaximum.x) * 0.5F, (GearMinimum.y + GearMaximum.y) * 0.5F};
            const ImU32  GearColor = ImGui::GetColorU32(ImGuiCol_Text);
            ImDrawList*  DrawList = ImGui::GetWindowDrawList();
            DrawList->AddCircle(GearCenter, 4.5F, GearColor, 8, 1.4F);
            for (int Tooth = 0; Tooth < 8; ++Tooth)
            {
                const float  Angle = glm::two_pi<float>() * static_cast<float>(Tooth) / 8.0F;
                const ImVec2 Direction{std::cos(Angle), std::sin(Angle)};
                DrawList->AddLine({GearCenter.x + Direction.x * 4.5F, GearCenter.y + Direction.y * 4.5F},
                                  {GearCenter.x + Direction.x * 6.3F, GearCenter.y + Direction.y * 6.3F},
                                  GearColor,
                                  1.4F);
            }
            DrawList->AddCircleFilled(GearCenter, 1.7F, ImGui::GetColorU32(ImGuiCol_Button));
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("뷰 설정");
            }
            if (bOpenViewSettings)
            {
                ImGui::OpenPopup("ViewSettingsPopup");
            }

            const ImGuiStyle& Style = ImGui::GetStyle();
            const auto        CheckboxWidth = [&](const char* Label)
            { return ImGui::GetFrameHeight() + Style.ItemInnerSpacing.x + ImGui::CalcTextSize(Label).x; };
            const auto ZoomButtonWidth = [&](const char* Label)
            { return ImGui::CalcTextSize(Label).x + Style.FramePadding.x * 2.0F; };
            const float RightControlsWidth = CheckboxWidth("Grid") + CheckboxWidth("Axis") + CheckboxWidth("Overlays") +
                                             ZoomButtonWidth("Metrics") + ImGui::CalcTextSize("|").x +
                                             ZoomButtonWidth("-") + ZoomButtonWidth("+") + Style.ItemSpacing.x * 4.0F +
                                             24.0F;
            const float RightControlsStart = ImGui::GetWindowWidth() - Style.WindowPadding.x - RightControlsWidth;
            ImGui::SameLine(0.0F, 6.0F);
            if (RightControlsStart > ImGui::GetCursorPosX() + Style.ItemSpacing.x)
            {
                ImGui::SetCursorPosX(RightControlsStart);
            }

            bool bGridVisible = FrameRenderer->IsWorldGridVisible();
            if (ImGui::Checkbox("Grid", &bGridVisible))
            {
                FrameRenderer->SetWorldGridVisible(bGridVisible);
            }
            ImGui::SameLine();
            bool bAxisVisible = FrameRenderer->IsWorldAxisVisible();
            if (ImGui::Checkbox("Axis", &bAxisVisible))
            {
                FrameRenderer->SetWorldAxisVisible(bAxisVisible);
            }
            ImGui::SameLine();
            ImGui::Checkbox("Overlays", &bViewportOverlaysVisible);
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("프로파일링 및 렌더 상태 오버레이를 표시하거나 숨깁니다.");
            }
            ImGui::SameLine(0.0F, 4.0F);
            if (ImGui::SmallButton("Metrics"))
            {
                ImGui::OpenPopup("ProfilingDisplayPopup");
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("프로파일링 열과 상세 수준을 선택합니다.");
            }
            if (ImGui::BeginPopup("ProfilingDisplayPopup"))
            {
                ImGui::TextDisabled("Profiling display");
                ImGui::Separator();
                ImGui::Checkbox("Average (1s)", &bShowProfilingAverage);
                ImGui::Checkbox("Max (1s)", &bShowProfilingMaximum);
                ImGui::Checkbox("Past 100ms", &bShowProfilingPast100ms);
                ImGui::Separator();
                ImGui::Checkbox("Detailed breakdowns", &bShowDetailedProfiling);
                if (ImGui::Checkbox("Solver timings per step", &bSolverMetricsPerStep))
                {
                    ResetProfilingAverages();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(
                        "Solver GPU 시간을 프레임 전체 시간으로 표시하거나 시뮬레이션 단계 수로 나누어 표시합니다.");
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine(0.0F, 10.0F);
            ImGui::TextDisabled("|");

            ImGui::SameLine(0.0F, 10.0F);
            if (ImGui::Button("-"))
            {
                DollyCamera(GetViewportCamera(SceneData, GetActiveViewportIndex()), -1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("축소합니다.");
            }
            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                DollyCamera(GetViewportCamera(SceneData, GetActiveViewportIndex()), 1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip("확대합니다.");
            }

            if (bOpenViewSettings || ImGui::IsPopupOpen("ViewSettingsPopup"))
                ImGui::SetNextWindowSizeConstraints({360.0F, 0.0F}, {560.0F, 600.0F});
            if (ImGui::BeginPopup("ViewSettingsPopup"))
            {
                const char* ViewSettingsTitle = CurrentName;
                ImGui::TextColored({0.46F, 0.68F, 0.92F, 1.0F}, "%s", ViewSettingsTitle);
                ImGui::Separator();
                const ImVec4 ContextTitleColor{0.24F, 0.48F, 0.82F, 1.0F};
                auto         BeginViewContext = [&](const char* Title, const char* Description)
                {
                    ImGui::TextColored(ContextTitleColor, "%s", Title);
                    ImGui::TextWrapped("%s", Description);
                    ImGui::PushTextWrapPos(0.0F);
                    ImGui::TextDisabled("형상 기준: %s", GetViewGeometryDescription(CurrentMode));
                    ImGui::PopTextWrapPos();
                    ImGui::Spacing();
                };
                auto DrawHeightGridControls = [&]()
                {
                    auto        Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                    int         GridMode = static_cast<int>(Settings.HeightGridMode);
                    const char* Modes[] = {"Off", "Overlay", "Grid only"};
                    bool        Changed = ImGui::Combo("Height grid", &GridMode, Modes, 3);
                    Settings.HeightGridMode = static_cast<std::uint32_t>(GridMode);
                    ImGui::SameLine();
                    int BlockSize = static_cast<int>(Settings.HeightGridBlockSize);
                    ImGui::SetNextItemWidth(130.0F);
                    Changed |= SliderIntWithDoubleClickInput("Cell texels", &BlockSize, 1, 64);
                    Settings.HeightGridBlockSize = static_cast<std::uint32_t>(BlockSize);
                    if (Changed)
                    {
                        FrameRenderer->SetSurfaceDebugDisplaySettings(Settings);
                    }
                };

                switch (CurrentMode)
                {
                    case Rendering::TRenderViewMode::Lit:
                        BeginViewContext("LIT", "재질색 × 조명(노멀 맵 포함).");
                        break;
                    case Rendering::TRenderViewMode::Unlit:
                        BeginViewContext("UNLIT", "조명 없이 재질색만 표시.");
                        break;
                    case Rendering::TRenderViewMode::Wireframe:
                    {
                        BeginViewContext("WIREFRAME", "삼각형 와이어프레임으로 형상을 표시.");
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
                        break;
                    }
                    case Rendering::TRenderViewMode::VertexNormalWS:
                        BeginViewContext("VERTEX NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::NormalTextureTS:
                        BeginViewContext("NORMAL TEXTURE",
                                         "RGB 채널은 탄젠트 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::MappedNormalWS:
                        BeginViewContext("MAPPED NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case Rendering::TRenderViewMode::SurfaceStateHeatmap:
                    {
                        BeginViewContext("STATE HEATMAP",
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
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceAccumulation:
                    case Rendering::TRenderViewMode::SurfaceFinalGeometry:
                    {
                        const bool bFinal = CurrentMode == Rendering::TRenderViewMode::SurfaceFinalGeometry;
                        BeginViewContext(bFinal ? "MESO + STATE GEOMETRY" : "STATE HEIGHT HEATMAP",
                                         bFinal ? "Meso와 선택 State의 높이를 적용한 형상입니다. 선택 State "
                                                  "미리보기이며 모든 State의 최종 합성이 아닙니다."
                                                : "형상에는 Meso와 선택 State 전체 높이를 적용합니다. 색은 선택한 적층 "
                                                  "성분을 표시하며, UV chart 경계는 열려 있습니다.");
                        auto Settings = FrameRenderer->GetSurfaceDebugDisplaySettings();
                        bool Changed = false;
                        ImGui::NewLine();
                        ImGui::SetNextItemWidth(95.0F);
                        Changed |= ImGui::DragFloat("Display scale",
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
                        if (Changed)
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
                        break;
                    }
                    case Rendering::TRenderViewMode::TotalSimulationHeight:
                    {
                        BeginViewContext("TOTAL SIMULATION HEIGHT",
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
                    case Rendering::TRenderViewMode::SurfaceValidity:
                        BeginViewContext("VALIDITY", "Surface 텍셀 유효 여부와 시뮬레이션 적용 여부.");
                        DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "유효·시뮬레이션 켜짐");
                        DrawLegendColor({0.18F, 0.48F, 0.82F, 1.0F}, "유효·시뮬레이션 꺼짐");
                        DrawLegendColor({0.86F, 0.12F, 0.08F, 1.0F}, "무효");
                        break;
                    case Rendering::TRenderViewMode::SurfaceID:
                        BeginViewContext("SURFACE ID",
                                         "색으로 Surface를 구분합니다. 색상 자체는 수치 의미가 없습니다.");
                        break;
                    case Rendering::TRenderViewMode::NeighborCount:
                        BeginViewContext("NEIGHBOR COUNT", "유효한 이웃 텍셀 개수.");
                        DrawLegendColor({0.08F, 0.10F, 0.18F, 1.0F}, "0개");
                        DrawLegendColor({0.95F, 0.72F, 0.12F, 1.0F}, "8개");
                        break;
                    case Rendering::TRenderViewMode::SurfaceSeam:
                        BeginViewContext("UV SEAM", "다른 UV chart로 이어지는 이웃 관계가 있는 경계.");
                        DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "Seam 이웃 있음");
                        DrawLegendColor({0.12F, 0.16F, 0.22F, 1.0F}, "없음");
                        break;
                    case Rendering::TRenderViewMode::OutgoingFluxScale:
                    {
                        BeginViewContext("OUTGOING FLUX SCALE",
                                         "선택 State 채널의 유출 제한값: 0은 제한, 1은 제한 없음.");
                        const SurfaceState::TSurfaceStateRegistry* Registry =
                            AssetManager != nullptr ? &SurfaceDataManager->GetSurfaceStateRegistry() : nullptr;
                        if (Registry != nullptr && Registry->GetStateCount() > 0)
                        {
                            const std::size_t StateCount = Registry->GetStateCount();
                            DebugState = std::min<SurfaceState::TStateId>(
                                DebugState, static_cast<SurfaceState::TStateId>(StateCount - 1));
                            ImGui::AlignTextToFramePadding();
                            ImGui::TextUnformatted("State");
                            ImGui::SameLine(0.0F, 6.0F);
                            ImGui::SetNextItemWidth(125.0F);
                            if (ImGui::BeginCombo("##OutgoingFluxState", Registry->GetStateName(DebugState).c_str()))
                            {
                                for (std::size_t Index = 0; Index < StateCount; ++Index)
                                {
                                    const SurfaceState::TStateId State = static_cast<SurfaceState::TStateId>(Index);
                                    const bool                   bSelected = State == DebugState;
                                    if (ImGui::Selectable(Registry->GetStateName(State).c_str(), bSelected))
                                    {
                                        DebugState = State;
                                        FrameRenderer->SetDebugStateChannel(DebugState);
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
                        break;
                    }
                    case Rendering::TRenderViewMode::SolverTransferWeight:
                    {
                        BeginViewContext("TRANSFER WEIGHT", "선택 가중치 평균: 0은 차단, 1은 완전 전달.");
                        int SelectedWeightView = static_cast<int>(FrameRenderer->GetSolverTransferWeightView());
                        ImGui::SetNextItemWidth(205.0F);
                        if (ImGui::Combo("##TransferWeightComponent",
                                         &SelectedWeightView,
                                         SolverTransferWeightViewNames.data(),
                                         static_cast<int>(SolverTransferWeightViewNames.size())))
                        {
                            FrameRenderer->SetSolverTransferWeightView(
                                static_cast<Rendering::TSolverTransferWeightView>(SelectedWeightView));
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        DrawLegendColor({0.16F, 0.035F, 0.24F, 1.0F}, "0 차단");
                        DrawLegendColor({0.30F, 0.24F, 0.78F, 1.0F}, "0.5 중간");
                        DrawLegendColor({0.18F, 0.94F, 0.98F, 1.0F}, "1 허용");
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceTexelGrid:
                    {
                        BeginViewContext("TEXEL GRID", "시뮬레이션 UV 격자. 확대하면 개별 텍셀 경계가 나타납니다.");
                        const std::uint32_t                  BlockSize = FrameRenderer->GetTexelGridBlockSize();
                        int                                  SelectedBlock = BlockSize == 16U ? 1 : 0;
                        constexpr std::array<const char*, 2> BlockNames = {"8 x 8 texels", "16 x 16 texels"};
                        ImGui::SetNextItemWidth(150.0F);
                        if (ImGui::Combo("##TexelGridBlock",
                                         &SelectedBlock,
                                         BlockNames.data(),
                                         static_cast<int>(BlockNames.size())))
                        {
                            FrameRenderer->SetTexelGridBlockSize(SelectedBlock == 0 ? 8U : 16U);
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        ImGui::Text("Resolution: %u x %u",
                                    FrameRenderer->GetSimulationResolution(),
                                    FrameRenderer->GetSimulationResolution());
                        DrawLegendColor({0.42F, 0.46F, 0.51F, 1.0F}, "1 텍셀");
                        DrawLegendColor({0.72F, 0.88F, 0.98F, 1.0F}, "묶음 경계");
                        break;
                    }
                    case Rendering::TRenderViewMode::SurfaceTexelArea:
                    {
                        BeginViewContext("TEXEL AREA", "텍셀당 월드 면적. 해상도 변경에도 동일 색 기준.");
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
                        break;
                    }
                    case Rendering::TRenderViewMode::MesoHeight:
                    case Rendering::TRenderViewMode::MesoOffset:
                    {
                        BeginViewContext("MESO GEOMETRY",
                                         "Meso 높이만 형상에 적용합니다. State 적층은 포함하지 않습니다.");
                        int Representation = CurrentMode == Rendering::TRenderViewMode::MesoHeight ? 0 : 1;
                        ImGui::SetNextItemWidth(150.0F);
                        if (ImGui::Combo("Representation", &Representation, "Height Color\0Displacement\0"))
                        {
                            CurrentMode = Representation == 0 ? Rendering::TRenderViewMode::MesoHeight
                                                              : Rendering::TRenderViewMode::MesoOffset;
                            FrameRenderer->SetRenderViewMode(CurrentMode);
                        }
                        if (CurrentMode == Rendering::TRenderViewMode::MesoHeight)
                        {
                            ImGui::SameLine(0.0F, 10.0F);
                            DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "음수");
                            DrawLegendColor({0.12F, 0.13F, 0.17F, 1.0F}, "0 기준");
                            DrawLegendColor({1.0F, 0.42F, 0.10F, 1.0F}, "양수");
                        }
                        DrawHeightGridControls();
                        break;
                    }
                    case Rendering::TRenderViewMode::MacroGeometry:
                        BeginViewContext("BASE GEOMETRY",
                                         "원본 메시 형상입니다. Meso 변위와 State 적층은 포함하지 않습니다.");
                        break;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    void TDebugUI::DrawViewportPanels(TScene&)
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
        for (std::size_t Index = 0; Index < Count; ++Index)
        {
            TViewportPanel&   Panel = ViewportPanels[Index];
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
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }
    }

    void TDebugUI::DrawBrickCubeTexelMeshNotice(const TScene& SceneData)
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

    void TDebugUI::DrawRenderSettingsWindow(TScene& SceneData)
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        if (ImGui::Begin("Render Settings"))
        {
            if (ImGui::BeginTabBar("RenderSettingsTabs"))
            {
                if (ImGui::BeginTabItem("Surface"))
                {
                    if (ImGui::BeginChild("SurfaceSettingsContent",
                                          {0.0F, 0.0F},
                                          ImGuiChildFlags_None,
                                          ImGuiWindowFlags_NoBackground))
                    {
                        DrawSectionHeader("Surface Shading");
                        float AmbientLight = FrameRenderer->GetAmbientLight();
                        if (LabeledSliderFloat("Ambient Strength", &AmbientLight, 0.0F, 1.0F, "%.2f"))
                        {
                            FrameRenderer->SetAmbientLight(AmbientLight);
                        }

                        DrawSectionHeader("Normal Map");
                        float NormalStrength = FrameRenderer->GetNormalStrength();
                        if (LabeledSliderFloat("Normal Strength", &NormalStrength, 0.0F, 4.0F, "%.2f"))
                        {
                            FrameRenderer->SetNormalStrength(NormalStrength);
                        }

                        bool bFlipNormalY = FrameRenderer->GetFlipNormalY();
                        if (ImGui::Checkbox("Flip Y", &bFlipNormalY))
                        {
                            FrameRenderer->SetFlipNormalY(bFlipNormalY);
                        }
                        if (ImGui::IsItemHovered())
                            SetDescriptionTooltip(
                                "화면의 Normal Map 조명만 바꿉니다. Meso 형상은 기본 Flip Y 켜짐으로 전처리됩니다.");
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Effects"))
                {
                    if (ImGui::BeginChild(
                            "EffectSettingsContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawSectionHeader("Common");
                        auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
                        bool EffectsChanged = ImGui::Checkbox("Enable Lit effects", &Effects.bEnabled);
                        EffectsChanged |= ImGui::Checkbox("Height-field smoothing", &Effects.bHeightFieldSmoothing);
                        if (ImGui::IsItemHovered())
                            SetDescriptionTooltip(
                                "렌더링용 State 적층 높이에 3x3 가우시안 필터링을 적용합니다.\n"
                                "가중치: 1 2 1 / 2 4 2 / 1 2 1 (기본 합 16).\n"
                                "같은 Surface, UV chart, Profile의 적층 텍셀만 섞고 유효 가중치로 나눕니다.\n"
                                "시뮬레이션 높이와 Solver는 변경하지 않습니다.");
                        EffectsChanged |=
                            LabeledSliderFloat("Dry roughness", &Effects.DryRoughness, 0.05F, 1.0F, "%.2f");
                        float LitHeightDisplayScale = SceneData.GetLitHeightDisplayScale();
                        if (LabeledSliderFloat("Lit height display scale", &LitHeightDisplayScale, 0.0F, 10.0F, "%.2f"))
                            FrameRenderer->SetSceneLitHeightDisplayScale(SceneData, LitHeightDisplayScale);

                        ImGui::Spacing();
                        ImGui::Separator();
                        DrawSectionHeader("Individual Effects", 6.0F, 6.0F);

                        if (ImGui::CollapsingHeader("Wetness"))
                        {
                            EffectsChanged |= ImGui::ColorEdit3("Tint##Wetness", glm::value_ptr(Effects.WetnessTint));
                            EffectsChanged |=
                                LabeledSliderFloat("Effect strength", &Effects.WetnessStrength, 0.0F, 1.0F, "%.2f");
                            EffectsChanged |=
                                LabeledSliderFloat("Wet roughness", &Effects.WetRoughness, 0.05F, 1.0F, "%.2f");
                            EffectsChanged |= LabeledSliderFloat(
                                "Specular strength", &Effects.WetnessSpecularStrength, 0.0F, 1.0F, "%.2f");
                        }

                        ImGui::Spacing();
                        if (ImGui::CollapsingHeader("WaterFilm"))
                        {
                            EffectsChanged |=
                                ImGui::Checkbox("Enable layer##WaterFilm", &Effects.bWaterFilmDisplacement);
                            EffectsChanged |=
                                ImGui::Checkbox("Visual smoothing##WaterFilm", &Effects.bWaterFilmSmoothing);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("렌더링용 WaterFilm 커버리지와 변위를 부드럽게 처리합니다. "
                                                      "끄면 공통 높이 필드 스무딩이 활성화된 경우를 제외하고 레이어별 "
                                                      "스무딩을 모두 건너뜁니다. "
                                                      "시뮬레이션 State는 바뀌지 않습니다.");
                            EffectsChanged |=
                                ImGui::ColorEdit3("Tint##WaterFilm", glm::value_ptr(Effects.WaterFilmTint));
                            EffectsChanged |=
                                LabeledSliderFloat("Opacity", &Effects.WaterFilmOpacity, 0.0F, 1.0F, "%.2f");
                            EffectsChanged |=
                                LabeledSliderFloat("Roughness", &Effects.WaterFilmRoughness, 0.05F, 1.0F, "%.2f");
                        }

                        ImGui::Spacing();
                        if (ImGui::CollapsingHeader("Mud"))
                        {
                            EffectsChanged |= ImGui::Checkbox("Enable layer##Mud", &Effects.bMudDisplacement);
                            EffectsChanged |= ImGui::Checkbox("Visual smoothing##Mud", &Effects.bMudSmoothing);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("렌더링용 Mud 커버리지와 변위를 부드럽게 처리합니다. "
                                                      "끄면 공통 높이 필드 스무딩이 활성화된 경우를 제외하고 레이어별 "
                                                      "스무딩을 모두 건너뜁니다. "
                                                      "시뮬레이션 State는 바뀌지 않습니다.");
                            EffectsChanged |=
                                LabeledSliderFloat("Mud roughness", &Effects.MudRoughness, 0.05F, 1.0F, "%.2f");
                        }
                        ImGui::Spacing();
                        if (ImGui::CollapsingHeader("Lava"))
                        {
                            EffectsChanged |= ImGui::Checkbox("Enable layer##Lava", &Effects.bLavaDisplacement);
                            EffectsChanged |= ImGui::Checkbox("Visual smoothing##Lava", &Effects.bLavaSmoothing);
                            if (ImGui::IsItemHovered())
                                SetDescriptionTooltip("렌더링용 Lava 커버리지와 변위를 부드럽게 처리합니다. "
                                                      "끄면 공통 높이 필드 스무딩이 활성화된 경우를 제외하고 레이어별 "
                                                      "스무딩을 모두 건너뜁니다. "
                                                      "시뮬레이션 State는 바뀌지 않습니다.");
                        }
                        if (EffectsChanged)
                            FrameRenderer->SetDemoSurfaceEffectSettings(Effects);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void TDebugUI::ResetProfilingAverages() noexcept
    {
        ProfilingWindowElapsed = ProfilingFpsSum = ProfilingFrameTimeSum = 0.0;
        ProfilingFrameSamples = 0;
        ProfilingMetricSums.fill(0.0);
        ProfilingMetricSamples.fill(0);
        ProfilingAverages.fill(-1.0F);
        ProfilingMaximums.fill(-1.0F);
        ProfilingWindowMaximums.fill(-1.0F);
        ProfilingRecent100msAverages.fill(-1.0F);
        ProfilingRecentSamples.clear();
        ProfilingElapsedSeconds = 0.0;
        bProfilingAverageAvailable = false;
    }

    void TDebugUI::DrawTotalHeightCacheOverlay()
    {
        if (FrameRenderer == nullptr ||
            FrameRenderer->GetRenderViewMode() != Rendering::TRenderViewMode::TotalSimulationHeight ||
            !FrameRenderer->WasTotalHeightCacheHit())
            return;

        const ImVec2    DisplaySize = ImGui::GetIO().DisplaySize;
        const glm::vec4 Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F || Rect.z <= 0.0F || Rect.w <= 0.0F)
            return;
        const ImVec2          Origin = ImGui::GetMainViewport()->Pos;
        constexpr const char* Title = "Total Height Cache: HIT";
        constexpr const char* Detail = "Last frame | all channels | height pass skipped";
        const ImVec2          TitleSize = ImGui::CalcTextSize(Title);
        const ImVec2          DetailSize = ImGui::CalcTextSize(Detail);
        const float           Width = std::max(TitleSize.x, DetailSize.x) + 24.0F;
        const float           Height = TitleSize.y + DetailSize.y + 20.0F;
        const float           Right = Origin.x + (Rect.x + Rect.z) * DisplaySize.x - 10.0F;
        const float           Top = Origin.y + Rect.y * DisplaySize.y + 10.0F;
        if (Width + 20.0F > Rect.z * DisplaySize.x || Height + 20.0F > Rect.w * DisplaySize.y)
            return;
        ImDrawList* DrawList = ImGui::GetForegroundDrawList();
        DrawList->AddRectFilled({Right - Width, Top}, {Right, Top + Height}, IM_COL32(18, 31, 30, 220), 5.0F);
        DrawList->AddRect({Right - Width, Top}, {Right, Top + Height}, IM_COL32(80, 190, 139, 220), 5.0F);
        DrawList->AddText({Right - Width + 12.0F, Top + 6.0F}, IM_COL32(153, 244, 181, 255), Title);
        DrawList->AddText({Right - Width + 12.0F, Top + 10.0F + TitleSize.y}, IM_COL32(212, 228, 221, 255), Detail);
    }

    void TDebugUI::DrawViewportStatsOverlay()
    {
        if (FrameRenderer == nullptr || !bViewportOverlaysVisible)
        {
            return;
        }
        ImGuiDockNode* ViewportNode = DockspaceID != 0 ? ImGui::DockBuilderGetCentralNode(DockspaceID) : nullptr;
        if (ViewportNode == nullptr)
        {
            return;
        }

        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const glm::vec4      Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + Rect.x * DisplaySize.x,
                                    MainViewport->Pos.y + Rect.y * DisplaySize.y};
        const ImVec2         ViewportSize{Rect.z * DisplaySize.x, Rect.w * DisplaySize.y};
        if (ViewportSize.x < 340.0F || ViewportSize.y < 240.0F)
            return;
        constexpr float            OverlayWindowWidth = 320.0F;
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        float RenderStateY = ViewportOrigin.y + 10.0F;
        ImGui::SetNextWindowPos({ViewportOrigin.x + 10.0F, ViewportOrigin.y + 10.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({OverlayWindowWidth, 0.0F}, {OverlayWindowWidth, 800.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0F, 7.0F});
        if (ImGui::Begin("Profiling##Overlay", nullptr, Flags))
        {
            if (bProfiledRawFluxCacheEnabled != FrameRenderer->IsRawFluxCacheEnabled())
            {
                ResetProfilingAverages();
                bProfiledRawFluxCacheEnabled = FrameRenderer->IsRawFluxCacheEnabled();
            }
            const ImGuiIO&                            IO = ImGui::GetIO();
            const double                              FrameSeconds = static_cast<double>(IO.DeltaTime);
            const Rendering::TRendererProfilingStats& Stats = FrameRenderer->GetProfilingStats();
            const bool  bFineRenderTimings = FrameRenderer->AreRenderPassSubstageTimingsReliable();
            const float FeedbackGpuMilliseconds =
                Stats.AccumulationGeometryGpuMilliseconds >= 0.0F && Stats.TransferWeightGpuMilliseconds >= 0.0F
                    ? Stats.AccumulationGeometryGpuMilliseconds + Stats.TransferWeightGpuMilliseconds
                    : -1.0F;
            const auto SolverMetric = [&](float Milliseconds)
            {
                if (!bSolverMetricsPerStep)
                    return Milliseconds;
                if (Milliseconds < 0.0F || Stats.SimulationSteps == 0U)
                    return -1.0F;
                return Milliseconds / static_cast<float>(Stats.SimulationSteps);
            };
            const std::array<float, 40> MetricValues = {
                Stats.RenderPreparationGpuMilliseconds + Stats.SceneDrawGpuMilliseconds +
                    (bFineRenderTimings ? Stats.UIDrawGpuMilliseconds + Stats.RenderPassEndGpuMilliseconds : 0.0F),
                SolverMetric(Stats.SolverGpuMilliseconds),
                SolverMetric(FeedbackGpuMilliseconds),
                SolverMetric(Stats.SolverPass1GpuMilliseconds),
                SolverMetric(Stats.SolverPass2GpuMilliseconds),
                Stats.RenderPreparationGpuMilliseconds,
                Stats.SceneDrawGpuMilliseconds,
                Stats.TexelInspectorGpuMilliseconds,
                Stats.FrameFenceWaitCpuMilliseconds,
                Stats.AcquireCpuMilliseconds,
                Stats.CommandRecordCpuMilliseconds,
                Stats.QueueSubmitCpuMilliseconds,
                Stats.PresentCpuMilliseconds,
                Stats.BaseMeshDrawGpuMilliseconds,
                Stats.MudOverlayDrawGpuMilliseconds,
                Stats.WaterFilmOverlayDrawGpuMilliseconds,
                Stats.OverlayPreparationGpuMilliseconds,
                Stats.OverlayGeometryGpuMilliseconds,
                Stats.OverlaySmoothingGpuMilliseconds,
                Stats.OverlaySidesGpuMilliseconds,
                Stats.SceneSetupGpuMilliseconds,
                Stats.SceneBetweenDrawsGpuMilliseconds,
                Stats.SceneTailGpuMilliseconds,
                Stats.OverlaySidesPreBarrierGpuMilliseconds,
                Stats.OverlaySidesDispatchGpuMilliseconds,
                Stats.OverlaySidesPostBarrierGpuMilliseconds,
                Stats.RenderPassBeginGpuMilliseconds,
                Stats.ViewportSetupGpuMilliseconds,
                Stats.OverlayBoundarySearchGpuMilliseconds,
                Stats.OverlayTopCommandGpuMilliseconds,
                Stats.OverlaySidesInterPassBarrierGpuMilliseconds,
                Stats.OverlayCoverageSampleGpuMilliseconds,
                Stats.RenderPassColorStageGpuMilliseconds,
                Stats.RenderPassDepthStageGpuMilliseconds,
                Stats.UIDrawGpuMilliseconds,
                Stats.RenderPassEndGpuMilliseconds,
                SolverMetric(Stats.AccumulationGeometryGpuMilliseconds),
                SolverMetric(Stats.TransferWeightGpuMilliseconds),
                Stats.OverlayHeightGpuMilliseconds,
                Stats.OverlayNormalVertexGpuMilliseconds,
            };
            // GPU timestamps arrive after the frame fence. Do not mix samples from the previous draw mode.
            if (std::isfinite(FrameSeconds) && FrameSeconds > 0.0 &&
                Stats.OverlayDrawMode == FrameRenderer->GetOverlayDrawProfilingMode())
            {
                ProfilingElapsedSeconds += FrameSeconds;
                ProfilingWindowElapsed += FrameSeconds;
                ProfilingFpsSum += 1.0 / FrameSeconds;
                ProfilingFrameTimeSum += FrameSeconds * 1000.0;
                ++ProfilingFrameSamples;
                TProfilingSample Sample;
                Sample.TimeSeconds = ProfilingElapsedSeconds;
                Sample.Values.fill(-1.0F);
                Sample.Values[0] = static_cast<float>(1.0 / FrameSeconds);
                Sample.Values[1] = static_cast<float>(FrameSeconds * 1000.0);
                ProfilingWindowMaximums[0] = std::max(ProfilingWindowMaximums[0], Sample.Values[0]);
                ProfilingWindowMaximums[1] = std::max(ProfilingWindowMaximums[1], Sample.Values[1]);
                for (std::size_t Index = 0; Index < MetricValues.size(); ++Index)
                {
                    if (std::isfinite(MetricValues[Index]) && MetricValues[Index] >= 0.0F)
                    {
                        ProfilingMetricSums[Index] += MetricValues[Index];
                        ++ProfilingMetricSamples[Index];
                        ProfilingWindowMaximums[Index + 2U] =
                            std::max(ProfilingWindowMaximums[Index + 2U], MetricValues[Index]);
                        Sample.Values[Index + 2U] = MetricValues[Index];
                    }
                }
                ProfilingRecentSamples.push_back(Sample);
                while (!ProfilingRecentSamples.empty() &&
                       ProfilingRecentSamples.front().TimeSeconds < ProfilingElapsedSeconds - 0.1)
                    ProfilingRecentSamples.pop_front();
                std::array<double, 43>        RecentSums{};
                std::array<std::uint32_t, 43> RecentCounts{};
                for (const TProfilingSample& RecentSample : ProfilingRecentSamples)
                {
                    for (std::size_t Index = 0; Index < RecentSample.Values.size(); ++Index)
                    {
                        const float Value = RecentSample.Values[Index];
                        if (std::isfinite(Value) && Value >= 0.0F)
                        {
                            RecentSums[Index] += Value;
                            ++RecentCounts[Index];
                        }
                    }
                }
                ProfilingRecent100msAverages.fill(-1.0F);
                for (std::size_t Index = 0; Index < ProfilingRecent100msAverages.size(); ++Index)
                {
                    if (RecentCounts[Index] != 0U)
                        ProfilingRecent100msAverages[Index] =
                            static_cast<float>(RecentSums[Index] / RecentCounts[Index]);
                }
            }
            if (ProfilingWindowElapsed >= 1.0 && ProfilingFrameSamples > 0)
            {
                ProfilingAverages[0] = static_cast<float>(ProfilingFpsSum / ProfilingFrameSamples);
                ProfilingAverages[1] = static_cast<float>(ProfilingFrameTimeSum / ProfilingFrameSamples);
                for (std::size_t Index = 0; Index < MetricValues.size(); ++Index)
                {
                    ProfilingAverages[Index + 2] =
                        ProfilingMetricSamples[Index] > 0
                            ? static_cast<float>(ProfilingMetricSums[Index] / ProfilingMetricSamples[Index])
                            : -1.0F;
                }
                ProfilingMaximums = ProfilingWindowMaximums;
                bProfilingAverageAvailable = true;
                ProfilingWindowElapsed = 0.0;
                ProfilingFpsSum = 0.0;
                ProfilingFrameTimeSum = 0.0;
                ProfilingFrameSamples = 0;
                ProfilingMetricSums.fill(0.0);
                ProfilingMetricSamples.fill(0U);
                ProfilingWindowMaximums.fill(-1.0F);
            }

            if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const float  FPS = ProfilingAverages[0];
                const ImVec4 FPSColor = FPS >= 55.0F   ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F)
                                        : FPS >= 30.0F ? ImVec4(1.0F, 0.84F, 0.47F, 1.0F)
                                                       : ImVec4(1.0F, 0.57F, 0.57F, 1.0F);
                if (bProfilingAverageAvailable)
                    ImGui::TextColored(FPSColor, "FPS: %.1f", FPS);
                else
                    ImGui::TextDisabled("FPS: collecting 1s average");
                int OverlayDrawMode = static_cast<int>(FrameRenderer->GetOverlayDrawProfilingMode());
                ImGui::TextDisabled("Draw test");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(125.0F);
                if (ImGui::Combo("##OverlayDrawTest", &OverlayDrawMode, "Both\0Top only\0Sides only\0"))
                {
                    FrameRenderer->SetOverlayDrawProfilingMode(
                        static_cast<Rendering::TOverlayDrawProfilingMode>(OverlayDrawMode));
                    ResetProfilingAverages();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("오버레이 그리기 호출만 바뀌며 렌더 준비는 계속 수행됩니다. "
                                      "각 모드에서 새 평균을 수집한 뒤 Render 패스를 비교하세요.");
                int OccupancyTile =
                    FrameRenderer->GetOverlayOccupancyTileSize() == Rendering::TOverlayOccupancyTileSize::Tile32 ? 1
                                                                                                                 : 0;
                ImGui::TextDisabled("Active tile");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(125.0F);
                if (ImGui::Combo("##OverlayOccupancyTile",
                                 &OccupancyTile,
                                 "16 x 16\0"
                                 "32 x 32\0"))
                {
                    bFrameTimeResetRequested = true;
                    FrameRenderer->SetOverlayOccupancyTileSize(OccupancyTile == 1
                                                                   ? Rendering::TOverlayOccupancyTileSize::Tile32
                                                                   : Rendering::TOverlayOccupancyTileSize::Tile16);
                    ResetProfilingAverages();
                }
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("형상 준비 전에 State 텍셀을 활성 타일 단위로 묶습니다.");
                const auto SetupMetricTable = [this](const char* Id)
                {
                    const int ColumnCount = 1 + static_cast<int>(bShowProfilingAverage) +
                                            static_cast<int>(bShowProfilingMaximum) +
                                            static_cast<int>(bShowProfilingPast100ms);
                    if (!ImGui::BeginTable(
                            Id, ColumnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
                        return false;
                    ImGui::TableSetupColumn("Metric", ImGuiTableColumnFlags_WidthFixed, 134.0F);
                    if (bShowProfilingAverage)
                        ImGui::TableSetupColumn("Avg", ImGuiTableColumnFlags_WidthFixed, 44.0F);
                    if (bShowProfilingMaximum)
                        ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 44.0F);
                    if (bShowProfilingPast100ms)
                        ImGui::TableSetupColumn("100ms", ImGuiTableColumnFlags_WidthFixed, 50.0F);
                    return true;
                };
                const auto DrawMetric =
                    [this](std::size_t Index, const char* Label, ImVec4 Color, const char* Tooltip = nullptr)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", Label);
                    if (Tooltip != nullptr && ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", Tooltip);
                    int        Column = 1;
                    const auto DrawValue = [&](float Value, bool bReady)
                    {
                        ImGui::TableSetColumnIndex(Column++);
                        if (Value >= 0.0F)
                            ImGui::TextColored(Color, "%.2f", Value);
                        else
                            ImGui::TextDisabled(bReady ? "—" : "…");
                    };
                    if (bShowProfilingAverage)
                        DrawValue(ProfilingAverages[Index + 2], bProfilingAverageAvailable);
                    if (bShowProfilingMaximum)
                        DrawValue(ProfilingMaximums[Index + 2], true);
                    if (bShowProfilingPast100ms)
                        DrawValue(ProfilingRecent100msAverages[Index + 2], true);
                };
                if (SetupMetricTable("ViewportProfilingSummary"))
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Metric");
                    if (bShowProfilingAverage)
                    {
                        ImGui::TableSetColumnIndex(ImGui::TableGetColumnIndex() + 1);
                        ImGui::TextDisabled("Avg ms");
                    }
                    if (bShowProfilingMaximum)
                    {
                        const int Column = 1 + static_cast<int>(bShowProfilingAverage);
                        ImGui::TableSetColumnIndex(Column);
                        ImGui::TextDisabled("Max ms");
                    }
                    if (bShowProfilingPast100ms)
                    {
                        const int Column =
                            1 + static_cast<int>(bShowProfilingAverage) + static_cast<int>(bShowProfilingMaximum);
                        ImGui::TableSetColumnIndex(Column);
                        ImGui::TextDisabled("100ms");
                    }
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("Frame time");
                    int        FrameColumn = 1;
                    const auto DrawFrameValue = [&](float Value, bool bReady)
                    {
                        ImGui::TableSetColumnIndex(FrameColumn++);
                        if (Value >= 0.0F)
                            ImGui::TextColored(FPSColor, "%.2f", Value);
                        else
                            ImGui::TextDisabled(bReady ? "—" : "…");
                    };
                    if (bShowProfilingAverage)
                        DrawFrameValue(ProfilingAverages[1], bProfilingAverageAvailable);
                    if (bShowProfilingMaximum)
                        DrawFrameValue(ProfilingMaximums[1], true);
                    if (bShowProfilingPast100ms)
                        DrawFrameValue(ProfilingRecent100msAverages[1], true);
                    DrawMetric(0, "Render GPU", {0.82F, 0.87F, 0.94F, 1.0F});
                    DrawMetric(1,
                               bSolverMetricsPerStep ? "Solver GPU / step" : "Solver GPU / frame",
                               {0.82F, 0.87F, 0.94F, 1.0F});
                    ImGui::EndTable();
                }
                ImGui::Separator();

                ImGui::PushStyleColor(ImGuiCol_Header, {0.0F, 0.0F, 0.0F, 0.0F});
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.30F, 0.32F, 0.35F, 0.35F});
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0.0F, 0.0F, 0.0F, 0.0F});
                if (bShowDetailedProfiling && BeginTextTreeNode("GPU: Simulation"))
                {
                    if (SetupMetricTable("ViewportProfilingSimulation"))
                    {
                        DrawMetric(2,
                                   bSolverMetricsPerStep ? "Geometry feedback / step" : "Geometry feedback / frame",
                                   {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(
                            3, bSolverMetricsPerStep ? "Pass 1 / step" : "Pass 1 / frame", {0.72F, 0.78F, 0.87F, 1.0F});
                        DrawMetric(
                            4, bSolverMetricsPerStep ? "Pass 2 / step" : "Pass 2 / frame", {0.72F, 0.78F, 0.87F, 1.0F});
                        DrawMetric(36,
                                   bSolverMetricsPerStep ? "Geometry build / step" : "Geometry build / frame",
                                   {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(37,
                                   bSolverMetricsPerStep ? "Transfer weights / step" : "Transfer weights / frame",
                                   {0.82F, 0.87F, 0.94F, 1.0F});
                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("WORKLOAD & SETTINGS");
                    if (ImGui::BeginTable("ViewportProfilingWorkload",
                                          2,
                                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
                    {
                        ImGui::TableSetupColumn("Setting", ImGuiTableColumnFlags_WidthFixed, 130.0F);
                        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 165.0F);
                        const auto AddStateRow = [](const char* Label, const char* Format, auto... Values)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextDisabled("%s", Label);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextDisabled(Format, Values...);
                        };
                        AddStateRow("Sim Steps", "%u", Stats.SimulationSteps);
                        AddStateRow("Simulated this frame", "%.2f ms", Stats.SimulatedThisFrameSeconds * 1000.0F);
                        AddStateRow("Pending simulation", "%.2f ms", Stats.PendingSimulationSeconds * 1000.0F);
                        AddStateRow("Time skipped", "%.2f ms", Stats.DroppedSimulationSeconds * 1000.0F);
                        AddStateRow("Instances", "%u", Stats.SimulationInstances);
                        AddStateRow("Texels / Ch.",
                                    "%llu / %u",
                                    static_cast<unsigned long long>(Stats.SimulationTexels),
                                    Stats.StateChannels);
                        AddStateRow("Resolution", "%u x %u", Stats.SimulationResolution, Stats.SimulationResolution);
                        AddStateRow("RawFlux Cache", "%s", FrameRenderer->IsRawFluxCacheEnabled() ? "ON" : "OFF");
                        AddStateRow(
                            "Geometry Feedback", "%s", FrameRenderer->IsAccumulationFeedbackEnabled() ? "ON" : "OFF");
                        AddStateRow("Fixed timestep", "%s", bFixedSimulationTimestep ? "ON" : "OFF");
                        AddStateRow("Auto substepping", "%s", bAutoSubstepping ? "ON" : "OFF");
                        AddStateRow("Time scale", "%.2fx", SimulationTimeScale);
                        const auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
                        AddStateRow("Height Smoothing", "%s", Effects.bHeightFieldSmoothing ? "ON" : "OFF");
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                if (bShowDetailedProfiling && BeginTextTreeNode("GPU: Rendering"))
                {
                    if (Stats.OverlayTotalTopTriangles > 0 &&
                        Stats.OverlayDrawMode == FrameRenderer->GetOverlayDrawProfilingMode())
                    {
                        const double ActivePercent = 100.0 * static_cast<double>(Stats.OverlayActiveTopTriangles) /
                                                     static_cast<double>(Stats.OverlayTotalTopTriangles);
                        ImGui::TextDisabled("Active top triangles: %.1f%%", ActivePercent);
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip(
                                "정점 하나 이상이 2%% 커버리지 기준을 넘으면 활성 삼각형으로 계산합니다. "
                                "활성화된 레이어는 각각 집계하며, 비율은 면적 가중치가 적용되지 않습니다.");
                        ImGui::TextDisabled("%llu / %llu triangles",
                                            static_cast<unsigned long long>(Stats.OverlayActiveTopTriangles),
                                            static_cast<unsigned long long>(Stats.OverlayTotalTopTriangles));
                    }
                    if (SetupMetricTable("ViewportProfilingRendering"))
                    {
                        DrawMetric(5, "Render prep", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(6, bFineRenderTimings ? "Scene draw" : "Render pass", {0.82F, 0.87F, 0.94F, 1.0F});
                        if (bFineRenderTimings)
                        {
                            DrawMetric(34, "UI draw", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(35, "Render pass end", {0.82F, 0.87F, 0.94F, 1.0F});
                        }
                        DrawMetric(7, "Texel inspector", {0.82F, 0.87F, 0.94F, 1.0F});
                        ImGui::EndTable();
                    }
                    if (bShowDetailedProfiling && BeginTextTreeNode("Render prep breakdown"))
                    {
                        if (SetupMetricTable("ViewportProfilingRenderPrepBreakdown"))
                        {
                            DrawMetric(16, "Overlay prep", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(17, "Overlay geometry", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(38,
                                       "  Height pass",
                                       {0.82F, 0.87F, 0.94F, 1.0F},
                                       "Height dispatch and its dependency barrier.");
                            DrawMetric(39,
                                       "  Normal + vertex pass",
                                       {0.82F, 0.87F, 0.94F, 1.0F},
                                       "Normal/vertex dispatch and its output barrier.");
                            DrawMetric(18, "Height smoothing", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(19, "Overlay sides", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(23, "  Pre-write barrier", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(24, "  Total dispatches", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(31, "    Coverage sample", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(28, "    Boundary + segment", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(29, "    Top draw command", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(30, "    Coverage barrier", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(25,
                                       "  Pre-draw sync",
                                       {0.82F, 0.87F, 0.94F, 1.0F},
                                       "Includes activity counter readback and draw barriers.");
                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                    if (!bFineRenderTimings)
                    {
                        TextDescriptionWrapped("MoltenVK on Apple GPUs records in-pass timestamps at the end "
                                               "of the render pass. Per-draw GPU timings are unavailable here.");
                    }
                    if (bFineRenderTimings && bShowDetailedProfiling && BeginTextTreeNode("Scene draw breakdown"))
                    {
                        if (SetupMetricTable("ViewportProfilingSceneDrawBreakdown"))
                        {
                            DrawMetric(20, "Render pass setup", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(26,
                                       "  Pass begin total",
                                       {0.72F, 0.78F, 0.87F, 1.0F},
                                       "Time around render-pass begin; may include image availability waits.");
                            DrawMetric(32,
                                       "    Color stage",
                                       {0.72F, 0.78F, 0.87F, 1.0F},
                                       "Color-output timestamps around pass begin. This overlaps the total.");
                            DrawMetric(33,
                                       "    Depth stage",
                                       {0.72F, 0.78F, 0.87F, 1.0F},
                                       "Early-depth timestamps around pass begin. This overlaps the total.");
                            DrawMetric(27, "  Viewport state setup", {0.72F, 0.78F, 0.87F, 1.0F});
                            DrawMetric(13, "Base mesh draw", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(14, "Mud overlay draw", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(15, "WaterFilm draw", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(21, "Between draws", {0.82F, 0.87F, 0.94F, 1.0F});
                            DrawMetric(22, "Grid / gizmo", {0.82F, 0.87F, 0.94F, 1.0F});
                            ImGui::EndTable();
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
                if (bShowDetailedProfiling && BeginTextTreeNode("CPU"))
                {
                    if (SetupMetricTable("ViewportProfilingCPU"))
                    {
                        DrawMetric(8, "Fence wait", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(9, "Acquire", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(10, "Command record", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(11, "Queue submit", {0.82F, 0.87F, 0.94F, 1.0F});
                        DrawMetric(12, "Present call", {0.82F, 0.87F, 0.94F, 1.0F});
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
                ImGui::PopStyleColor(3);
            }
            RenderStateY = ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + 6.0F;
        }
        const ImVec2 ProfilingPos = ImGui::GetWindowPos();
        const ImVec2 ProfilingSize = ImGui::GetWindowSize();
        const ImVec2 Mouse = ImGui::GetIO().MousePos;
        bMouseOverViewportOverlay |= Mouse.x >= ProfilingPos.x && Mouse.y >= ProfilingPos.y &&
                                     Mouse.x < ProfilingPos.x + ProfilingSize.x &&
                                     Mouse.y < ProfilingPos.y + ProfilingSize.y;
        ImGui::End();
        ImGui::PopStyleVar(3);

        const auto Effects = FrameRenderer->GetDemoSurfaceEffectSettings();
        const bool bLitView = FrameRenderer->GetRenderViewMode() == Rendering::TRenderViewMode::Lit;
        const bool bEffectsActive = bLitView && Effects.bEnabled;
        const auto StatusText = [](bool bEnabled, bool bActive)
        {
            if (!bActive)
                return bEnabled ? "ON · inactive" : "OFF · inactive";
            return bEnabled ? "ON" : "OFF";
        };
        const auto StatusColor = [](bool bEnabled, bool bActive)
        {
            if (!bActive)
                return ImVec4(0.62F, 0.64F, 0.68F, 1.0F);
            return bEnabled ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F) : ImVec4(0.70F, 0.72F, 0.75F, 1.0F);
        };
        ImGui::SetNextWindowPos({ViewportOrigin.x + 10.0F, RenderStateY}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({OverlayWindowWidth, 0.0F}, {OverlayWindowWidth, 800.0F});
        ImGui::SetNextWindowBgAlpha(0.84F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10.0F, 7.0F});
        constexpr ImGuiWindowFlags StateFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus;
        if (ImGui::Begin("Render State##Overlay", nullptr, StateFlags) &&
            ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::BeginTable(
                    "RenderStateFlags", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
            {
                ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed, 128.0F);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 112.0F);
                const auto DrawState = [&](const char* Label, bool bEnabled, bool bActive)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", Label);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(StatusColor(bEnabled, bActive), "%s", StatusText(bEnabled, bActive));
                };
                DrawState("Lit effects", Effects.bEnabled, bLitView);
                DrawState("Mud layer", Effects.bMudDisplacement, bEffectsActive);
                DrawState("WaterFilm layer", Effects.bWaterFilmDisplacement, bEffectsActive);
                DrawState("Lava layer", Effects.bLavaDisplacement, bEffectsActive);
                const bool bSmoothingActive =
                    bEffectsActive &&
                    (Effects.bMudDisplacement || Effects.bWaterFilmDisplacement || Effects.bLavaDisplacement);
                DrawState("Height-field smoothing", Effects.bHeightFieldSmoothing, bSmoothingActive);
                ImGui::EndTable();
            }
        }
        const ImVec2 StatePos = ImGui::GetWindowPos();
        const ImVec2 StateSize = ImGui::GetWindowSize();
        bMouseOverViewportOverlay |= Mouse.x >= StatePos.x && Mouse.y >= StatePos.y &&
                                     Mouse.x < StatePos.x + StateSize.x && Mouse.y < StatePos.y + StateSize.y;
        ImGui::End();
        ImGui::PopStyleVar(3);
    }

    void TDebugUI::DrawSimulationDebugWindow(TScene& SceneData)
    {
        if (AssetManager == nullptr || FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiViewport*  Viewport = ImGui::GetMainViewport();
        constexpr float Width = 310.0F;
        const ImVec2    WindowSize{Width, 520.0F};
        const ImVec2    WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - Width - 10.0F,
                                    Viewport->WorkPos.y + 150.0F};
        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin("Simulation Debug", nullptr, Flags))
        {
            ImGui::TextUnformatted("Simulation");
            DrawSimulationCommonControls();
            ImGui::Dummy({0.0F, 10.0F});
            ImGui::Separator();
            ImGui::Dummy({0.0F, 10.0F});

            // 본문 배경은 부모 패널을 그대로 사용하고, 각 탭의 본문만 스크롤한다.
            if (ImGui::BeginTabBar("SimulationTabs"))
            {
                if (ImGui::BeginTabItem("Solver"))
                {
                    if (ImGui::BeginChild(
                            "SolverContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawSolverTab();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Contact Input"))
                {
                    if (ImGui::BeginChild(
                            "ContactInputContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawContactInputTab();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Profile Tuning"))
                {
                    if (ImGui::BeginChild(
                            "ProfileTuningContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawProfileTuningTab(SceneData);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Inspector"))
                {
                    if (ImGui::BeginChild(
                            "InspectorContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawTexelInspectorTab();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Global Settings"))
                {
                    if (ImGui::BeginChild(
                            "GlobalSettingsContent", {0.0F, 0.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground))
                    {
                        DrawGlobalSettingsTab(SceneData);
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void TDebugUI::DrawDebugStateSelector()
    {
        const auto& Registry = SurfaceDataManager->GetSurfaceStateRegistry();
        if (Registry.GetStateCount() == 0)
        {
            ImGui::TextDisabled("등록된 State 없음");
            return;
        }
        DebugState = FrameRenderer->GetDebugStateChannel();
        ImGui::SetNextItemWidth(125.0F);
        if (ImGui::BeginCombo("##InspectorState", Registry.GetStateName(DebugState).c_str()))
        {
            for (std::size_t Index = 0; Index < Registry.GetStateCount(); ++Index)
            {
                const auto State = static_cast<SurfaceState::TStateId>(Index);
                if (ImGui::Selectable(Registry.GetStateName(State).c_str(), State == DebugState))
                {
                    DebugState = State;
                    FrameRenderer->SetDebugStateChannel(State);
                }
            }
            ImGui::EndCombo();
        }
    }

    void TDebugUI::DrawTexelInspectorTab()
    {
        DrawSectionHeader("Texel Inspector");
        TextDescriptionWrapped(
            "Shift + left click on the surface to select a texel. Pause / Step controls remain available above.");
        TextDescriptionWrapped("Picking uses the Macro mesh. Accumulation is a selected-State preview; it does not "
                               "update Solver geometry.");
        DrawDebugStateSelector();
        const auto& Selection = FrameRenderer->GetInspectedTexel();
        if (!Selection)
        {
            ImGui::TextDisabled("선택된 texel 없음");
            return;
        }
        if (ImGui::Button("Clear selection"))
        {
            FrameRenderer->ClearInspectedTexel();
            return;
        }
        ImGui::Text("Instance %zu / Surface %u", Selection->Instance, Selection->Surface);
        ImGui::Text("Texel %u (%u, %u)", Selection->Texel, Selection->XY.x, Selection->XY.y);
        ImGui::Text("Hit triangle %u", Selection->Triangle);
        ImGui::TextWrapped("Profile: %s", Selection->Profile.c_str());
        const auto& Snapshot = FrameRenderer->GetTexelSnapshot();
        if (!Snapshot)
        {
            ImGui::TextDisabled("GPU snapshot 대기 중");
            return;
        }
        const auto& S = *Snapshot;
        ImGui::Text("Completed sample / Step %llu / Buffer %s",
                    static_cast<unsigned long long>(S.Step),
                    S.bStateAB ? "A" : "B");
        const auto                           Status = static_cast<std::uint32_t>(S.Values[2].w);
        constexpr std::array<const char*, 5> StatusNames{
            "Invalid texel", "Unassigned Profile", "Unsupported State", "Valid", "Invalid value / area"};
        ImGui::TextColored(Status == 3 ? ImVec4{0.35F, 0.85F, 0.55F, 1} : ImVec4{1, 0.65F, 0.25F, 1},
                           "%s",
                           StatusNames[std::min(Status, 4U)]);
        if (Status != 3)
            return;
        TextDescriptionWrapped("Values are calculated on the GPU with the same formula used by the debug views. "
                               "Heights use mesh-local units and include the Lit height display scale.");
        if (ImGui::BeginTable("TexelValues", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 1.4F);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0F);
            const auto Row = [](const char* Label, float Value)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(Label);
                ImGui::TableNextColumn();
                ImGui::Text("%.7g", Value);
            };
            Row("Raw State (total)", S.Values[0].x);
            Row("Profile Capacity", S.Values[0].y / S.Values[4].y);
            Row("Texel Capacity", S.Values[0].y);
            Row("Saturation", S.Values[0].z);
            Row("Reference-area amount", S.Values[0].w);
            Row("World area", S.Values[4].x);
            Row("Area / reference area", S.Values[4].y);
            Row("Accumulation factor", S.Values[1].x);
            Row("Cavity fill factor", S.Values[1].y);
            Row("Meso height", S.Values[1].z);
            Row("Thickness per amount (world)", S.Values[1].w);
            Row("Lit height display scale", S.Values[4].z);
            Row("World-to-local height", S.Values[4].w);
            Row("Cavity depth", S.Values[2].x);
            Row("Cavity fill (0-1)", S.Values[2].y);
            Row("Cavity excess", S.Values[2].z);
            Row("Cavity height", S.Values[3].x);
            Row("Following height", S.Values[3].y);
            Row("Accumulation height", S.Values[3].z);
            Row("Final height", S.Values[3].w);
            ImGui::EndTable();
        }
        ImGui::Text("Final local normal (scale 1): %.4g, %.4g, %.4g", S.Values[5].x, S.Values[5].y, S.Values[5].z);
    }

    void TDebugUI::DrawSimulationCommonControls()
    {
        const float ControlColumnX = ImGui::GetCursorPosX() + LabeledControlColumnWidth;
        const auto  ContinueControlRow = [ControlColumnX](float NextItemWidth)
        {
            const float RightEdge = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            if (ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + NextItemWidth <= RightEdge)
            {
                ImGui::SameLine();
            }
            else
            {
                ImGui::SetCursorPosX(ControlColumnX);
            }
        };
        const auto RadioWidth = [](const char* Label)
        { return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(Label).x; };

        BeginLabeledControlRow("Playback");
        if (ImGui::RadioButton("Running", !bSimulationPaused))
        {
            bSimulationPaused = false;
        }
        ContinueControlRow(RadioWidth("Paused"));
        if (ImGui::RadioButton("Paused", bSimulationPaused))
        {
            bSimulationPaused = true;
        }

        BeginLabeledControlRow("Controls");
        ImGui::BeginDisabled(!bSimulationPaused);
        if (ImGui::Button("Step"))
        {
            bSolverStepRequested = true;
        }
        ImGui::EndDisabled();
        ContinueControlRow(ImGui::CalcTextSize("Reset State").x + 2.0F * ImGui::GetStyle().FramePadding.x);
        if (ImGui::Button("Reset State"))
        {
            bSolverResetRequested = true;
            bFrameTimeResetRequested = true;
        }

        BeginLabeledControlRow("Speed");
        constexpr std::array<std::pair<const char*, float>, 6> SpeedPresets = {
            {{"0.25x", 0.25F}, {"0.5x", 0.5F}, {"1x", 1.0F}, {"2x", 2.0F}, {"4x", 4.0F}, {"8x", 8.0F}}};
        bool bFirstPreset = true;
        for (const auto& [Label, Scale] : SpeedPresets)
        {
            if (!bFirstPreset)
            {
                ContinueControlRow(RadioWidth(Label));
            }
            if (ImGui::RadioButton(Label, std::abs(SimulationTimeScale - Scale) < 0.001F))
            {
                SimulationTimeScale = Scale;
            }
            bFirstPreset = false;
        }
        ContinueControlRow(160.0F);
        ImGui::SetNextItemWidth(-1.0F);
        SliderFloatWithDoubleClickInput("##Speed", &SimulationTimeScale, 0.05F, 8.0F, "%.2fx");
    }

    void TDebugUI::DrawGlobalSettingsTab(TScene& SceneData)
    {
        // Keep the first control aligned with the standard section-header top margin.
        ImGui::Dummy({0.0F, SectionHeaderTopPadding});
        const std::uint32_t CurrentResolution = FrameRenderer->GetSimulationResolution();
        const char*         CurrentLabel = "Medium";
        for (const auto& Preset : SurfaceState::SurfaceSimulationResolutionPresets)
            if (Preset.Resolution == CurrentResolution)
                CurrentLabel = Preset.Label;
        if (BeginLabeledCombo("Resolution", CurrentLabel))
        {
            for (const auto& Preset : SurfaceState::SurfaceSimulationResolutionPresets)
            {
                const bool Selected = Preset.Resolution == CurrentResolution;
                if (ImGui::Selectable(Preset.Label, Selected) && !Selected)
                {
                    bFrameTimeResetRequested = true;
                    try
                    {
                        FrameRenderer->SetSimulationResolution(SceneData, Preset.Resolution);
                        ResolutionStatus = "Resolution changed. State reset.";
                        ResetProfilingAverages();
                    }
                    catch (const std::exception& Error)
                    {
                        ResolutionStatus = std::string("Resolution change failed: ") + Error.what();
                        TLogger::Warning("TDebugUI", ResolutionStatus);
                    }
                }
                if (Selected)
                    ImGui::SetItemDefaultFocus();
            }
            EndLabeledCombo();
        }
        const std::uint32_t ActiveResolution = FrameRenderer->GetSimulationResolution();
        ImGui::TextDisabled("%u x %u per surface", ActiveResolution, ActiveResolution);
        TextDescriptionWrapped("Changing resolution resets State.");
        if (!ResolutionStatus.empty())
        {
            if (ResolutionStatus.starts_with("Resolution change failed:"))
            {
                ImGui::TextWrapped("%s", ResolutionStatus.c_str());
            }
            else
            {
                TextDescriptionWrapped(ResolutionStatus.c_str());
            }
        }

        if (ImGui::Checkbox("Fixed timestep", &bFixedSimulationTimestep))
        {
            bFrameTimeResetRequested = true;
            ResetProfilingAverages();
        }
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("경과 시간을 누적해 1/60초 고정 간격으로 처리합니다. Profile 배율은 이 간격을 바꾸지 "
                                  "않습니다. 자동 세부 단계 분할은 각 간격을 나눠 처리할 수 있습니다.");
        if (ImGui::Checkbox("Auto substepping", &bAutoSubstepping))
        {
            bFrameTimeResetRequested = true;
            ResetProfilingAverages();
        }
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip(
                "수송 한계를 기준으로 시뮬레이션 시간을 더 작은 Solver 단계로 나눕니다. GPU 작업량이 늘어납니다. 끄면 "
                "고정 시간 간격이 켜진 경우 1/60초 간격을 사용하고, 꺼진 경우 경과 시간을 사용합니다.");
        ImGui::TextDisabled("%u steps, %.3f ms maximum step",
                            FrameRenderer->GetLastSimulationStepCount(),
                            1000.0F * FrameRenderer->GetMaximumSimulationStep());
        ImGui::TextDisabled("Simulation %.2f s, pending %.3f s",
                            FrameRenderer->GetSimulatedSeconds(),
                            FrameRenderer->GetPendingSimulationSeconds());
        if (FrameRenderer->GetProfilingStats().DroppedSimulationSeconds > 0.0F)
            TextDescriptionWrapped("Long frames skip excess simulation time to keep controls responsive.");
    }

    void TDebugUI::DrawSolverTab()
    {
        const auto DrawSolverTerm =
            [this](SurfaceState::TSurfaceSolverTerm Term, const char* Label, const char* Description)
        {
            bool bEnabled = FrameRenderer->IsDebugSolverTermEnabled(Term);
            if (ImGui::Checkbox(Label, &bEnabled))
            {
                bFrameTimeResetRequested = true;
                FrameRenderer->SetDebugSolverTermEnabled(Term, bEnabled);
                ResetProfilingAverages();
            }
            if (ImGui::IsItemHovered())
            {
                SetDescriptionTooltip(Description);
            }
        };

        DrawSectionHeader("Geometry Feedback");
        bool bAccumulationFeedback = FrameRenderer->IsAccumulationFeedbackEnabled();
        if (ImGui::Checkbox("Accumulation feedback", &bAccumulationFeedback))
        {
            bFrameTimeResetRequested = true;
            FrameRenderer->SetAccumulationFeedbackEnabled(bAccumulationFeedback);
            ResetProfilingAverages();
        }
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip(
                "켜짐: 적층 높이가 있는 모든 State를 합산해 이후 수송의 위치·법선·거리 계산에 반영합니다.\n"
                "꺼짐: 기존 정적 지형으로 수송합니다. 시뮬레이션 두께는 .SRProfile의 thicknessPerAmount를 사용하며,\n"
                "렌더 옵션의 표시 배율은 렌더링 전용입니다. 켜짐 상태에서는 단계마다 GPU 작업이 추가됩니다.");
        }

        DrawSectionHeader("Transport");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::SaturationDrive,
                       "Saturation spreading",
                       "ON: 포화도 차이에 따른 이웃 전달을 적용합니다.\n"
                       "OFF: 포화도 차이 전달량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::GeometryDrive,
                       "Gravity-guided flow",
                       "ON: 높이와 중력 방향에 따른 전달에 출발 포화도를 곱합니다. 포화도는 1을 넘을 수 있습니다.\n"
                       "OFF: 기하 전달량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::MesoDirectionNormal,
                       "DirectionDrive: MesoNormal",
                       "ON: 중력을 표면에 투영할 때 복원된 MesoNormal을 사용합니다.\n"
                       "OFF: 중력을 표면에 투영할 때 기본 mesh normal을 사용합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::DistanceWeight,
                       "DistanceWeight",
                       "ON: 이웃 간 실제 표면 거리 기반 가중치를 적용합니다.\n"
                       "OFF: 거리 가중치를 1.0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::NormalWeight,
                       "NormalWeight",
                       "ON: 이웃 표면 법선의 방향 일치도 가중치를 적용합니다.\n"
                       "OFF: 법선 가중치를 1.0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::ProfileBoundaryWeight,
                       "ProfileBoundaryWeight",
                       "ON: 서로 다른 Profile 사이 전달 가중치를 0.5로 낮춥니다.\n"
                       "OFF: Profile 경계 가중치를 1.0으로 설정합니다.");

        DrawSectionHeader("Decay");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::Decay,
                       "Decay",
                       "ON: Profile DecayRate에 따른 State 감소를 적용합니다.\n"
                       "OFF: 감소량을 0으로 설정합니다.");
        DrawSolverTerm(SurfaceState::TSurfaceSolverTerm::ConcavityRetention,
                       "Cavity decay protection",
                       "ON: 오목도와 Profile 계수로 Decay 보유량을 조절합니다.\n"
                       "OFF: Decay 보유율을 1.0으로 설정합니다.");

        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Cache Comparison"))
        {
            bool bCacheEnabled = FrameRenderer->IsRawFluxCacheEnabled();
            if (ImGui::Checkbox("RawFlux Cache", &bCacheEnabled))
            {
                bFrameTimeResetRequested = true;
                FrameRenderer->SetRawFluxCacheEnabled(bCacheEnabled);
                ResetProfilingAverages();
            }
            ImGui::TextDisabled("%s", bCacheEnabled ? "ON: reuse Pass 1 flux" : "OFF: recompute in Pass 2");
            const auto       Memory = FrameRenderer->GetSurfaceGPUResources().GetRawFluxMemoryUsage();
            constexpr double MiB = 1024.0 * 1024.0;
            ImGui::Text("Cache buffers: %.2f MiB",
                        static_cast<double>(Memory.InstanceRawFluxBytes +
                                            Memory.SharedReverseNeighborDirectionIndexBytes) /
                        MiB);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("RawFlux (all instances): %.2f MiB",
                            static_cast<double>(Memory.InstanceRawFluxBytes) / MiB);
                ImGui::Text("Reverse neighbor direction indices (shared once): %.2f MiB",
                            static_cast<double>(Memory.SharedReverseNeighborDirectionIndexBytes) / MiB);
                ImGui::TextDisabled("Buffer sizes; excludes allocator overhead.");
                ImGui::EndTooltip();
            }
            TextDescriptionWrapped("State is preserved. Buffers stay allocated while OFF.");
            TextDescriptionWrapped("Fixed timestep and Auto substepping are available in the Global Settings tab.");
            TextDescriptionWrapped(
                "Compare with the same resolution, time scale and starting State. Reset and replay the "
                "same input for each mode; allow warmup before reading averages.");
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Diagnostics"))
        {
            const SurfaceState::TSurfaceGPUResourceManager& GPUResources = FrameRenderer->GetSurfaceGPUResources();
            std::size_t                                     TotalTexels = 0;
            std::size_t                                     ValidTexels = 0;
            std::optional<bool>                             FirstCurrentAB;
            bool                                            bMixedCurrentBuffers = false;
            for (std::size_t SceneIndex = 0; SceneIndex < GPUResources.GetSceneInstanceCount(); ++SceneIndex)
            {
                if (GPUResources.GetInstanceDescriptors(SceneIndex) == nullptr)
                {
                    continue;
                }
                TotalTexels += GPUResources.GetInstanceTexelCount(SceneIndex);
                ValidTexels += GPUResources.GetInstanceValidTexelCount(SceneIndex);
                const bool bCurrentAB = GPUResources.IsCurrentStateAB(SceneIndex);
                if (!FirstCurrentAB.has_value())
                {
                    FirstCurrentAB = bCurrentAB;
                }
                else if (*FirstCurrentAB != bCurrentAB)
                {
                    bMixedCurrentBuffers = true;
                }
            }
            const float ValidRatio =
                TotalTexels > 0 ? 100.0F * static_cast<float>(ValidTexels) / static_cast<float>(TotalTexels) : 0.0F;
            ImGui::Text("Texels: %zu", TotalTexels);
            ImGui::Text("Valid: %zu (%.1f%%)", ValidTexels, ValidRatio);
            const char* CurrentBuffer = bMixedCurrentBuffers          ? "Mixed"
                                        : !FirstCurrentAB.has_value() ? "N/A"
                                        : *FirstCurrentAB             ? "A"
                                                                      : "B";
            if (bSimulationPaused)
            {
                ImGui::Text("Next read buffer: %s", CurrentBuffer);
                const float SolverMilliseconds = FrameRenderer->GetLastSolverGpuMilliseconds();
                if (SolverMilliseconds >= 0.0F)
                {
                    ImGui::Text("Recent Solver GPU: %.2f ms", SolverMilliseconds);
                }
                else
                {
                    ImGui::TextDisabled("Recent Solver GPU: N/A");
                }
            }
        }
    }

    void TDebugUI::DrawContactInputTab()
    {
        DrawSectionHeader("Contact Input");
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
        BeginLabeledControlRow("Texel search");
        ImGui::SetNextItemWidth(-1.0F);
        SliderIntWithDoubleClickInput("##InjectTexelSearchRadius", &InjectTexelSearchRadius, 0, 16, "%d texels");
        if (ImGui::IsItemHovered())
        {
            SetDescriptionTooltip("같은 삼각형 안에서 UV 대체 검색을 수행할 축별 범위입니다. 기본값은 2이며, 0이면 "
                                  "대체 검색을 사용하지 않습니다.");
        }
    }

    void TDebugUI::DrawProfileTuningTab(TScene& SceneData)
    {
        DrawSectionHeader("Runtime Profile Parameters");
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
            SetDescriptionTooltip("더 오목하지 않은 이웃 방향으로 이동할 때 수송을 줄입니다. 오목한 정도가 같으면 "
                                  "흐름에 영향을 주지 않습니다.");
        DrawSectionHeader("Decay", 6.0F, 2.0F);
        bChanged |= LabeledDragFloat("Decay /s", &ParameterDraft.DecayRate, 0.01F, 0.0F, 100.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("기준 면적당 초당 감소량입니다. 각 텍셀의 실제 월드 면적에 비례해 적용됩니다.");
        bChanged |=
            LabeledSliderFloat("Cavity decay protection", &ParameterDraft.CavityRetentionFactor, 0.0F, 1.0F, "%.3f");
        if (ImGui::IsItemHovered())
            SetDescriptionTooltip("오목한 텍셀에서 State의 감소를 줄입니다. 수송 속도에는 직접 영향을 주지 않습니다.");
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

    void TDebugUI::DrawLogWindow()
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowSize({std::max(Viewport->WorkSize.x * 0.70F, 720.0F), LogWindowHeight},
                                 ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Log"))
        {
            const std::uint64_t CurrentRevision = TLogger::GetRevision();
            if (CurrentRevision != LastSeenLogRevision)
            {
                CachedLogEntries = TLogger::GetEntries();
                bScrollLogToBottom = true;
                LastSeenLogRevision = CurrentRevision;
            }

            if (ImGui::Button("Clear"))
            {
                TLogger::Clear();
                bScrollLogToBottom = true;
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(260.0F);
            ImGui::InputTextWithHint("##LogSearch", "Search logs...", LogSearch.data(), LogSearch.size());
            ImGui::SameLine();
            if (ImGui::Button("Copy Visible"))
            {
                std::string ClipboardText;
                for (const TLogEntry& Entry : CachedLogEntries)
                {
                    const std::size_t Index = static_cast<std::size_t>(Entry.Level);
                    if (Index < LogLevelFilters.size() && LogLevelFilters[Index] &&
                        ContainsCaseInsensitive(Entry.Formatted, LogSearch.data()))
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
                LogLevelFilters.fill(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("None"))
            {
                LogLevelFilters.fill(false);
            }

            ImGui::SameLine();
            ImGui::TextUnformatted("Filter:");

            for (const TLogLevel Level : DisplayedLevels)
            {
                ImGui::SameLine();
                const std::size_t Index = static_cast<std::size_t>(Level);
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Level));
                ImGui::Checkbox(TLogger::GetLevelName(Level), &LogLevelFilters[Index]);
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            ImGui::BeginChild(
                "LogMessages", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            for (const TLogEntry& Entry : CachedLogEntries)
            {
                const std::size_t Index = static_cast<std::size_t>(Entry.Level);
                if (Index >= LogLevelFilters.size() || !LogLevelFilters[Index])
                {
                    continue;
                }
                if (!ContainsCaseInsensitive(Entry.Formatted, LogSearch.data()))
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

            if (bScrollLogToBottom)
            {
                ImGui::SetScrollHereY(1.0F);
                bScrollLogToBottom = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
} // MDSS 네임스페이스
