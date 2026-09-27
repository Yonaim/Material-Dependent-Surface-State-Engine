/**
 * @file DebugUI.cpp
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#include "DebugUI/DebugUI.h"

#include "Application/SceneFileDialog.h"
#include "Application/Window.h"
#include "AssetManager/Core/AssetManager.h"
#include "AssetManager/Loaders/SceneLoader.h"
#include "InputSystem/Raycaster.h"
#include "Renderer/Renderer.h"
#include "Renderer/Swapchain.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "VulkanContext/Vulkan/VulkanQueue.h"
#include "VulkanContext/VulkanContext.h"

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
#include <glm/gtc/matrix_inverse.hpp>
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
                    return {0.45F, 0.78F, 1.00F, 1.0F}; // 연한 파랑
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

        constexpr std::array<const char*, 5> RenderViewModeNames = {
            "Lit",
            "Unlit",
            "Vertex Normal (World Space)",
            "Normal Texture (Tangent Space)",
            "Mapped Normal (World Space)",
        };

        constexpr std::array<const char*, 5> SurfaceDebugViewNames = {
            "State Heatmap",
            "Validity",
            "Surface ID",
            "Neighbor Count",
            "UV Seam",
        };

        constexpr std::array<const char*, 4> SolverTransferWeightViewNames = {
            "Combined TransferWeight",
            "DistanceWeight",
            "NormalWeight",
            "ProfileBoundaryWeight",
        };

        void DrawLegendColor(ImVec4 Color, const char* Label)
        {
            ImGui::PushID(Label);
            ImGui::ColorButton("##Color", Color, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
                               {12.0F, 12.0F});
            ImGui::SameLine(0.0F, 4.0F);
            ImGui::TextUnformatted(Label);
            ImGui::PopID();
            ImGui::SameLine(0.0F, 10.0F);
        }

        constexpr float LabeledControlColumnWidth = 158.0F;

        void DollyCamera(TCamera& CameraData, float Steps)
        {
            const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
            if (glm::dot(ViewDirection, ViewDirection) <= 1.0e-12F)
            {
                return;
            }

            constexpr float DollyDistancePerStep = 0.35F;
            const glm::vec3 DollyDelta = glm::normalize(ViewDirection) * Steps * DollyDistancePerStep;
            CameraData.SetPosition(CameraData.GetPosition() + DollyDelta);
            CameraData.SetTarget(CameraData.GetTarget() + DollyDelta);
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

        bool LabeledSliderFloat(const char* Label, float* Value, float Minimum, float Maximum, const char* Format)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = ImGui::SliderFloat("##Value", Value, Minimum, Maximum, Format);
            ImGui::PopID();
            return bChanged;
        }

        bool
        LabeledDragFloat(const char* Label, float* Value, float Speed, float Minimum, float Maximum, const char* Format)
        {
            ImGui::PushID(Label);
            BeginLabeledControlRow(Label);
            ImGui::SetNextItemWidth(-1.0F);
            const bool bChanged = ImGui::DragFloat("##Value", Value, Speed, Minimum, Maximum, Format);
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

        bool LabeledCheckbox(const char* Label, bool* Value)
        {
            ImGui::PushID(Label);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Label);
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
            const bool bChanged = ImGui::Checkbox("##Value", Value);
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

    TDebugUI::TDebugUI(const TVulkanContext& Context,
                       const TWindow&      TWindow,
                       TRenderer&          TRenderer,
                       TAssetManager&      Assets)
        : Device(Context.GetDevice()), NativeWindow(TWindow.GetNativeHandle()), FrameRenderer(&TRenderer),
          AssetManager(&Assets)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        IO.IniFilename = "Config/MDSS_EditorLayout.ini";

#if defined(_WIN32)
        constexpr const char* FontCandidates[] = {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf"};
#elif defined(__APPLE__)
        constexpr const char* FontCandidates[] = {"/System/Library/Fonts/Supplemental/Arial.ttf",
                                                   "/System/Library/Fonts/Supplemental/Verdana.ttf"};
#else
        constexpr const char* FontCandidates[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                                   "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"};
#endif
        for (const char* FontPath : FontCandidates)
        {
            if (std::filesystem::exists(FontPath))
            {
                if (ImFont* Font = IO.Fonts->AddFontFromFileTTF(FontPath, 13.0F))
                {
                    IO.FontDefault = Font;
                    break;
                }
            }
        }
        if (IO.FontDefault == nullptr)
        {
            IO.FontDefault = IO.Fonts->AddFontDefault();
        }

        ImGui::StyleColorsDark();
        ImGuiStyle& Style = ImGui::GetStyle();
        Style.WindowPadding = {12.0F, 10.0F};
        Style.FramePadding = {8.0F, 5.0F};
        Style.ItemSpacing = {8.0F, 6.0F};
        Style.WindowRounding = 6.0F;
        Style.FrameRounding = 4.0F;

        if (!ImGui_ImplGlfw_InitForVulkan(TWindow.GetNativeHandle(), true))
        {
            ImGui::DestroyContext();
            throw std::runtime_error("Failed to initialize Dear ImGui GLFW backend.");
        }

        const std::uint32_t ImageCount = static_cast<std::uint32_t>(TRenderer.GetSwapchain().GetImages().size());
        const TSwapchainSupportDetails Support =
            TSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
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
        InitInfo.PipelineInfoMain.RenderPass = TRenderer.GetRenderPassHandle();
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

        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        ImGui::GetIO().FontGlobalScale = DisplaySize.x >= 1280.0F && DisplaySize.y >= 800.0F ? 1.15F : 1.0F;
        SetupDockspace();

        DrawSceneWindow(SceneData);
        DrawCameraWindow(SceneData);
        DrawSelectedTransformWindow(SceneData);
        DrawRenderOptionsWindow(SceneData);
        DrawRenderSettingsWindow();
        DrawViewportStatsOverlay();
        DrawSimulationDebugWindow(SceneData);
        DrawLogWindow();
        ProcessCameraInput(SceneData);
        ProcessSelectionAndGizmo(SceneData);

        if (bInjectMode)
        {
            ImGuiViewport* Viewport = ImGui::GetMainViewport();
            const ImVec2   DisplaySize = ImGui::GetIO().DisplaySize;
            const ImVec2   Center{
                Viewport->Pos.x +
                    (SceneViewportRectNormalized.x + SceneViewportRectNormalized.z * 0.5F) * DisplaySize.x,
                Viewport->Pos.y +
                    (SceneViewportRectNormalized.y + SceneViewportRectNormalized.w * 0.5F) * DisplaySize.y};
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

    void TDebugUI::Render(VkCommandBuffer CommandBuffer) const
    {
        ImDrawData* DrawData = ImGui::GetDrawData();
        if (DrawData != nullptr && DrawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(DrawData, CommandBuffer);
        }
    }

    void TDebugUI::OnSwapchainRecreated(const TVulkanContext& Context, const TRenderer& TRenderer)
    {
        const TSwapchainSupportDetails Support =
            TSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
        const std::uint32_t MinImageCount = std::max(2U, Support.Capabilities.minImageCount);
        ImGui_ImplVulkan_SetMinImageCount(MinImageCount);

        TLogger::Debug("TDebugUI",
                      "ImGui Vulkan backend updated after swapchain recreation (images=" +
                          std::to_string(TRenderer.GetSwapchain().GetImages().size()) + ").");
    }

    bool TDebugUI::IsInjectModeEnabled() const noexcept
    {
        return bInjectMode;
    }

    TStateId TDebugUI::GetInjectState() const noexcept
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

    float TDebugUI::GetSimulationTimeScale() const noexcept
    {
        return SimulationTimeScale;
    }

    bool TDebugUI::IsSimulationPaused() const noexcept
    {
        return bSimulationPaused;
    }

    glm::vec4 TDebugUI::GetSceneViewportRectNormalized() const noexcept
    {
        return SceneViewportRectNormalized;
    }

    std::optional<std::size_t> TDebugUI::GetSelectedObject() const noexcept
    {
        return SelectedObject;
    }

    int TDebugUI::GetHoveredGizmoAxis() const noexcept
    {
        return HoveredGizmoAxis;
    }

    TStateId TDebugUI::GetDebugState() const noexcept
    {
        return DebugState;
    }

    bool TDebugUI::IsKeyboardCaptured() const noexcept
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void TDebugUI::SetupDockspace()
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        constexpr ImGuiDockNodeFlags DockFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        constexpr ImGuiID StableDockspaceID = 0x4D445356U;
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

        ImGui::DockBuilderDockWindow("Scene File", SceneID);
        ImGui::DockBuilderDockWindow("Camera", CameraID);
        ImGui::DockBuilderDockWindow("Selected Transform##SceneTools", TransformID);
        ImGui::DockBuilderDockWindow("Render Settings", RenderSettingsID);
        ImGui::DockBuilderDockWindow("Simulation Debug", RightID);
        ImGui::DockBuilderDockWindow("Log", LogID);
        ImGui::DockBuilderFinish(DockspaceID);
        bDockLayoutInitialized = true;
    }

    void TDebugUI::DrawSceneWindow(TScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 10.0F}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 115.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Scene File", nullptr, Flags);
        if (ImGui::Button("Load Scene"))
        {
            if (const auto Path = TSceneFileDialog::OpenScene())
            {
                try
                {
                    TScene Loaded = TSceneLoader::Load(*Path, *AssetManager);
                    FrameRenderer->ReloadSceneResources(Loaded);
                    SceneData = std::move(Loaded);
                    SelectedObject.reset();
                    ActiveGizmoAxis = -1;
                    SceneStatus = "Loaded: " + Path->filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Load failed: ") + Error.what();
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Scene"))
        {
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
                    SceneStatus = "Saved: " + SavePath.filename().string();
                }
                catch (const std::exception& Error)
                {
                    SceneStatus = std::string("Save failed: ") + Error.what();
                }
            }
        }
        if (SceneStatus.empty())
        {
            ImGui::TextDisabled("Z-up  |  X red, Y green, Z blue");
        }
        else
        {
            ImGui::TextWrapped("%s", SceneStatus.c_str());
        }
        ImGui::End();
    }

    void TDebugUI::ProcessSelectionAndGizmo(TScene& SceneData)
    {
        ImGuiIO&     IO = ImGui::GetIO();
        const ImVec2 DisplaySize = IO.DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2         ViewportOrigin{MainViewport->Pos.x + SceneViewportRectNormalized.x * DisplaySize.x,
                                    MainViewport->Pos.y + SceneViewportRectNormalized.y * DisplaySize.y};
        const ImVec2         ViewportSize{SceneViewportRectNormalized.z * DisplaySize.x,
                                  SceneViewportRectNormalized.w * DisplaySize.y};
        if (ViewportSize.x <= 0.0F || ViewportSize.y <= 0.0F)
        {
            return;
        }
        const float     Aspect = ViewportSize.x / ViewportSize.y;
        const glm::mat4 VP = SceneData.GetMainCamera().GetViewProjectionMatrix(Aspect);
        const glm::vec3 Origin = SceneData.GetMainCamera().GetPosition();
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

        if (ActiveGizmoAxis >= 0)
        {
            HoveredGizmoAxis = ActiveGizmoAxis;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ActiveGizmoAxis = -1;
            }
            else if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
            {
                if (glm::length(glm::vec2(Mouse.x, Mouse.y) - GizmoDragStartMouse) >= 3.0F &&
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
        if (bMouseInViewport && SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                ImVec2 A{}, B{};
                float  WorldScale = 0.0F, Along = 0.0F;
                if (GetAxisScreenSegment(Position, Axis, A, B, WorldScale))
                {
                    const float Distance = PointSegmentDistance(Mouse, A, B, Along);
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        HoveredGizmoAxis = Axis;
                    }
                }
            }
        }

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
            ImGui::IsAnyItemActive() || bInjectMode)
        {
            return;
        }
        if (!bMouseInViewport)
        {
            return;
        }

        if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float           BestDistance = 13.0F;
            int             BestAxis = -1;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                ImVec2 A{}, B{};
                float  WorldScale = 0.0F, Along = 0.0F;
                if (GetAxisScreenSegment(Position, Axis, A, B, WorldScale))
                {
                    const float Distance = PointSegmentDistance(Mouse, A, B, Along);
                    if (Distance < BestDistance)
                    {
                        BestDistance = Distance;
                        BestAxis = Axis;
                    }
                }
            }
            if (BestAxis >= 0)
            {
                ActiveGizmoAxis = BestAxis;
                GizmoDragStartMouse = {Mouse.x, Mouse.y};
                TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
                GizmoDragStartPosition = Transform.Position;
                ImVec2 A{}, B{};
                float  WorldScale = 0.0F;
                if (GetAxisScreenSegment(Transform.Position, BestAxis, A, B, WorldScale))
                {
                    const glm::vec2 ScreenAxis{B.x - A.x, B.y - A.y};
                    GizmoDragPixelLength = glm::length(ScreenAxis);
                    GizmoDragScreenAxis =
                        GizmoDragPixelLength > 0.0F ? ScreenAxis / GizmoDragPixelLength : glm::vec2(0.0F);
                    GizmoDragWorldScale = WorldScale;
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
        if (Hit.Hit)
        {
            SelectedObject = Hit.InstanceIndex;
        }
        else
        {
            SelectedObject.reset();
        }
    }

    void TDebugUI::DrawSelectedTransformWindow(TScene& SceneData)
    {
        if (!SelectedObject || *SelectedObject >= SceneData.GetStaticMeshInstances().size())
        {
            return;
        }
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const ImVec2 Position{Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 135.0F};
        ImGui::SetNextWindowPos(Position, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({330.0F, 190.0F}, ImGuiCond_FirstUseEver);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        ImGui::Begin("Selected Transform##SceneTools", nullptr, Flags);
        TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
        ImGui::Text("Object %zu", *SelectedObject);
        LabeledDragFloat3("Position", &Transform.Position.x, 0.02F);
        LabeledDragFloat3("Rotation", &Transform.RotationDegrees.x, 0.25F);
        LabeledDragFloat3("Scale", &Transform.Scale.x, 0.02F);
        ImGui::TextDisabled("Drag the colored axis arrows to move.");
        ImGui::End();
    }

    void TDebugUI::ProcessCameraInput(TScene& SceneData)
    {
        ImGuiIO& IO = ImGui::GetIO();
        TCamera&  CameraData = SceneData.GetMainCamera();

        const bool bRightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (!bRotatingCamera && bRightMouseDown && !IO.WantCaptureMouse)
        {
            bRotatingCamera = true;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (bRotatingCamera && !bRightMouseDown)
        {
            bRotatingCamera = false;
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        if (bRotatingCamera)
        {
            constexpr float MouseSensitivity = 0.12F;
            glm::vec2       RotationDegrees = CameraData.GetRotationDegrees();
            RotationDegrees.x -= IO.MouseDelta.y * MouseSensitivity;
            RotationDegrees.y += IO.MouseDelta.x * MouseSensitivity;
            CameraData.SetRotationDegrees(RotationDegrees);
        }

        if (!IO.WantCaptureMouse && IO.MouseWheel != 0.0F)
        {
            const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
            if (glm::dot(ViewDirection, ViewDirection) > 1.0e-12F)
            {
                DollyCamera(CameraData, IO.MouseWheel);
            }
        }

        if (IO.WantTextInput || ImGui::IsAnyItemActive())
        {
            return;
        }
        if (ActiveGizmoAxis >= 0)
        {
            return;
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

        constexpr float MoveSpeed = 2.5F;
        constexpr float FastMoveMultiplier = 3.0F;
        const float     Speed = MoveSpeed * (IO.KeyShift ? FastMoveMultiplier : 1.0F);
        const glm::vec3 Delta = glm::normalize(MoveDirection) * Speed * IO.DeltaTime;
        CameraData.SetPosition(CameraData.GetPosition() + Delta);
        CameraData.SetTarget(CameraData.GetTarget() + Delta);
    }

    void TDebugUI::DrawCameraWindow(TScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    CameraWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{CameraWidth, 215.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + 10.0F, Viewport->WorkPos.y + 10.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;

        if (ImGui::Begin("Camera", nullptr, Flags))
        {
            TCamera& CameraData = SceneData.GetMainCamera();

            if (ImGui::Button("Reset Camera View"))
            {
                CameraData.SetPosition({3.0F, -5.0F, 3.0F});
                CameraData.SetTarget({0.0F, 0.0F, 0.0F});
                CameraData.SetVerticalFieldOfViewDegrees(60.0F);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Restore the startup camera position, target, and FOV.");
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

            ImGui::Separator();
            ImGui::TextDisabled("RMB + Mouse: look  |  WASD: move");
            ImGui::TextDisabled("Shift: faster  |  Wheel: dolly");
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
        ImGui::SetNextWindowPos(ViewportNode->Pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize({ViewportNode->Size.x, 110.0F}, ImGuiCond_Always);
        const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        if (DisplaySize.x > 0.0F && DisplaySize.y > 0.0F)
        {
            constexpr float ToolbarHeight = 110.0F;
            SceneViewportRectNormalized = {(ViewportNode->Pos.x - MainViewport->Pos.x) / DisplaySize.x,
                (ViewportNode->Pos.y + ToolbarHeight - MainViewport->Pos.y) / DisplaySize.y,
                ViewportNode->Size.x / DisplaySize.x,
                std::max(ViewportNode->Size.y - ToolbarHeight, 1.0F) / DisplaySize.y};
        }
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                           ImGuiWindowFlags_NoNavFocus;

        if (ImGui::Begin("Render Options##ViewportToolbar", nullptr, Flags))
        {
            TRenderViewMode CurrentMode = FrameRenderer->GetRenderViewMode();
            const int CurrentIndex = static_cast<int>(CurrentMode);
            const char*     CurrentName = "Solver Transfer Weights";
            if (CurrentIndex >= 0 && CurrentIndex < static_cast<int>(RenderViewModeNames.size()))
            {
                CurrentName = RenderViewModeNames[CurrentIndex];
            }
            else if (CurrentMode >= TRenderViewMode::SurfaceStateHeatmap &&
                     CurrentMode < TRenderViewMode::SolverTransferWeight)
            {
                CurrentName =
                    SurfaceDebugViewNames[CurrentIndex - static_cast<int>(TRenderViewMode::SurfaceStateHeatmap)];
            }
            else if (CurrentMode == TRenderViewMode::MesoHeight)
            {
                CurrentName = "Meso Color";
            }
            else if (CurrentMode == TRenderViewMode::MesoOffset)
            {
                CurrentName = "Meso Displacement";
            }
            else if (CurrentMode == TRenderViewMode::MacroGeometry)
            {
                CurrentName = "Macro Geometry";
            }
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("View");
            ImGui::SameLine(0.0F, 8.0F);
            ImGui::SetNextItemWidth(185.0F);
            ImGui::PushID("ViewportViewCombo");
            if (ImGui::BeginCombo("##View", CurrentName, ImGuiComboFlags_HeightLarge))
            {
                ImGui::TextColored({0.55F, 0.78F, 1.0F, 1.0F}, "DISPLAY");
                ImGui::Separator();
                for (int Index = 0; Index < static_cast<int>(RenderViewModeNames.size()); ++Index)
                {
                    const auto Mode = static_cast<TRenderViewMode>(Index);
                    if (ImGui::Selectable(RenderViewModeNames[Index], CurrentMode == Mode))
                    {
                        CurrentMode = Mode;
                        FrameRenderer->SetRenderViewMode(Mode);
                    }
                }
                ImGui::Spacing();
                ImGui::TextColored({1.0F, 0.76F, 0.38F, 1.0F}, "DEBUG");
                ImGui::Separator();
                for (int Index = 0; Index < static_cast<int>(SurfaceDebugViewNames.size()); ++Index)
                {
                    const auto Mode =
                        static_cast<TRenderViewMode>(static_cast<int>(TRenderViewMode::SurfaceStateHeatmap) + Index);
                    if (ImGui::Selectable(SurfaceDebugViewNames[Index], CurrentMode == Mode))
                    {
                        CurrentMode = Mode;
                        FrameRenderer->SetRenderViewMode(Mode);
                    }
                }
                const auto SolverMode = TRenderViewMode::SolverTransferWeight;
                if (ImGui::Selectable("Solver Transfer Weights", CurrentMode == SolverMode))
                {
                    CurrentMode = SolverMode;
                    FrameRenderer->SetRenderViewMode(SolverMode);
                }
                if (ImGui::Selectable("Macro Geometry", CurrentMode == TRenderViewMode::MacroGeometry))
                {
                    CurrentMode = TRenderViewMode::MacroGeometry;
                    FrameRenderer->SetRenderViewMode(CurrentMode);
                }
                if (ImGui::BeginMenu("Meso"))
                {
                    // Meso 높이를 색으로 볼지, 실제 형상 변위로 볼지 선택한다.
                    if (ImGui::RadioButton("Color", CurrentMode == TRenderViewMode::MesoHeight))
                    {
                        CurrentMode = TRenderViewMode::MesoHeight;
                        FrameRenderer->SetRenderViewMode(CurrentMode);
                    }
                    if (ImGui::RadioButton("Displacement", CurrentMode == TRenderViewMode::MesoOffset))
                    {
                        CurrentMode = TRenderViewMode::MesoOffset;
                        FrameRenderer->SetRenderViewMode(CurrentMode);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();

            ImGui::SameLine();
            if (ImGui::Button("-"))
            {
                DollyCamera(SceneData.GetMainCamera(), -1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Zoom out");
            }
            ImGui::SameLine();
            if (ImGui::Button("+"))
            {
                DollyCamera(SceneData.GetMainCamera(), 1.0F);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Zoom in");
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0F);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {8.0F, 2.0F});
            if (ImGui::BeginChild("SelectedViewContext", {0.0F, 50.0F}, true,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                const ImVec4 ContextTitleColor{0.42F, 0.76F, 1.0F, 1.0F};
                auto BeginViewContext = [&](const char* Title, const char* Description)
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(ContextTitleColor, "%s", Title);
                    ImGui::SameLine(0.0F, 10.0F);
                    ImGui::TextDisabled("%s", Description);
                    ImGui::NewLine();
                };

                switch (CurrentMode)
                {
                    case TRenderViewMode::Lit:
                        BeginViewContext("LIT", "재질색 × 조명(노멀 맵 포함).");
                        break;
                    case TRenderViewMode::Unlit:
                        BeginViewContext("UNLIT", "조명 없이 재질색만 표시.");
                        break;
                    case TRenderViewMode::VertexNormalWS:
                        BeginViewContext("VERTEX NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case TRenderViewMode::NormalTextureTS:
                        BeginViewContext("NORMAL TEXTURE", "RGB 채널은 탄젠트 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case TRenderViewMode::MappedNormalWS:
                        BeginViewContext("MAPPED NORMAL", "RGB 채널은 월드 X/Y/Z 성분(-1~+1)을 0.5 기준으로 인코딩.");
                        break;
                    case TRenderViewMode::SurfaceStateHeatmap:
                    {
                        BeginViewContext("STATE HEATMAP", "값=State/용량 비율; 회색=미지원, 진회색=비활성.");
                        const TSurfaceStateRegistry* Registry =
                            AssetManager != nullptr ? &AssetManager->GetSurfaceStateRegistry() : nullptr;
                        const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;
                        if (StateCount == 0)
                        {
                            ImGui::TextDisabled("등록된 State 없음");
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
                            ImGui::SetNextItemWidth(125.0F);
                            if (ImGui::BeginCombo("##HeatmapState", Registry->GetStateName(DebugState).c_str()))
                            {
                                for (std::size_t Index = 0; Index < StateCount; ++Index)
                                {
                                    const TStateId State = static_cast<TStateId>(Index);
                                    const bool bSelected = State == DebugState;
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
                            ImGui::SameLine(0.0F, 10.0F);
                            bool bReliefShadingEnabled = FrameRenderer->IsStateHeatmapReliefShadingEnabled();
                            if (ImGui::Checkbox("Relief", &bReliefShadingEnabled))
                            {
                                FrameRenderer->SetStateHeatmapReliefShadingEnabled(bReliefShadingEnabled);
                            }
                        }
                        DrawLegendColor({0.075F, 0.090F, 0.260F, 1.0F}, "0%");
                        DrawLegendColor({0.120F, 0.610F, 0.540F, 1.0F}, "중간");
                        DrawLegendColor({0.990F, 0.900F, 0.200F, 1.0F}, "100%");
                        DrawLegendColor({0.42F, 0.42F, 0.45F, 1.0F}, "State 미지원");
                        DrawLegendColor({0.18F, 0.20F, 0.24F, 1.0F}, "시뮬레이션 꺼짐");
                        break;
                    }
                    case TRenderViewMode::SurfaceValidity:
                        BeginViewContext("VALIDITY", "Surface 텍셀 유효 여부와 시뮬레이션 적용 여부.");
                        DrawLegendColor({0.10F, 0.78F, 0.24F, 1.0F}, "유효·시뮬레이션 켜짐");
                        DrawLegendColor({0.18F, 0.48F, 0.82F, 1.0F}, "유효·시뮬레이션 꺼짐");
                        DrawLegendColor({0.86F, 0.12F, 0.08F, 1.0F}, "무효");
                        break;
                    case TRenderViewMode::SurfaceID:
                        BeginViewContext("SURFACE ID", "색으로 Surface를 구분합니다. 색상 자체는 수치 의미가 없습니다.");
                        break;
                    case TRenderViewMode::NeighborCount:
                        BeginViewContext("NEIGHBOR COUNT", "유효한 이웃 텍셀 개수.");
                        DrawLegendColor({0.08F, 0.10F, 0.18F, 1.0F}, "0개");
                        DrawLegendColor({0.95F, 0.72F, 0.12F, 1.0F}, "8개");
                        break;
                    case TRenderViewMode::SurfaceSeam:
                        BeginViewContext("UV SEAM", "다른 UV chart로 이어지는 이웃 관계가 있는 경계.");
                        DrawLegendColor({1.0F, 0.18F, 0.72F, 1.0F}, "Seam 이웃 있음");
                        DrawLegendColor({0.12F, 0.16F, 0.22F, 1.0F}, "없음");
                        break;
                    case TRenderViewMode::SolverTransferWeight:
                    {
                        BeginViewContext("TRANSFER WEIGHT", "선택 가중치 평균: 0은 차단, 1은 완전 전달.");
                        int SelectedWeightView = static_cast<int>(FrameRenderer->GetSolverTransferWeightView());
                        ImGui::SetNextItemWidth(205.0F);
                        if (ImGui::Combo("##TransferWeightComponent", &SelectedWeightView,
                                         SolverTransferWeightViewNames.data(),
                                         static_cast<int>(SolverTransferWeightViewNames.size())))
                        {
                            FrameRenderer->SetSolverTransferWeightView(
                                static_cast<TSolverTransferWeightView>(SelectedWeightView));
                        }
                        ImGui::SameLine(0.0F, 10.0F);
                        DrawLegendColor({0.16F, 0.035F, 0.24F, 1.0F}, "0 차단");
                        DrawLegendColor({0.30F, 0.24F, 0.78F, 1.0F}, "0.5 중간");
                        DrawLegendColor({0.18F, 0.94F, 0.98F, 1.0F}, "1 허용");
                        break;
                    }
                    case TRenderViewMode::MesoHeight:
                        BeginViewContext("MESO COLOR", "복원 높이의 부호와 크기.");
                        DrawLegendColor({0.12F, 0.52F, 0.92F, 1.0F}, "음수");
                        DrawLegendColor({0.12F, 0.13F, 0.17F, 1.0F}, "0 기준");
                        DrawLegendColor({1.0F, 0.42F, 0.10F, 1.0F}, "양수");
                        break;
                    case TRenderViewMode::MesoOffset:
                        BeginViewContext("MESO DISPLACEMENT", "높이만큼 정점을 이동하고 복원 노멀로 음영.");
                        break;
                    case TRenderViewMode::MacroGeometry:
                        BeginViewContext("MACRO GEOMETRY", "노멀 맵 효과를 제외한 원본 형상.");
                        break;
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
        }
        ImGui::End();
    }

    void TDebugUI::DrawRenderSettingsWindow()
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        if (ImGui::Begin("Render Settings"))
        {
            float NormalStrength = FrameRenderer->GetNormalStrength();
            if (LabeledSliderFloat("Normal", &NormalStrength, 0.0F, 4.0F, "%.2f"))
            {
                FrameRenderer->SetNormalStrength(NormalStrength);
            }

            float AmbientLight = FrameRenderer->GetAmbientLight();
            if (LabeledSliderFloat("Ambient", &AmbientLight, 0.0F, 1.0F, "%.2f"))
            {
                FrameRenderer->SetAmbientLight(AmbientLight);
            }

            bool bFlipNormalY = FrameRenderer->GetFlipNormalY();
            if (LabeledCheckbox("Flip Normal Y", &bFlipNormalY))
            {
                FrameRenderer->SetFlipNormalY(bFlipNormalY);
            }
        }
        ImGui::End();
    }

    void TDebugUI::DrawViewportStatsOverlay()
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

        constexpr float ToolbarHeight = 68.0F;
        ImGui::SetNextWindowPos({ViewportNode->Pos.x + 10.0F, ViewportNode->Pos.y + ToolbarHeight + 10.0F},
                                ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.72F);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNavFocus |
                                           ImGuiWindowFlags_NoInputs;

        if (ImGui::Begin("Viewport Stats##Overlay", nullptr, Flags))
        {
            const float FPS = ImGui::GetIO().Framerate;
            const float FrameTimeMs = FPS > 0.0F ? 1000.0F / FPS : 0.0F;
            const ImVec4 FPSColor = FPS >= 55.0F ? ImVec4(0.57F, 0.92F, 0.68F, 1.0F)
                                   : FPS >= 30.0F ? ImVec4(1.0F, 0.84F, 0.47F, 1.0F)
                                                  : ImVec4(1.0F, 0.57F, 0.57F, 1.0F);
            ImGui::TextColored(FPSColor, "%.1f FPS", FPS);
            ImGui::SameLine();
            ImGui::TextDisabled("%.2f ms", FrameTimeMs);

            const float SolverMilliseconds = FrameRenderer->GetLastSolverGpuMilliseconds();
            if (SolverMilliseconds >= 0.0F)
            {
                ImGui::TextDisabled("Solver GPU %.2f ms", SolverMilliseconds);
            }
            else
            {
                ImGui::TextDisabled("Solver GPU unavailable");
            }
        }
        ImGui::End();
    }

    void TDebugUI::DrawSimulationDebugWindow(const TScene& SceneData)
    {
        if (AssetManager == nullptr || FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        constexpr float Width = 310.0F;
        const ImVec2 WindowSize{Width, 280.0F};
        const ImVec2 WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - Width - 10.0F,
                                    Viewport->WorkPos.y + 190.0F};
        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_FirstUseEver);

        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_None;
        if (ImGui::Begin("Simulation Debug", nullptr, Flags))
        {
            if (ImGui::BeginTabBar("SimulationTabs"))
            {
                if (ImGui::BeginTabItem("Control"))
                {
                    ImGui::TextDisabled("Playback");
                    if (ImGui::RadioButton("Running", !bSimulationPaused))
                    {
                        bSimulationPaused = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Paused", bSimulationPaused))
                    {
                        bSimulationPaused = true;
                    }

                    ImGui::TextDisabled("Speed preset");
                    if (ImGui::RadioButton("0.25x", std::abs(SimulationTimeScale - 0.25F) < 0.001F))
                    {
                        SimulationTimeScale = 0.25F;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("0.5x", std::abs(SimulationTimeScale - 0.5F) < 0.001F))
                    {
                        SimulationTimeScale = 0.5F;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("1x", std::abs(SimulationTimeScale - 1.0F) < 0.001F))
                    {
                        SimulationTimeScale = 1.0F;
                    }
                    ImGui::SameLine();
                    if (ImGui::RadioButton("2x", std::abs(SimulationTimeScale - 2.0F) < 0.001F))
                    {
                        SimulationTimeScale = 2.0F;
                    }
                    LabeledSliderFloat("Time scale", &SimulationTimeScale, 0.05F, 4.0F, "%.2fx");

                    ImGui::Separator();
                    ImGui::TextDisabled("Solver debug terms");
                    bool bGeometryDrive = FrameRenderer->IsDebugGeometryDriveEnabled();
                    if (LabeledCheckbox("GeometryDrive", &bGeometryDrive))
                    {
                        FrameRenderer->SetDebugGeometryDriveEnabled(bGeometryDrive);
                    }
                    ImGui::SameLine();
                    bool bNormalWeight = FrameRenderer->IsDebugNormalWeightEnabled();
                    if (LabeledCheckbox("NormalWeight", &bNormalWeight))
                    {
                        FrameRenderer->SetDebugNormalWeightEnabled(bNormalWeight);
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Inject"))
                {
                    LabeledCheckbox("Inject mode", &bInjectMode);
                    ImGui::TextDisabled("Aim with the crosshair and press Space.");

                    const TSurfaceStateRegistry* InjectRegistry =
                        AssetManager != nullptr ? &AssetManager->GetSurfaceStateRegistry() : nullptr;
                    const std::size_t InjectStateCount =
                        InjectRegistry != nullptr ? InjectRegistry->GetStateCount() : 0;
                    if (InjectStateCount == 0)
                    {
                        ImGui::TextDisabled("No Surface States are registered.");
                        InjectState = InvalidStateId;
                    }
                    else
                    {
                        if (InjectState >= InjectStateCount)
                        {
                            InjectState = 0;
                        }
                        const std::string& SelectedName = InjectRegistry->GetStateName(InjectState);
                        if (BeginLabeledCombo("State", SelectedName.c_str()))
                        {
                            for (std::size_t Index = 0; Index < InjectStateCount; ++Index)
                            {
                                const TStateId State = static_cast<TStateId>(Index);
                                const std::string& Name = InjectRegistry->GetStateName(State);
                                const bool bSelected = State == InjectState;
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
                    LabeledSliderFloat("Strength", &InjectStrength, 0.0F, 2.0F, "%.2f");
                    LabeledSliderFloat("Falloff", &InjectFalloff, 0.0F, 4.0F, "%.2f");
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Runtime Override"))
                {
                    std::vector<TSRProfileAssetHandle> SceneProfiles;
                    for (const TStaticMeshInstance& Instance : SceneData.GetStaticMeshInstances())
                    {
                        const TSurfaceRuntimeDataHandle SurfaceData = Instance.GetSurfaceData();
                        if (!AssetManager->HasSurfaceData(SurfaceData))
                        {
                            continue;
                        }
                        for (TSRProfileAssetHandle Profile : AssetManager->GetSurfaceProfileTable(SurfaceData))
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
                        ImGui::EndTabItem();
                        ImGui::EndTabBar();
                        ImGui::End();
                        return;
                    }
                    if (std::find(SceneProfiles.begin(), SceneProfiles.end(), DebugParameterProfile) ==
                        SceneProfiles.end())
                    {
                        DebugParameterProfile = SceneProfiles.front();
                        ParameterDraftKey = {InvalidAssetHandle, InvalidStateId};
                    }

                    const auto GetProfileName = [this](TSRProfileAssetHandle Handle) -> const char*
                    { return AssetManager->GetSRProfile(Handle).GetName().c_str(); };
                    if (BeginLabeledCombo("Profile", GetProfileName(DebugParameterProfile)))
                    {
                        for (TSRProfileAssetHandle Profile : SceneProfiles)
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

                    const TSurfaceStateRegistry& Registry = AssetManager->GetSurfaceStateRegistry();
                    const std::size_t            StateCount = Registry.GetStateCount();
                    if (StateCount == 0)
                    {
                        ImGui::TextDisabled("No State channels are registered.");
                        ImGui::EndTabItem();
                        ImGui::EndTabBar();
                        ImGui::End();
                        return;
                    }
                    if (DebugParameterState >= StateCount)
                    {
                        DebugParameterState = 0;
                    }
                    const std::string& StateName = Registry.GetStateName(DebugParameterState);
                    if (BeginLabeledCombo("State", StateName.c_str()))
                    {
                        for (std::size_t Index = 0; Index < StateCount; ++Index)
                        {
                            const TStateId     State = static_cast<TStateId>(Index);
                            const std::string& Name = Registry.GetStateName(State);
                            const bool         bSelected = State == DebugParameterState;
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

                    const std::pair<TSRProfileAssetHandle, TStateId> Key{DebugParameterProfile, DebugParameterState};
                    if (ParameterDraftKey != Key)
                    {
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
                            const TRegisteredSurfaceResponseProfileData Resolved =
                                Registry.ResolveProfile(AssetManager->GetSRProfile(DebugParameterProfile).GetData());
                            if (DebugParameterState < Resolved.States.size() &&
                                Resolved.States[DebugParameterState].has_value())
                            {
                                const auto Override = RuntimeProfileOverrides.find(Key);
                                ParameterDraft = Override != RuntimeProfileOverrides.end()
                                                     ? Override->second
                                                     : *Resolved.States[DebugParameterState];
                                ParameterDrafts[Key] = ParameterDraft;
                                bParameterDraftAvailable = true;
                            }
                        }
                    }

                    const TRegisteredSurfaceResponseProfileData Resolved =
                        Registry.ResolveProfile(AssetManager->GetSRProfile(DebugParameterProfile).GetData());
                    const bool bProfileSupportsState = DebugParameterState < Resolved.States.size() &&
                                                       Resolved.States[DebugParameterState].has_value();
                    if (!bProfileSupportsState || !bParameterDraftAvailable)
                    {
                        ImGui::TextDisabled("This Profile does not define the selected State.");
                        ImGui::EndTabItem();
                        ImGui::EndTabBar();
                        ImGui::End();
                        return;
                    }

                    ImGui::Separator();
                    ImGui::TextDisabled("Runtime only. Apply updates the current scene; source files stay unchanged.");
                    ImGui::TextDisabled("Only parameters currently used by the solver are shown.");
                    bool bChanged = false;
                    bChanged |=
                        LabeledDragFloat("Capacity", &ParameterDraft.StateCapacity, 0.01F, 0.001F, 1000.0F, "%.3f");
                    bChanged |=
                        LabeledDragFloat("Input factor", &ParameterDraft.InputFactor, 0.01F, 0.0F, 100.0F, "%.3f");
                    bChanged |= LabeledDragFloat(
                        "Saturation transfer /s", &ParameterDraft.SaturationTransferRate, 0.01F, 0.0F, 100.0F, "%.3f");
                    bChanged |= LabeledDragFloat(
                        "Geometry transfer /s", &ParameterDraft.GeometryTransferRate, 0.01F, 0.0F, 100.0F, "%.3f");
                    bChanged |= LabeledDragFloat("Decay /s", &ParameterDraft.DecayRate, 0.01F, 0.0F, 100.0F, "%.3f");
                    bChanged |= LabeledSliderFloat(
                        "Cavity retention", &ParameterDraft.CavityRetentionFactor, 0.0F, 1.0F, "%.3f");
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

                    if (ImGui::Button("Apply Override") && bParameterDraftDirty)
                    {
                        try
                        {
                            TSurfaceResponseProfileData ValidationData;
                            ValidationData.States.emplace(Registry.GetStateName(DebugParameterState), ParameterDraft);
                            ValidateSurfaceResponseProfileData(ValidationData);
                            FrameRenderer->SetDebugProfileParameters(
                                DebugParameterProfile, DebugParameterState, ParameterDraft);
                            RuntimeProfileOverrides[Key] = ParameterDraft;
                            DirtyParameterDrafts.erase(Key);
                            bParameterDraftDirty = false;
                            ParameterDrafts[Key] = ParameterDraft;
                        }
                        catch (const std::exception& Exception)
                        {
                            ParameterStatus = Exception.what();
                            TLogger::Warning("TDebugUI", "Could not apply Profile override: " + ParameterStatus);
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Restore Profile Values"))
                    {
                        const TSurfaceStateParameters& Original = *Resolved.States[DebugParameterState];
                        if (RuntimeProfileOverrides.contains(Key))
                        {
                            FrameRenderer->SetDebugProfileParameters(
                                DebugParameterProfile, DebugParameterState, Original, false);
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
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
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
