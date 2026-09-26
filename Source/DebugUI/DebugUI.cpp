/**
 * @file DebugUI.cpp
 * @brief ImGui 기반 카메라·렌더 설정과 로그 진단 UI.
 */

#include "DebugUI/DebugUI.h"

#include "Application/Window.h"
#include "Application/SceneFileDialog.h"
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
#include <cstdint>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <imgui.h>
#include <stdexcept>
#include <string>
#include <filesystem>
#include <exception>
#include <vector>
#include <cmath>
#include <limits>

namespace MDSS
{
    namespace
    {
        ImVec4 GetLogColor(TLogLevel Level)
        {
            switch (Level)
            {
                case TLogLevel::Verbose:
                    return {0.55F, 0.55F, 0.55F, 1.0F}; // gray
                case TLogLevel::Debug:
                    return {1.00F, 1.00F, 1.00F, 1.0F}; // white
                case TLogLevel::Info:
                    return {0.45F, 0.78F, 1.00F, 1.0F}; // light blue
                case TLogLevel::Warning:
                    return {1.00F, 0.84F, 0.25F, 1.0F}; // yellow
                case TLogLevel::Error:
                    return {1.00F, 0.30F, 0.30F, 1.0F}; // red
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

        bool ProjectToScreen(glm::vec3 World, const glm::mat4& VP, ImVec2 Size, ImVec2& Screen, float* Depth = nullptr)
        {
            const glm::vec4 Clip = VP * glm::vec4(World, 1.0F);
            if (Clip.w <= 1.0e-5F) return false;
            const glm::vec3 NDC = glm::vec3(Clip) / Clip.w;
            if (Depth != nullptr) *Depth = NDC.z;
            Screen = {((NDC.x + 1.0F) * 0.5F) * Size.x, ((NDC.y + 1.0F) * 0.5F) * Size.y};
            return NDC.z >= 0.0F && NDC.z <= 1.0F;
        }

        float PointSegmentDistance(ImVec2 P, ImVec2 A, ImVec2 B, float& Along)
        {
            const float DX = B.x - A.x, DY = B.y - A.y;
            const float Denominator = DX * DX + DY * DY;
            if (Denominator < 1.0e-6F) { Along = 0.0F; return std::hypot(P.x - A.x, P.y - A.y); }
            Along = std::clamp(((P.x - A.x) * DX + (P.y - A.y) * DY) / Denominator, 0.0F, 1.0F);
            return std::hypot(P.x - A.x - Along * DX, P.y - A.y - Along * DY);
        }
    } // namespace

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
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        IO.IniFilename = nullptr;

        ImGui::StyleColorsDark();

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

        DrawSceneWindow(SceneData);
        DrawCameraWindow(SceneData);
        DrawSelectedTransformWindow(SceneData);
        DrawRenderOptionsWindow();
        DrawInjectWindow();
        DrawLogWindow();
        ProcessCameraInput(SceneData);
        ProcessSelectionAndGizmo(SceneData);

        if (bInjectMode)
        {
            ImGuiViewport* Viewport = ImGui::GetMainViewport();
            const ImVec2   Center{Viewport->WorkPos.x + Viewport->WorkSize.x * 0.5F,
                                Viewport->WorkPos.y + Viewport->WorkSize.y * 0.5F};
            constexpr float CrosshairHalfSize = 9.0F;
            constexpr ImU32 CrosshairColor = IM_COL32(255, 255, 255, 230);
            ImDrawList* DrawList = ImGui::GetForegroundDrawList();
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

    std::optional<std::size_t> TDebugUI::GetSelectedObject() const noexcept
    {
        return SelectedObject;
    }

    TStateId TDebugUI::GetDebugState() const noexcept
    {
        return DebugState;
    }

    bool TDebugUI::IsKeyboardCaptured() const noexcept
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    void TDebugUI::DrawSceneWindow(TScene& SceneData)
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({Viewport->WorkPos.x + 350.0F, Viewport->WorkPos.y + 10.0F}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({330.0F, 92.0F}, ImGuiCond_Always);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar;
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
        ImGuiIO& IO = ImGui::GetIO();
        const ImVec2 DisplaySize = IO.DisplaySize;
        if (DisplaySize.x <= 0.0F || DisplaySize.y <= 0.0F)
        {
            return;
        }
        const float Aspect = DisplaySize.x / DisplaySize.y;
        const glm::mat4 VP = SceneData.GetMainCamera().GetViewProjectionMatrix(Aspect);
        const glm::vec3 Origin = SceneData.GetMainCamera().GetPosition();
        const ImVec2 Mouse = IO.MousePos;

        auto GetAxisScreenSegment = [&](glm::vec3 Position, int Axis, ImVec2& A, ImVec2& B, float& WorldScale)
        {
            const glm::vec3 AxisVector = Axis == 0 ? glm::vec3(1, 0, 0) :
                                         Axis == 1 ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
            WorldScale = glm::length(Origin - Position) * 0.18F;
            if (WorldScale <= 0.01F || !ProjectToScreen(Position, VP, DisplaySize, A) ||
                !ProjectToScreen(Position + AxisVector * WorldScale, VP, DisplaySize, B))
            {
                return false;
            }
            return true;
        };

        if (ActiveGizmoAxis >= 0)
        {
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
                    const float WorldDelta = glm::dot(MouseDelta, GizmoDragScreenAxis) *
                                             (GizmoDragWorldScale / GizmoDragPixelLength);
                    const glm::vec3 AxisVector = ActiveGizmoAxis == 0 ? glm::vec3(1, 0, 0) :
                                                 ActiveGizmoAxis == 1 ? glm::vec3(0, 1, 0) : glm::vec3(0, 0, 1);
                    SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position =
                        GizmoDragStartPosition + AxisVector * WorldDelta;
                }
            }
            return;
        }

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
            ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive() || bInjectMode)
        {
            return;
        }

