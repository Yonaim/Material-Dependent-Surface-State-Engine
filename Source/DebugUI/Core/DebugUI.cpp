/**
 * @file DebugUI.cpp
 * @brief 디버그 UI의 초기화·프레임·종료와 공통 입력·표시 상태를 관리한다.
 */

#include "Application/Editor/SurfaceSimulation/SurfaceSimulation.h"
#include "DebugUI/Core/DebugUIShared.h"
#include "DebugUI/Panels/EditorLayoutStyle.h"

namespace MDSS
{
// ============================================================
// 00. Debug UI Lifecycle
// ============================================================
#pragma region 00_Debug_UI_Lifecycle
    // ImGui와 플랫폼·GPU backend의 초기화·종료 순서를 관리한다.

    FDebugUI::FDebugUI(const GPU::FVulkanContext         &Context,
                       const FWindow                     &Window,
                       Rendering::FRenderer              &Renderer,
                       Asset::FAssetManager              &Assets,
                       SurfaceState::FSurfaceDataManager &SurfaceData)
        : GpuContext(Context),
          Device(Context.GetDevice()),
          NativeWindow(Window.GetNativeHandle()),
          FrameRenderer(&Renderer),
          AssetManager(&Assets),
          SurfaceDataManager(&SurfaceData)
    {
        InitializeImGuiContext();
        ConfigureImGuiFonts();
        ConfigureImGuiStyle();
        InitializeImGuiBackends(Context, Window, Renderer);

        ResetSurfaceStateSettings();
        FLogger::Info("TDebugUI", "Dear ImGui initialized with GLFW/Vulkan backends.");
        FLogger::Debug("TDebugUI",
                       "Camera/render controls, injection controls, normal debug views, and log filtering are active.");
        LogState.Entries = FLogger::GetEntries();
        LogState.LastSeenRevision = FLogger::GetRevision();
    }

    // 00.00. Context and Appearance
    #pragma region 00_00_Context_and_Appearance
        // ImGui 컨텍스트·폰트·스타일을 UI 내용과 독립적으로 초기화한다.

        void FDebugUI::InitializeImGuiContext()
        {
            ResetProfilingAverages();
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO &IO = ImGui::GetIO();
            IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
            std::filesystem::create_directories(GetEngineConfigDirectory());
            ViewportState.EditorLayoutPath = (GetEngineConfigDirectory() / "EditorWorkspace-v3.ini").string();
            IO.IniFilename = ViewportState.EditorLayoutPath.c_str();
        }

