#include "DebugUI/Core/DebugUIShared.h"

namespace MDSS
{
// ============================================================
// 00. Debug UI Lifecycle
// ============================================================
#pragma region 00_Debug_UI_Lifecycle

    FDebugUI::FDebugUI(const GPU::FVulkanContext&         Context,
                       const FWindow&                     Window,
                       Rendering::FRenderer&              Renderer,
                       Asset::FAssetManager&              Assets,
                       SurfaceState::FSurfaceDataManager& SurfaceData)
        : Device(Context.GetDevice()), NativeWindow(Window.GetNativeHandle()), FrameRenderer(&Renderer),
          AssetManager(&Assets), SurfaceDataManager(&SurfaceData)
    {
        ResetProfilingAverages();
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
        std::filesystem::create_directories(GetEngineConfigDirectory());
        ViewportState.EditorLayoutPath = (GetEngineConfigDirectory() / "EditorLayout.ini").string();
        IO.IniFilename = ViewportState.EditorLayoutPath.c_str();

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
                    ViewportState.SectionHeaderFont =
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
        if (ViewportState.SectionHeaderFont == nullptr)
        {
            ImFontConfig HeaderFontConfig{};
            HeaderFontConfig.SizePixels = 17.0F;
            ViewportState.SectionHeaderFont = IO.Fonts->AddFontDefault(&HeaderFontConfig);
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
        Style.DisabledAlpha = 0.78F;
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

        if (!ImGui_ImplGlfw_InitForVulkan(Window.GetNativeHandle(), true))
        {
            ImGui::DestroyContext();
            throw std::runtime_error("Failed to initialize Dear ImGui GLFW backend.");
        }

        const std::uint32_t ImageCount = static_cast<std::uint32_t>(Renderer.GetSwapchain().GetImages().size());
        const GPU::FSwapchainSupportDetails Support =
            GPU::FSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
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
        FLogger::Info("TDebugUI", "Dear ImGui initialized with GLFW/Vulkan backends.");
        FLogger::Debug("TDebugUI",
                       "Camera/render controls, injection controls, normal debug views, and log filtering are active.");
        LogState.Entries = FLogger::GetEntries();
        LogState.LastSeenRevision = FLogger::GetRevision();
    }

    FDebugUI::~FDebugUI()
    {
        if (NativeWindow != nullptr && bRotatingCamera)
        {
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        if (Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Device);
        }

        FLogger::Verbose("TDebugUI", "Shutting down Dear ImGui backends.");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

#pragma endregion
// ============================================================
// 01. Frame Presentation
// ============================================================
#pragma region 01_Frame_Presentation

    void FDebugUI::BeginFrame(FScene& SceneData)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ViewportState.bMouseOverViewportOverlay = false;

        const ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        ImGui::GetIO().FontGlobalScale = DisplaySize.x >= 1280.0F && DisplaySize.y >= 800.0F ? 1.15F : 1.0F;
        if (ImGui::IsKeyPressed(ImGuiKey_F10))
            TogglePresentationMode();
        if (Presentation.bPresentationMode && ImGui::IsKeyPressed(ImGuiKey_Escape))
            TogglePresentationMode();
        if (ImGui::IsKeyPressed(ImGuiKey_F9) && SceneData.HasDemoAnimation())
            ReplayDemo(SceneData);
        UpdateDemoPlaybackGridAxis(SceneData);
        if (Presentation.bPresentationMode)
        {
            DrawDemoWeatherOverlay(SceneData);
            ImGui::Render();
            return;
        }
        SetupDockspace();

        // Render the viewport windows before the other editor windows.  The
        // viewport panels are transparent overlays, so creating them last
        // would also make them the topmost ImGui hit-test target when another
        // window is visually above the viewport.
        DrawRenderOptionsWindow(SceneData);
        DrawViewportPanels(SceneData);
        DrawDemoWeatherOverlay(SceneData);
        DrawBrickCubeTexelMeshNotice(SceneData);
        DrawSceneWindow(SceneData);
        DrawAnimationWindow(SceneData);
        DrawCameraWindow(SceneData);
        DrawSelectedObjectWindow(SceneData);
        DrawRenderSettingsWindow(SceneData);
        DrawViewportStatsOverlay();
        if (ViewportState.bMouseOverViewportOverlay)
            ViewportState.HoveredViewportId = 0;
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

    void FDebugUI::DrawSectionHeader(const char* Title, float TopPadding, float BottomPadding) const
    {
        ImGui::Dummy({0.0F, TopPadding});
        ImGui::PushStyleColor(ImGuiCol_Text, {0.91F, 0.93F, 0.97F, 1.0F});
        if (ViewportState.SectionHeaderFont != nullptr)
        {
            ImGui::PushFont(ViewportState.SectionHeaderFont);
        }
        ImGui::TextUnformatted(Title);
        if (ViewportState.SectionHeaderFont != nullptr)
        {
            ImGui::PopFont();
        }
        ImGui::PopStyleColor();
        ImGui::Separator();
        ImGui::Dummy({0.0F, BottomPadding});
    }

    void FDebugUI::Render(VkCommandBuffer CommandBuffer) const
    {
        ImDrawData* DrawData = ImGui::GetDrawData();
        if (DrawData != nullptr && DrawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(DrawData, CommandBuffer);
        }
    }

    void FDebugUI::OnSwapchainRecreated(const GPU::FVulkanContext& Context, const Rendering::FRenderer& Renderer)
    {
        const GPU::FSwapchainSupportDetails Support =
            GPU::FSwapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
        const std::uint32_t MinImageCount = std::max(2U, Support.Capabilities.minImageCount);
        ImGui_ImplVulkan_SetMinImageCount(MinImageCount);

        FLogger::Debug("TDebugUI",
                       "ImGui Vulkan backend updated after swapchain recreation (images=" +
                           std::to_string(Renderer.GetSwapchain().GetImages().size()) + ").");
    }

#pragma endregion
// ============================================================
// 02. UI State Accessors
// ============================================================
#pragma region 02_UI_State_Accessors

    bool FDebugUI::IsInjectModeEnabled() const noexcept
    {
        return bInjectMode && !Presentation.bPresentationMode;
    }

    SurfaceState::FStateId FDebugUI::GetInjectState() const noexcept
    {
        return Simulation.InjectState;
    }

    float FDebugUI::GetInjectStrength() const noexcept
    {
        return Simulation.InjectStrength;
    }

    float FDebugUI::GetInjectRadius() const noexcept
    {
        return Simulation.InjectRadius;
    }

    float FDebugUI::GetInjectFalloff() const noexcept
    {
        return Simulation.InjectFalloff;
    }

    SurfaceState::ESurfaceContactOperation FDebugUI::GetInjectOperation() const noexcept
    {
        return Simulation.InjectOperation;
    }

    SurfaceState::ESurfaceContactShape FDebugUI::GetInjectShape() const noexcept
    {
        return Simulation.InjectShape;
    }

    float FDebugUI::GetInjectSecondaryRadius() const noexcept
    {
        return Simulation.InjectSecondaryRadius;
    }

    float FDebugUI::GetInjectRotationDegrees() const noexcept
    {
        return Simulation.InjectRotationDegrees;
    }

    std::uint32_t FDebugUI::GetInjectTexelSearchRadius() const noexcept
    {
        return static_cast<std::uint32_t>(Simulation.InjectTexelSearchRadius);
    }

    float FDebugUI::GetAnimationTimeScale() const noexcept
    {
        return Simulation.AnimationTimeScale;
    }

    float FDebugUI::GetSimulationTimeScale() const noexcept
    {
        return Simulation.SimulationTimeScale;
    }

    bool FDebugUI::IsSimulationPaused() const noexcept
    {
        return Simulation.bSimulationPaused;
    }

    bool FDebugUI::ConsumeSolverStepRequest() noexcept
    {
        const bool bRequested = Simulation.bSolverStepRequested;
        Simulation.bSolverStepRequested = false;
        return bRequested;
    }

    bool FDebugUI::ConsumeSolverResetRequest() noexcept
    {
        const bool bRequested = Simulation.bSolverResetRequested;
        Simulation.bSolverResetRequested = false;
        return bRequested;
    }

    bool FDebugUI::ConsumeFrameTimeResetRequest() noexcept
    {
        const bool bRequested = Simulation.bFrameTimeResetRequested;
        Simulation.bFrameTimeResetRequested = false;
        return bRequested;
    }

#pragma endregion
// ============================================================
// 03. Viewport and Selection Accessors
// ============================================================
#pragma region 03_Viewport_and_Selection_Accessors

    void FDebugUI::DrawDemoWeatherOverlay(const FScene& SceneData)
    {
        const auto* Cue = SceneData.GetDemoWeather();
        if (!Cue)
            return;
        const float Time = SceneData.GetDemoAnimationTime();
        if (Time >= Cue->CloudEndSeconds)
            return;

        const ImVec2    Display = ImGui::GetIO().DisplaySize;
        const ImVec2    Origin = ImGui::GetMainViewport()->Pos;
        const glm::vec4 Rect = GetViewportRectNormalized(GetActiveViewportIndex());
        if (Rect.z <= 0.0F || Rect.w <= 0.0F)
            return;
        const ImVec2 Min{Origin.x + Rect.x * Display.x, Origin.y + Rect.y * Display.y};
        const ImVec2 Max{Min.x + Rect.z * Display.x, Min.y + Rect.w * Display.y};
        const float  Width = Max.x - Min.x;
        const float  Height = Max.y - Min.y;
        const float  Enter = std::clamp((Time + 0.05F) / 0.75F, 0.0F, 1.0F);
        const float  Leave = std::clamp((Cue->CloudEndSeconds - Time) / 1.2F, 0.0F, 1.0F);
        const float  CloudAlpha = Cue->Intensity * Enter * Leave;
        const float  RainAlpha = Cue->Intensity * std::clamp((Time - Cue->RainStartSeconds) / 0.45F, 0.0F, 1.0F) *
                                std::clamp((Cue->RainEndSeconds - Time) / 0.45F, 0.0F, 1.0F);
        ImDrawList* Draw = ImGui::GetBackgroundDrawList();
        Draw->PushClipRect(Min, Max, true);
        const float Band = std::min(Height * 0.19F, 115.0F);
        Draw->AddRectFilledMultiColor(Min,
                                      {Max.x, Min.y + Band},
                                      IM_COL32(36, 43, 56, static_cast<int>(CloudAlpha * 155.0F)),
                                      IM_COL32(36, 43, 56, static_cast<int>(CloudAlpha * 155.0F)),
                                      IM_COL32(36, 43, 56, 0),
                                      IM_COL32(36, 43, 56, 0));
        if (RainAlpha > 0.0F)
        {
            for (int I = 0; I < 110; ++I)
            {
                const float SeedX = std::fmod(static_cast<float>(I * 73 + 19), 109.0F) / 109.0F;
                const float SeedY = std::fmod(static_cast<float>(I * 47 + 7), 113.0F) / 113.0F;
                const float X = Min.x + SeedX * Width;
                const float Y = Min.y + std::fmod(SeedY + Time * (0.27F + 0.06F * (I % 4)), 1.0F) * Height;
                Draw->AddLine(
                    {X, Y}, {X - 3.0F, Y + 12.0F}, IM_COL32(181, 207, 231, static_cast<int>(RainAlpha * 95.0F)), 1.0F);
            }
        }
        Draw->PopClipRect();
    }

    std::size_t FDebugUI::GetViewportCount() const noexcept
    {
        return Presentation.bPresentationMode ? 1U : ViewportState.ViewportPanels.size();
    }

    glm::vec4 FDebugUI::GetViewportRectNormalized(std::size_t Index) const noexcept
    {
        if (Presentation.bPresentationMode)
            return Index == 0 ? glm::vec4(0.0F, 0.0F, 1.0F, 1.0F) : glm::vec4(0.0F);
        return Index < ViewportState.ViewportPanels.size() ? ViewportState.ViewportPanels[Index].RectNormalized
                                                           : glm::vec4(0.0F);
    }

    const FCamera& FDebugUI::GetViewportCamera(const FScene& SceneData, std::size_t Index) const noexcept
    {
        (void)Index;
        return SceneData.GetMainCamera();
    }

    FCamera& FDebugUI::GetViewportCamera(FScene& SceneData, std::size_t Index) noexcept
    {
        (void)Index;
        return SceneData.GetMainCamera();
    }

    std::size_t FDebugUI::GetActiveViewportIndex() const noexcept
    {
        for (std::size_t Index = 0; Index < ViewportState.ViewportPanels.size(); ++Index)
            if (ViewportState.ViewportPanels[Index].Id == ViewportState.ActiveViewportId)
                return Index;
        return 0;
    }

    void FDebugUI::ApplyCameraViewPreset(FScene& SceneData, std::size_t Index)
    {
        if (!Gizmo.SelectedObject || *Gizmo.SelectedObject >= SceneData.GetStaticMeshInstances().size() ||
            Index >= CameraViewPresetPitchYawDegrees.size())
        {
            return;
        }

        FTransform& Transform = SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform();
        glm::vec3   Target = Transform.Position;
        if (AssetManager != nullptr)
        {
            const Asset::FMeshAsset& Mesh =
                AssetManager->GetMesh(SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetMesh());
            const auto& Vertices = Mesh.GetVertices();
            if (!Vertices.empty())
            {
                glm::vec3 Minimum(std::numeric_limits<float>::max());
                glm::vec3 Maximum(std::numeric_limits<float>::lowest());
                for (const Asset::FVertex& Vertex : Vertices)
                {
                    Minimum = glm::min(Minimum, Vertex.Position);
                    Maximum = glm::max(Maximum, Vertex.Position);
                }
                const glm::vec3 LocalBoundsCenter = (Minimum + Maximum) * 0.5F;
                Target = glm::vec3(Transform.GetMatrix() * glm::vec4(LocalBoundsCenter, 1.0F));
            }
        }

        FCamera&        Camera = GetViewportCamera(SceneData, GetActiveViewportIndex());
        const glm::vec3 ViewOffset = Camera.GetPosition() - Camera.GetTarget();
        const float     OffsetLength = glm::length(ViewOffset);
        const float     Distance = OffsetLength > 1.0e-6F ? OffsetLength : 3.0F;
        const float     Pitch = glm::radians(CameraViewPresetPitchYawDegrees[Index][0]);
        const float     Yaw = glm::radians(CameraViewPresetPitchYawDegrees[Index][1]);
        const glm::vec3 Forward{std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch)};
        Camera.SetPosition(Target - Forward * Distance);
        Camera.SetTarget(Target);
    }

    const FCamera& FDebugUI::GetActiveViewportCamera(const FScene& SceneData) const noexcept
    {
        return GetViewportCamera(SceneData, GetActiveViewportIndex());
    }

    std::optional<std::size_t> FDebugUI::GetSelectedObject() const noexcept
    {
        return Presentation.bPresentationMode ? std::nullopt : Gizmo.SelectedObject;
    }

    int FDebugUI::GetHoveredGizmoAxis() const noexcept
    {
        return Gizmo.HoveredAxis;
    }

    bool FDebugUI::IsRotationGizmoMode() const noexcept
    {
        return Gizmo.bRotationMode;
    }

    SurfaceState::FStateId FDebugUI::GetDebugState() const noexcept
    {
        return Simulation.DebugState;
    }

    bool FDebugUI::ShouldSuppressDebugHotkey() const noexcept
    {
        const ImGuiIO& IO = ImGui::GetIO();
        return ViewportState.bMouseOverViewportOverlay || IO.WantTextInput ||
               (ViewportState.HoveredViewportId == 0 && (IO.WantCaptureKeyboard || ImGui::IsAnyItemActive() ||
                                                         ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)));
    }

#pragma endregion
} // namespace MDSS