        if (SelectedObject && *SelectedObject < SceneData.GetStaticMeshInstances().size())
        {
            const glm::vec3 Position = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform().Position;
            float BestDistance = 13.0F;
            int BestAxis = -1;
            for (int Axis = 0; Axis < 3; ++Axis)
            {
                ImVec2 A{}, B{};
                float WorldScale = 0.0F, Along = 0.0F;
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
                float WorldScale = 0.0F;
                if (GetAxisScreenSegment(Transform.Position, BestAxis, A, B, WorldScale))
                {
                    const glm::vec2 ScreenAxis{B.x - A.x, B.y - A.y};
                    GizmoDragPixelLength = glm::length(ScreenAxis);
                    GizmoDragScreenAxis = GizmoDragPixelLength > 0.0F
                                              ? ScreenAxis / GizmoDragPixelLength
                                              : glm::vec2(0.0F);
                    GizmoDragWorldScale = WorldScale;
                }
                return;
            }
        }

        const glm::vec2 NDC{Mouse.x / DisplaySize.x * 2.0F - 1.0F,
                            Mouse.y / DisplaySize.y * 2.0F - 1.0F};
        const glm::mat4 InvVP = glm::inverse(VP);
        glm::vec4 FarPoint = InvVP * glm::vec4(NDC, 1.0F, 1.0F);
        if (std::abs(FarPoint.w) <= 1.0e-6F)
        {
            SelectedObject.reset();
            return;
        }
        FarPoint /= FarPoint.w;
        const TSurfaceRayHit Hit = TRaycaster::Cast(SceneData,
                                                    *AssetManager,
                                                    Origin,
                                                    glm::vec3(FarPoint) - Origin);
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
        const ImVec2 Position{Viewport->WorkPos.x + Viewport->WorkSize.x - 340.0F,
                              Viewport->WorkPos.y + 190.0F};
        ImGui::SetNextWindowPos(Position, ImGuiCond_Always);
        ImGui::SetNextWindowSize({330.0F, 170.0F}, ImGuiCond_Always);
        constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                           ImGuiWindowFlags_NoMove;
        ImGui::Begin("Selected Transform", nullptr, Flags);
        TTransform& Transform = SceneData.GetStaticMeshInstances()[*SelectedObject].GetTransform();
        ImGui::Text("Object %zu", *SelectedObject);
        ImGui::DragFloat3("Position", &Transform.Position.x, 0.02F);
        ImGui::DragFloat3("Rotation", &Transform.RotationDegrees.x, 0.25F);
        ImGui::DragFloat3("Scale", &Transform.Scale.x, 0.02F);
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
                constexpr float DollyDistancePerStep = 0.35F;
                const glm::vec3 DollyDelta = glm::normalize(ViewDirection) * IO.MouseWheel * DollyDistancePerStep;
                CameraData.SetPosition(CameraData.GetPosition() + DollyDelta);
                CameraData.SetTarget(CameraData.GetTarget() + DollyDelta);
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

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Camera", nullptr, Flags))
        {
            TCamera& CameraData = SceneData.GetMainCamera();

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

    void TDebugUI::DrawRenderOptionsWindow()
    {
        if (FrameRenderer == nullptr)
        {
            return;
        }

        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    AvailableWidth = std::max(Viewport->WorkSize.x, 1.0F);
        const float    WindowWidth = std::min(330.0F, std::max(240.0F, AvailableWidth - 20.0F));
        const ImVec2   WindowSize{WindowWidth, 320.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + 10.0F, Viewport->WorkPos.y + 235.0F};

        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

        if (ImGui::Begin("Render Options", nullptr, Flags))
        {
            constexpr std::array<const char*, 2> ViewGroups = {"Rendering", "Surface Debug"};
            TRenderViewMode CurrentMode = FrameRenderer->GetRenderViewMode();
            int SelectedGroup = CurrentMode >= TRenderViewMode::SurfaceStateHeatmap ? 1 : 0;
            if (ImGui::Combo("View Group", &SelectedGroup, ViewGroups.data(), static_cast<int>(ViewGroups.size())))
            {
                const TRenderViewMode FirstMode = SelectedGroup == 0 ? TRenderViewMode::Lit
                                                                      : TRenderViewMode::SurfaceStateHeatmap;
                FrameRenderer->SetRenderViewMode(FirstMode);
                CurrentMode = FirstMode;
            }

            if (SelectedGroup == 0)
            {
                int SelectedMode = static_cast<int>(CurrentMode);
                if (ImGui::Combo("View Mode",
                                 &SelectedMode,
                                 RenderViewModeNames.data(),
                                 static_cast<int>(RenderViewModeNames.size())))
                {
                    FrameRenderer->SetRenderViewMode(static_cast<TRenderViewMode>(SelectedMode));
                }
            }
            else
            {
                int SelectedMode = static_cast<int>(CurrentMode) -
                                   static_cast<int>(TRenderViewMode::SurfaceStateHeatmap);
                if (SelectedMode < 0 || SelectedMode >= static_cast<int>(SurfaceDebugViewNames.size()))
                {
                    SelectedMode = 0;
                }
                if (ImGui::Combo("Debug View",
                                 &SelectedMode,
                                 SurfaceDebugViewNames.data(),
                                 static_cast<int>(SurfaceDebugViewNames.size())))
                {
                    FrameRenderer->SetRenderViewMode(static_cast<TRenderViewMode>(
                        static_cast<int>(TRenderViewMode::SurfaceStateHeatmap) + SelectedMode));
                }
            }

            const TSurfaceStateRegistry* Registry =
                AssetManager != nullptr ? &AssetManager->GetSurfaceStateRegistry() : nullptr;
            const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;
            if (FrameRenderer->GetRenderViewMode() == TRenderViewMode::SurfaceStateHeatmap)
            {
                if (StateCount == 0)
                {
                    ImGui::TextDisabled("No Surface States are registered.");
                }
                else
                {
                    if (DebugState >= StateCount)
                    {
                        DebugState = 0;
                    }
                    const std::string& SelectedName = Registry->GetStateName(DebugState);
                    if (ImGui::BeginCombo("State channel", SelectedName.c_str()))
                    {
                        for (std::size_t Index = 0; Index < StateCount; ++Index)
                        {
                            const TStateId State = static_cast<TStateId>(Index);
                            const std::string& Name = Registry->GetStateName(State);
                            const bool bSelected = State == DebugState;
                            if (ImGui::Selectable(Name.c_str(), bSelected))
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
                    ImGui::TextDisabled("Color shows State / Profile capacity (0 to 1).");
                    ImDrawList* DrawList = ImGui::GetWindowDrawList();
                    const ImVec2 BarMin = ImGui::GetCursorScreenPos();
                    const ImVec2 BarMax{BarMin.x + ImGui::GetContentRegionAvail().x, BarMin.y + 12.0F};
                    constexpr std::array<ImU32, 4> HeatmapColors = {
                        IM_COL32(19, 23, 66, 255),
                        IM_COL32(33, 92, 156, 255),
                        IM_COL32(31, 156, 138, 255),
                        IM_COL32(253, 230, 51, 255)};
                    const float SegmentWidth = (BarMax.x - BarMin.x) / static_cast<float>(HeatmapColors.size() - 1U);
                    for (std::size_t Segment = 0; Segment + 1U < HeatmapColors.size(); ++Segment)
                    {
                        const ImVec2 SegmentMin{BarMin.x + SegmentWidth * static_cast<float>(Segment), BarMin.y};
                        const ImVec2 SegmentMax{BarMin.x + SegmentWidth * static_cast<float>(Segment + 1U), BarMax.y};
                        DrawList->AddRectFilledMultiColor(SegmentMin,
                                                          SegmentMax,
                                                          HeatmapColors[Segment],
                                                          HeatmapColors[Segment + 1U],
                                                          HeatmapColors[Segment + 1U],
                                                          HeatmapColors[Segment]);
                    }
                    ImGui::Dummy({0.0F, 14.0F});
                    ImGui::Text("0  (empty)");
                    ImGui::SameLine();
                    ImGui::Text("1  (capacity)");
                }
            }
            else if (FrameRenderer->GetRenderViewMode() == TRenderViewMode::SurfaceValidity)
            {
                ImGui::TextDisabled("Green: valid texel   Red: invalid texel");
            }
            else if (FrameRenderer->GetRenderViewMode() == TRenderViewMode::SurfaceID)
            {
                ImGui::TextDisabled("Each Surface uses a stable display color.");
            }
            else if (FrameRenderer->GetRenderViewMode() == TRenderViewMode::NeighborCount)
            {
                ImGui::TextDisabled("Dark: 0 neighbors   Bright: 8 neighbors");
            }
            else if (FrameRenderer->GetRenderViewMode() == TRenderViewMode::SurfaceSeam)
            {
                ImGui::TextDisabled("Bright texels have a neighbor on another UV chart.");
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

    void TDebugUI::DrawInjectWindow()
    {
        ImGuiViewport* Viewport = ImGui::GetMainViewport();
        const float    Width = std::min(300.0F, std::max(240.0F, Viewport->WorkSize.x - 20.0F));
        const ImVec2   WindowSize{Width, 170.0F};
        const ImVec2   WindowPosition{Viewport->WorkPos.x + Viewport->WorkSize.x - Width - 10.0F,
                                    Viewport->WorkPos.y + 10.0F};
        ImGui::SetNextWindowPos(WindowPosition, ImGuiCond_Always);
        ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Always);

        constexpr ImGuiWindowFlags Flags =
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        if (ImGui::Begin("Inject", nullptr, Flags))
        {
            ImGui::Checkbox("Inject mode", &bInjectMode);
            ImGui::TextDisabled("Enable mode, aim with the crosshair, press Space.");

            const TSurfaceStateRegistry* Registry =
                AssetManager != nullptr ? &AssetManager->GetSurfaceStateRegistry() : nullptr;
            const std::size_t StateCount = Registry != nullptr ? Registry->GetStateCount() : 0;
            if (StateCount == 0)
            {
                ImGui::TextDisabled("No Surface States are registered.");
                InjectState = InvalidStateId;
            }
            else
            {
                if (InjectState >= StateCount)
                {
                    InjectState = 0;
                }
                const std::string& SelectedName = Registry->GetStateName(InjectState);
                if (ImGui::BeginCombo("State", SelectedName.c_str()))
                {
                    for (std::size_t Index = 0; Index < StateCount; ++Index)
                    {
                        const TStateId State = static_cast<TStateId>(Index);
                        const std::string& Name = Registry->GetStateName(State);
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
                    ImGui::EndCombo();
                }
            }

            ImGui::SliderFloat("Strength", &InjectStrength, 0.0F, 2.0F, "%.2f");
        }
        ImGui::End();
    }

    void TDebugUI::DrawLogWindow()
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
            if (ImGui::Button("Copy Visible"))
            {
                std::string ClipboardText;
                for (const TLogEntry& Entry : CachedLogEntries)
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
