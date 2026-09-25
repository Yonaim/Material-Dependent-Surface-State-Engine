/**
 * @file DebugUI.cpp
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#include "DebugUI/DebugUI.h"

#include "Application/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/Swapchain.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "VulkanContext/Vulkan/VulkanQueue.h"
#include "VulkanContext/VulkanContext.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <stdexcept>
#include <string>

namespace MDSS
{
    namespace
    {
        ImVec4 GetLogColor(LogLevel Level)
        {
            switch (Level)
            {
                case LogLevel::Verbose:
                    return {0.55F, 0.55F, 0.55F, 1.0F}; // gray
                case LogLevel::Debug:
                    return {1.00F, 1.00F, 1.00F, 1.0F}; // white
                case LogLevel::Info:
                    return {0.45F, 0.78F, 1.00F, 1.0F}; // light blue
                case LogLevel::Warning:
                    return {1.00F, 0.84F, 0.25F, 1.0F}; // yellow
                case LogLevel::Error:
                    return {1.00F, 0.30F, 0.30F, 1.0F}; // red
                case LogLevel::Count:
                    break;
            }
            return {1.0F, 1.0F, 1.0F, 1.0F};
        }

        constexpr std::array<LogLevel, 5> DisplayedLevels = {
            LogLevel::Verbose,
            LogLevel::Debug,
            LogLevel::Info,
            LogLevel::Warning,
            LogLevel::Error,
        };

        constexpr std::array<const char*, 5> RenderViewModeNames = {
            "Lit",
            "Unlit",
            "Vertex Normal (World Space)",
            "Normal Texture (Tangent Space)",
            "Mapped Normal (World Space)",
        };
    } // namespace

    DebugUI::DebugUI(const VulkanContext& Context, const Window& Window, Renderer& Renderer)
        : Device(Context.GetDevice()), NativeWindow(Window.GetNativeHandle()), FrameRenderer(&Renderer)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& IO = ImGui::GetIO();
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        IO.IniFilename = nullptr;

        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForVulkan(Window.GetNativeHandle(), true))
        {
            ImGui::DestroyContext();
            throw std::runtime_error("Failed to initialize Dear ImGui GLFW backend.");
        }

        const std::uint32_t ImageCount = static_cast<std::uint32_t>(Renderer.GetSwapchain().GetImages().size());
        const SwapchainSupportDetails Support =
            Swapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
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

        Logger::Info("DebugUI", "Dear ImGui initialized with GLFW/Vulkan backends.");
        Logger::Debug("DebugUI",
                      "Camera, render options, normal debug views, and five-level log filtering are active.");
        CachedLogEntries = Logger::GetEntries();
        LastSeenLogRevision = Logger::GetRevision();
    }

    DebugUI::~DebugUI()
    {
        if (NativeWindow != nullptr && bRotatingCamera)
        {
            glfwSetInputMode(NativeWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }

        if (Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(Device);
        }

        Logger::Verbose("DebugUI", "Shutting down Dear ImGui backends.");
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void DebugUI::BeginFrame(Scene& SceneData)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ProcessCameraInput(SceneData);
        DrawCameraWindow(SceneData);
        DrawRenderOptionsWindow();
        DrawLogWindow();

        ImGui::Render();
    }

    void DebugUI::Render(VkCommandBuffer CommandBuffer) const
    {
        ImDrawData* DrawData = ImGui::GetDrawData();
        if (DrawData != nullptr && DrawData->CmdListsCount > 0)
        {
            ImGui_ImplVulkan_RenderDrawData(DrawData, CommandBuffer);
        }
    }

    void DebugUI::OnSwapchainRecreated(const VulkanContext& Context, const Renderer& Renderer)
    {
        const SwapchainSupportDetails Support =
            Swapchain::QuerySupport(Context.GetPhysicalDevice(), Context.GetSurface());
        const std::uint32_t MinImageCount = std::max(2U, Support.Capabilities.minImageCount);
        ImGui_ImplVulkan_SetMinImageCount(MinImageCount);

        Logger::Debug("DebugUI",
                      "ImGui Vulkan backend updated after swapchain recreation (images=" +
                          std::to_string(Renderer.GetSwapchain().GetImages().size()) + ").");
    }

    void DebugUI::ProcessCameraInput(Scene& SceneData)
    {
        ImGuiIO& IO = ImGui::GetIO();
        Camera&  CameraData = SceneData.GetMainCamera();

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
                constexpr float DollyDistancePerStep = 0.35F;
                const glm::vec3 DollyDelta = glm::normalize(ViewDirection) * IO.MouseWheel * DollyDistancePerStep;
                CameraData.SetPosition(CameraData.GetPosition() + DollyDelta);
                CameraData.SetTarget(CameraData.GetTarget() + DollyDelta);
            }
        }

        if (IO.WantCaptureKeyboard)
        {
            return;
        }

        const glm::vec3 ViewDirection = CameraData.GetTarget() - CameraData.GetPosition();
        if (glm::dot(ViewDirection, ViewDirection) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Forward = glm::normalize(ViewDirection);
        const glm::vec3 WorldUp{0.0F, 1.0F, 0.0F};
        const glm::vec3 RightVector = glm::cross(Forward, WorldUp);
        if (glm::dot(RightVector, RightVector) <= 1.0e-12F)
        {
            return;
        }

        const glm::vec3 Right = glm::normalize(RightVector);
        glm::vec3       MoveDirection{0.0F};

        if (ImGui::IsKeyDown(ImGuiKey_W))
        {
            MoveDirection += Forward;
        }
        if (ImGui::IsKeyDown(ImGuiKey_S))
        {
            MoveDirection -= Forward;
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

    void DebugUI::DrawCameraWindow(Scene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    CameraWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{CameraWidth, 215.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - WindowSize.x - 10.0F,
                                    Viewport->WorkPos.y + 10.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Camera", nullptr, Flags))
        {
            Camera& CameraData = SceneData.GetMainCamera();

            glm::vec3 Position = CameraData.GetPosition();
            if (ImGui::DragFloat3("Position", &Position.x, 0.05F))
            {
                const glm::vec3 Delta = Position - CameraData.GetPosition();
                const glm::vec3 ShiftedTarget = CameraData.GetTarget() + Delta;
                CameraData.SetPosition(Position);
                CameraData.SetTarget(ShiftedTarget);
            }

            glm::vec2 RotationDegrees = CameraData.GetRotationDegrees();
            if (ImGui::DragFloat2("Angle (Pitch/Yaw)", &RotationDegrees.x, 0.25F))
            {
                CameraData.SetRotationDegrees(RotationDegrees);
            }

            float FieldOfViewDegrees = CameraData.GetVerticalFieldOfViewDegrees();
            if (ImGui::SliderFloat("FOV", &FieldOfViewDegrees, 20.0F, 120.0F, "%.1f deg"))
            {
                CameraData.SetVerticalFieldOfViewDegrees(FieldOfViewDegrees);
            }

            ImGui::Separator();
            ImGui::TextDisabled("RMB + Mouse: look  |  WASD: move");
            ImGui::TextDisabled("Shift: faster  |  Wheel: dolly");
        }
        ImGui::End();
    }

    void DebugUI::DrawRenderOptionsWindow()
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    WindowWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{WindowWidth, 250.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - WindowSize.x - 10.0F,
                                    Viewport->WorkPos.y + 235.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Render Options", nullptr, Flags))
        {
            int SelectedMode = static_cast<int>(FrameRenderer->GetRenderViewMode());
            if (ImGui::Combo("View Mode",
                             &SelectedMode,
                             RenderViewModeNames.data(),
                             static_cast<int>(RenderViewModeNames.size())))
            {
                FrameRenderer->SetRenderViewMode(static_cast<RenderViewMode>(SelectedMode));
            }

            bool bFlipNormalY = FrameRenderer->GetFlipNormalY();
            if (ImGui::Checkbox("Flip Normal Y", &bFlipNormalY))
            {
                FrameRenderer->SetFlipNormalY(bFlipNormalY);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip(
                    "Useful when the source normal-map Y convention differs from the engine tangent basis.");
            }

            float NormalStrength = FrameRenderer->GetNormalStrength();
            if (ImGui::SliderFloat("Normal Strength", &NormalStrength, 0.0F, 4.0F, "%.2f"))
            {
                FrameRenderer->SetNormalStrength(NormalStrength);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Controls normal-map depth. 0 is flat, 1 uses the original strength.");
            }

            float AmbientLight = FrameRenderer->GetAmbientLight();
            if (ImGui::SliderFloat("Ambient Light", &AmbientLight, 0.0F, 1.0F, "%.2f"))
            {
                FrameRenderer->SetAmbientLight(AmbientLight);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Sets the minimum brightness on surfaces facing away from the light.");
            }

            ImGui::Separator();
            ImGui::TextWrapped("Normal Texture (TS) shows the decoded normal map. Mapped Normal (WS) shows the result "
                               "after TBN conversion.");
        }
        ImGui::End();
    }

    void DebugUI::DrawLogWindow()
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    MaxLogHeight = std::max(120.0F, Viewport->WorkSize.y * 0.80F);
        LogWindowHeight = std::clamp(LogWindowHeight, 120.0F, MaxLogHeight);

        const ImVec2 WindowPosition{Viewport->WorkPos.x,
                                    Viewport->WorkPos.y + std::max(0.0F, Viewport->WorkSize.y - LogWindowHeight)};
        const ImVec2 WindowSize{std::max(Viewport->WorkSize.x, 1.0F),
                                std::min(LogWindowHeight, std::max(Viewport->WorkSize.y, 1.0F))};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Log", nullptr, Flags))
        {
            // A dedicated top-edge splitter keeps the log anchored to the bottom while allowing arbitrary height
            // changes.
            const ImVec2 ResizeStart = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton("##LogHeightResize", ImVec2(-1.0F, 6.0F));
            const ImVec2 ResizeEnd = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(ImVec2(ResizeStart.x, ResizeStart.y + 3.0F),
                                                ImVec2(ResizeEnd.x, ResizeStart.y + 3.0F),
                                                ImGui::GetColorU32(ImGuiCol_Separator),
                                                2.0F);
            if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Drag to resize the log window height.");
            }
            if (ImGui::IsItemActive())
            {
                LogWindowHeight = std::clamp(LogWindowHeight - ImGui::GetIO().MouseDelta.y, 120.0F, MaxLogHeight);
            }

            const std::uint64_t CurrentRevision = Logger::GetRevision();
            if (CurrentRevision != LastSeenLogRevision)
            {
                CachedLogEntries = Logger::GetEntries();
                bScrollLogToBottom = true;
                LastSeenLogRevision = CurrentRevision;
            }

            if (ImGui::Button("Clear"))
            {
                Logger::Clear();
                bScrollLogToBottom = true;
            }

            ImGui::SameLine();
            if (ImGui::Button("Copy Visible"))
            {
                std::string ClipboardText;
                for (const LogEntry& Entry : CachedLogEntries)
                {
                    const std::size_t Index = static_cast<std::size_t>(Entry.Level);
                    if (Index < LogLevelFilters.size() && LogLevelFilters[Index])
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

            for (const LogLevel Level : DisplayedLevels)
            {
                ImGui::SameLine();
                const std::size_t Index = static_cast<std::size_t>(Level);
                ImGui::PushStyleColor(ImGuiCol_Text, GetLogColor(Level));
                ImGui::Checkbox(Logger::GetLevelName(Level), &LogLevelFilters[Index]);
                ImGui::PopStyleColor();
            }

            ImGui::Separator();

            ImGui::BeginChild(
                "LogMessages", ImVec2(0.0F, 0.0F), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            for (const LogEntry& Entry : CachedLogEntries)
            {
                const std::size_t Index = static_cast<std::size_t>(Entry.Level);
                if (Index >= LogLevelFilters.size() || !LogLevelFilters[Index])
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
} // namespace MDSS