        void FDebugUI::ConfigureImGuiFonts()
        {
            ImGuiIO &IO = ImGui::GetIO();

                                                                                                    #if defined(_WIN32)
            constexpr const char *FontCandidates[] = {"C:/Windows/Fonts/malgun.ttf",
                                                      "C:/Windows/Fonts/segoeui.ttf",
                                                      "C:/Windows/Fonts/arial.ttf"};
            constexpr const char *BoldFontCandidates[] = {"C:/Windows/Fonts/malgunbd.ttf",
                                                          "C:/Windows/Fonts/segoeuib.ttf",
                                                          "C:/Windows/Fonts/arialbd.ttf"};
                                                                                                    #elif defined(__APPLE__)
            constexpr const char *FontCandidates[] = {"/System/Library/Fonts/AppleSDGothicNeo.ttc",
                                                      "/System/Library/Fonts/Supplemental/Arial.ttf",
                                                      "/System/Library/Fonts/Supplemental/Verdana.ttf"};
            constexpr const char *BoldFontCandidates[] = {"/System/Library/Fonts/Supplemental/Arial Bold.ttf",
                                                          "/System/Library/Fonts/Supplemental/Verdana Bold.ttf"};
                                                                                                    #else
            constexpr const char *FontCandidates[] = {"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
                                                      "/usr/share/fonts/truetype/nanum/NanumGothic.ttf",
                                                      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                                      "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"};
            constexpr const char *BoldFontCandidates[] = {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
                                                          "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf",
                                                          "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc"};
                                                                                                    #endif
            for (const char *FontPath : FontCandidates)
            {
                if (std::filesystem::exists(FontPath))
                {
                    if (ImFont *Font =
                            IO.Fonts->AddFontFromFileTTF(FontPath, 14.0F, nullptr, IO.Fonts->GetGlyphRangesKorean()))
                    {
                        IO.FontDefault = Font;
                        ViewportState.SectionHeaderFont =
                            IO.Fonts->AddFontFromFileTTF(FontPath, 16.0F, nullptr, IO.Fonts->GetGlyphRangesKorean());
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
                HeaderFontConfig.SizePixels = 16.0F;
                ViewportState.SectionHeaderFont = IO.Fonts->AddFontDefault(&HeaderFontConfig);
            }

            // 상태 요약의 영문 항목명은 본문과 같은 크기의 굵은 글꼴을 쓴다.
            for (const char *FontPath : BoldFontCandidates)
            {
                if (!std::filesystem::exists(FontPath))
                    continue;
                ViewportState.BoldLabelFont = IO.Fonts->AddFontFromFileTTF(FontPath, 14.F);
                if (ViewportState.BoldLabelFont)
                    break;
            }
        }

        void FDebugUI::ConfigureImGuiStyle()
        {
            ImGui::StyleColorsDark();
            ImGuiStyle &Style = ImGui::GetStyle();
            Style.WindowPadding = {12.F, 10.F};
            Style.FramePadding = {9.F, 5.F};
            Style.ItemSpacing = {8.F, 7.F};
            Style.WindowRounding = 6.F;
            Style.FrameRounding = 4.F;
            Style.PopupRounding = 6.F;
            Style.ScrollbarRounding = 5.F;
            Style.GrabRounding = 4.F;
            Style.TabRounding = 4.F;
            Style.TabBarBorderSize = 1.F;
            Style.DisabledAlpha = 0.75F;

            const auto Color = [](std::uint32_t RGB)
            {
                return ImVec4(static_cast<float>((RGB >> 16U) & 255U) / 255.F,
                              static_cast<float>((RGB >> 8U) & 255U) / 255.F,
                              static_cast<float>(RGB & 255U) / 255.F,
                              1.F);
            };
            const auto Panel = Color(0x191E26);
            const auto Chrome = Color(0x12171E);
            const auto Control = Color(0x252D38);
            const auto Hover = Color(0x355F61);
            const auto Selected = Color(0x2B4D50);
            const auto Accent = Color(0x84E3D5);
            const auto Border = Color(0x343E4C);
            auto      &Colors = Style.Colors;
            Colors[ImGuiCol_Text] = Color(0xE8EDF5);
            Colors[ImGuiCol_TextDisabled] = Color(0xA1ADBD);
            Colors[ImGuiCol_WindowBg] = Panel;
            Colors[ImGuiCol_ChildBg] = Panel;
            Colors[ImGuiCol_PopupBg] = Color(0x202732);
            Colors[ImGuiCol_MenuBarBg] = Chrome;
            Colors[ImGuiCol_DockingEmptyBg] = Chrome;
            Colors[ImGuiCol_DockingPreview] = Color(0x559A99);
            Colors[ImGuiCol_Border] = Border;
            Colors[ImGuiCol_Separator] = Border;
            Colors[ImGuiCol_SeparatorHovered] = Hover;
            Colors[ImGuiCol_SeparatorActive] = Accent;
            Colors[ImGuiCol_TitleBg] = Chrome;
            Colors[ImGuiCol_TitleBgActive] = Chrome;
            Colors[ImGuiCol_TitleBgCollapsed] = Chrome;
            Colors[ImGuiCol_FrameBg] = Control;
            Colors[ImGuiCol_FrameBgHovered] = Hover;
            Colors[ImGuiCol_FrameBgActive] = Selected;
            Colors[ImGuiCol_Button] = Control;
            Colors[ImGuiCol_ButtonHovered] = Hover;
            Colors[ImGuiCol_ButtonActive] = Selected;
            Colors[ImGuiCol_Header] = Selected;
            Colors[ImGuiCol_HeaderHovered] = Hover;
            Colors[ImGuiCol_HeaderActive] = Selected;
            Colors[ImGuiCol_CheckMark] = Accent;
            Colors[ImGuiCol_CheckboxSelectedBg] = Selected;
            Colors[ImGuiCol_SliderGrab] = Color(0x69BDB5);
            Colors[ImGuiCol_SliderGrabActive] = Accent;
            Colors[ImGuiCol_Tab] = Chrome;
            Colors[ImGuiCol_TabHovered] = Hover;
            Colors[ImGuiCol_TabSelected] = Selected;
            Colors[ImGuiCol_TabSelectedOverline] = Accent;
            Colors[ImGuiCol_TabDimmed] = Chrome;
            Colors[ImGuiCol_TabDimmedSelected] = Control;
            Colors[ImGuiCol_TabDimmedSelectedOverline] = Color(0x539996);
            Colors[ImGuiCol_TableHeaderBg] = Control;
            Colors[ImGuiCol_TableBorderStrong] = Border;
            Colors[ImGuiCol_TableBorderLight] = Control;
            Colors[ImGuiCol_TableRowBgAlt] = Color(0x202733);
            Colors[ImGuiCol_TextSelectedBg] = Selected;
            Colors[ImGuiCol_NavCursor] = Accent;
            Colors[ImGuiCol_ResizeGrip] = Control;
            Colors[ImGuiCol_ResizeGripHovered] = Hover;
            Colors[ImGuiCol_ResizeGripActive] = Accent;
            Colors[ImGuiCol_ScrollbarBg] = Chrome;
            Colors[ImGuiCol_ScrollbarGrab] = Border;
            Colors[ImGuiCol_ScrollbarGrabHovered] = Hover;
            Colors[ImGuiCol_ScrollbarGrabActive] = Color(0x536574);

            // ImGui의 Vulkan 셰이더가 버텍스 색 공간을 변환하지 않으므로 sRGB attachment에 선형 색을 준다.
            const VkFormat Format = FrameRenderer->GetSwapchain().GetImageFormat();
            if (Format == VK_FORMAT_B8G8R8A8_SRGB || Format == VK_FORMAT_R8G8B8A8_SRGB ||
                Format == VK_FORMAT_A8B8G8R8_SRGB_PACK32)
            {
                const auto ToLinear = [](float Component)
                { return Component <= 0.04045F ? Component / 12.92F : std::pow((Component + 0.055F) / 1.055F, 2.4F); };
                for (auto &Value : Colors)
                {
                    Value.x = ToLinear(Value.x);
                    Value.y = ToLinear(Value.y);
                    Value.z = ToLinear(Value.z);
                }
            }
        }

    #pragma endregion

    // 00.01. Platform and GPU Backends
    #pragma region 00_01_Platform_and_GPU_Backends
        // GLFW·Vulkan backend의 초기화 실패 시 기존과 동일한 단계로 자원을 되돌린다.

        void FDebugUI::InitializeImGuiBackends(const GPU::FVulkanContext &Context,
                                               const FWindow             &Window,
                                               Rendering::FRenderer      &Renderer)
        {
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
        }

    #pragma endregion

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

        EditorLab.reset();
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
    // 이번 frame의 UI 입력과 패널을 구성해 렌더링할 draw 자료를 만든다.

    void FDebugUI::BeginFrame(FScene &SceneData)
    {
        BeginImGuiFrame(SceneData);

        if (Presentation.bPresentationMode)
        {
            if (UsesSurfaceSimulation())
            {
                const auto *Main = ImGui::GetMainViewport();
                EditorLab->DrawViewport(Main->Pos, Main->Size, true);
            }
            else
                DrawDemoWeatherOverlay(SceneData);
            ImGui::Render();
            return;
        }

        DrawEditorPanels(SceneData);
        if (!UsesSurfaceSimulation())
            DrawInjectionCrosshair();

        ImGui::Render();
    }

    // 01.00. Frame Input and Panels
    #pragma region 01_00_Frame_Input_and_Panels
        // 입력 처리와 UI 창의 표시 순서를 유지한다(투명 overlay의 hit-test 순서에 주의).

        void FDebugUI::BeginImGuiFrame(FScene &SceneData)
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
            if (UsesSurfaceSimulation())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_F9))
                    EditorLab->Restart();
            }
            else
            {
                if (ImGui::IsKeyPressed(ImGuiKey_F9) && SceneData.HasDemoAnimation())
                    ReplayDemo(SceneData);
            }
        }

        void FDebugUI::DrawEditorPanels(FScene &SceneData)
        {
            DrawWorkspaceMenu();
            SetupDockspace();
            if (UsesSurfaceSimulation())
                EditorLab->BeginFrame();
            DrawSceneLibrary(SceneData);
            DrawViewportToolbar(SceneData);

            // Render the viewport windows before the other editor windows.  The
            // viewport panels are transparent overlays, so creating them last
            // would also make them the topmost ImGui hit-test target when another
            // window is visually above the viewport.
            if (UsesSurfaceSimulation())
            {
                DrawSurfaceSimulationViewport();
            }
            else
            {
                DrawViewportPanels(SceneData);
                if (bSceneWeatherPreview)
                    DrawDemoWeatherOverlay(SceneData);
                if (ViewportState.bMouseOverViewportOverlay)
                    ViewportState.HoveredViewportId = 0;
                ProcessCameraInput(SceneData);
                ProcessSelectionAndGizmo(SceneData);
            }
            DrawRenderOptionsWindow(SceneData);
            DrawWorkspaceDetails(SceneData);
            DrawWorkspaceTools();
            DrawProfilingWindow();
        }

    #pragma endregion

    // 01.01. Injection Cursor
    #pragma region 01_01_Injection_Cursor
        // Inject 모드에서 활성 뷰포트 중심에 십자선을 표시한다.

        void FDebugUI::DrawInjectionCrosshair()
        {
            if (bInjectMode && GetViewportRectNormalized(GetActiveViewportIndex()).z > 0.0F)
            {
                ImGuiViewport  *Viewport = ImGui::GetMainViewport();
                const ImVec2    DisplaySize = ImGui::GetIO().DisplaySize;
                const glm::vec4 Rect = GetViewportRectNormalized(GetActiveViewportIndex());
                const ImVec2    Center{Viewport->Pos.x + (Rect.x + Rect.z * 0.5F) * DisplaySize.x,
                                       Viewport->Pos.y + (Rect.y + Rect.w * 0.5F) * DisplaySize.y};
                constexpr float CrosshairHalfSize = 9.0F;
                constexpr ImU32 CrosshairColor = IM_COL32(255, 255, 255, 230);
                ImDrawList     *DrawList = ImGui::GetForegroundDrawList();
                DrawList->AddLine({Center.x - CrosshairHalfSize, Center.y},
                                  {Center.x + CrosshairHalfSize, Center.y},
                                  CrosshairColor,
                                  2.0F);
                DrawList->AddLine({Center.x, Center.y - CrosshairHalfSize},
                                  {Center.x, Center.y + CrosshairHalfSize},
                                  CrosshairColor,
                                  2.0F);
            }
        }

    #pragma endregion

    void FDebugUI::SetSurfaceSimulationEnabled(bool bEnabled)
    {
        if (bEnabled && !EditorLab)
            EditorLab = std::make_unique<SurfaceSimulation::FEditorSession>(GpuContext);
        bSurfaceSimulationEnabled = bEnabled;
    }

    bool FDebugUI::UsesSurfaceSimulation() const noexcept
    {
        return bSurfaceSimulationEnabled;
    }

    void FDebugUI::RecordSurfaceSimulation(VkCommandBuffer Commands, std::uint32_t FrameSlot) const
    {
        if (UsesSurfaceSimulation())
            EditorLab->Record(Commands, FrameSlot);
    }

    void FDebugUI::DrawSurfaceSimulationViewport()
    {
        const auto *Main = ImGui::GetMainViewport();
        const auto  Size = ImGui::GetIO().DisplaySize;
        const auto &Rect = ViewportState.ViewportWorkspaceRectNormalized;
        EditorLab->DrawViewport({Main->Pos.x + Rect.x * Size.x, Main->Pos.y + Rect.y * Size.y},
                                {Rect.z * Size.x, Rect.w * Size.y});
    }

    void FDebugUI::DrawSectionHeader(const char *Title, float TopPadding, float BottomPadding) const
    {
        EditorLayout::SectionTitle(Title, TopPadding, BottomPadding);
    }

    void FDebugUI::Render(VkCommandBuffer CommandBuffer) const
    {
        ImDrawData *DrawData = ImGui::GetDrawData();

        if (DrawData != nullptr && DrawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(DrawData, CommandBuffer);
        }
    }

    void FDebugUI::OnSwapchainRecreated(const GPU::FVulkanContext &Context, const Rendering::FRenderer &Renderer)
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
    // 현재 UI 표시 상태와 진단 선택을 관련 소비자에게 제공한다.

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
    // 활성 viewport와 선택 대상 정보를 카메라·진단 처리에서 공유한다.

    void FDebugUI::DrawDemoWeatherOverlay(const FScene &SceneData)
    {
        const auto *Cue = SceneData.GetDemoWeather();

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
        ImDrawList  *Draw = ImGui::GetBackgroundDrawList();
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
                Draw->AddLine({X, Y},
                              {X - 3.0F, Y + 12.0F},
                              IM_COL32(181, 207, 231, static_cast<int>(RainAlpha * 95.0F)),
                              1.0F);
            }
        }

        Draw->PopClipRect();
    }

    std::size_t FDebugUI::GetViewportCount() const noexcept
    {
        return UsesSurfaceSimulation() ? 0U : Presentation.bPresentationMode ? 1U : ViewportState.ViewportPanels.size();
    }

    glm::vec4 FDebugUI::GetViewportRectNormalized(std::size_t Index) const noexcept
    {
        if (Presentation.bPresentationMode)
            return Index == 0 ? glm::vec4(0.0F, 0.0F, 1.0F, 1.0F) : glm::vec4(0.0F);
        return Index < ViewportState.ViewportPanels.size() ? ViewportState.ViewportPanels[Index].RectNormalized
                                                           : glm::vec4(0.0F);
    }

    const FCamera &FDebugUI::GetViewportCamera(const FScene &SceneData, std::size_t Index) const noexcept
    {
        (void)Index;
        return SceneData.GetMainCamera();
    }

    FCamera &FDebugUI::GetViewportCamera(FScene &SceneData, std::size_t Index) noexcept
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

    void FDebugUI::ApplyCameraViewPreset(FScene &SceneData, std::size_t Index)
    {
        if (!Gizmo.SelectedObject || *Gizmo.SelectedObject >= SceneData.GetStaticMeshInstances().size() ||
            Index >= CameraViewPresetPitchYawDegrees.size())
        {
            return;
        }

        FTransform &Transform = SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetTransform();
        glm::vec3   Target = Transform.Position;

        if (AssetManager != nullptr)
        {
            const Asset::FMeshAsset &Mesh =
                AssetManager->GetMesh(SceneData.GetStaticMeshInstances()[*Gizmo.SelectedObject].GetMesh());
            const auto &Vertices = Mesh.GetVertices();

            if (!Vertices.empty())
            {
                glm::vec3 Minimum(std::numeric_limits<float>::max());
                glm::vec3 Maximum(std::numeric_limits<float>::lowest());

                for (const Asset::FVertex &Vertex : Vertices)
                {
                    Minimum = glm::min(Minimum, Vertex.Position);
                    Maximum = glm::max(Maximum, Vertex.Position);
                }

                const glm::vec3 LocalBoundsCenter = (Minimum + Maximum) * 0.5F;
                Target = glm::vec3(Transform.GetMatrix() * glm::vec4(LocalBoundsCenter, 1.0F));
            }
        }

        FCamera        &Camera = GetViewportCamera(SceneData, GetActiveViewportIndex());
        const glm::vec3 ViewOffset = Camera.GetPosition() - Camera.GetTarget();
        const float     OffsetLength = glm::length(ViewOffset);
        const float     Distance = OffsetLength > 1.0e-6F ? OffsetLength : 3.0F;
        const float     Pitch = glm::radians(CameraViewPresetPitchYawDegrees[Index][0]);
        const float     Yaw = glm::radians(CameraViewPresetPitchYawDegrees[Index][1]);
        const glm::vec3 Forward{std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), std::sin(Pitch)};
        Camera.SetPosition(Target - Forward * Distance);
        Camera.SetTarget(Target);
    }

    const FCamera &FDebugUI::GetActiveViewportCamera(const FScene &SceneData) const noexcept
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
        const ImGuiIO &IO = ImGui::GetIO();
        return ViewportState.bMouseOverViewportOverlay || IO.WantTextInput ||
               (ViewportState.HoveredViewportId == 0 && (IO.WantCaptureKeyboard || ImGui::IsAnyItemActive() ||
                                                         ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)));
    }

#pragma endregion
} // namespace MDSS
