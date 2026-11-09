/**
 * @file InputSystem.cpp
 * @brief 장치 입력과 디버그 입력을 Surface 접촉 이벤트로 변환한다.
 */

#include "InputSystem/InputSystem.h"

#include "AssetManager/Core/AssetManager.h"
#include "InputSystem/Raycaster.h"
#include "Logger/Logger.h"
#include "Scene/Camera.h"
#include "Scene/Scene.h"
#include "Scene/StaticMeshInstance.h"
#include "SurfaceState/Runtime/SurfaceDataManager.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>
#include <limits>

namespace MDSS
{
// ============================================================
// 00. FInputSystem Implementation
// ============================================================
#pragma region 00_TInputSystem_Implementation

    FInputSystem::FInputSystem(GLFWwindow* Window) noexcept : Window(Window)
    {
    }

    std::optional<SurfaceState::FSurfaceContactInput>
    FInputSystem::PollDebugContact(const FScene&                            Scene,
                                   const Asset::FAssetManager&              Assets,
                                   const SurfaceState::FSurfaceDataManager& SurfaceData,
                                   const FCamera&                           Camera,
                                   bool                                     bInjectMode,
                                   SurfaceState::FStateId                   State,
                                   float                                    Strength,
                                   float                                    Radius,
                                   float                                    Falloff,
                                   SurfaceState::ESurfaceContactOperation   Operation,
                                   SurfaceState::ESurfaceContactShape       Shape,
                                   float                                    SecondaryRadius,
                                   float                                    RotationDegrees,
                                   std::uint32_t                            TexelSearchRadius,
                                   bool                                     bHotkeySuppressed)
    {
        const bool bSpaceDown = Window != nullptr && glfwGetKey(Window, GLFW_KEY_SPACE) == GLFW_PRESS;
        const bool bSpacePressed = bSpaceDown && !bWasSpaceDown;
        bWasSpaceDown = bSpaceDown;

        if (!bSpacePressed || !bInjectMode || bHotkeySuppressed)
        {
            return std::nullopt;
        }
        if (State == SurfaceState::InvalidStateId || Strength < 0.0F || Radius <= 0.0F || Falloff < 0.0F)
        {
            FLogger::Warning("TInputSystem",
                             "Ignored debug contact because State, Strength, radius, or falloff is invalid.");
            return std::nullopt;
        }

        const glm::vec3 Direction = Camera.GetTarget() - Camera.GetPosition();
        if (glm::dot(Direction, Direction) <= 1.0e-12F)
        {
            FLogger::Warning("TInputSystem", "Ignored debug contact because the camera has no forward direction.");
            return std::nullopt;
        }

        const FSurfaceRayHit Hit = FRaycaster::Cast(Scene, Assets, Camera.GetPosition(), Direction);
        if (!Hit.bHit)
        {
            FLogger::Warning("TInputSystem", "Debug contact missed all front-facing mesh triangles.");
            return std::nullopt;
        }
        if (Hit.InstanceIndex >= Scene.GetStaticMeshInstances().size() ||
            Hit.InstanceIndex >= static_cast<std::size_t>(SurfaceState::InvalidSurfaceInstanceID))
        {
            FLogger::Error("TInputSystem", "Raycast returned an invalid Surface instance index.");
            return std::nullopt;
        }

        const FStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[Hit.InstanceIndex];
        if (!SurfaceData.HasSurfaceData(Instance.GetSurfaceData()))
        {
            FLogger::Warning("TInputSystem", "Raycast hit a Mesh instance without Surface simulation data.");
            return std::nullopt;
        }

        SurfaceState::FSurfaceContactInput Contact;
        Contact.TargetInstance = static_cast<SurfaceState::FSurfaceInstanceID>(Hit.InstanceIndex);
        Contact.State = State;
        Contact.WorldPosition = Hit.WorldPosition;
        Contact.WorldDirection = glm::normalize(Direction);
        Contact.Radius = Radius;
        Contact.Strength = Strength;
        Contact.Falloff = Falloff;
        Contact.Operation = Operation;
        Contact.Shape = Shape;
        Contact.SecondaryRadius = SecondaryRadius;
        Contact.RotationDegrees = RotationDegrees;
        if (Shape != SurfaceState::ESurfaceContactShape::Circle)
        {
            Contact.bPlanar = true;
            Contact.PlaneNormal = Hit.WorldNormal;
            Contact.PlaneHalfDepth = Radius;
        }
        Contact.TexelSearchRadius = TexelSearchRadius;
        Contact.bHasSimulationMapping = true;
        Contact.TargetTriangle = Hit.TriangleID;
        Contact.SimulationUV = Hit.SimulationUV;
        return Contact;
    }
#pragma endregion
} // namespace MDSS
