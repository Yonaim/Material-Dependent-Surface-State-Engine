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

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glm/geometric.hpp>

#include <limits>

namespace MDSS
{
    TInputSystem::TInputSystem(GLFWwindow* Window) noexcept : Window(Window)
    {
    }

    std::optional<TSurfaceContactInput> TInputSystem::PollDebugContact(const TScene&       Scene,
                                                                       const TAssetManager& Assets,
                                                                       const TCamera&       Camera,
                                                                       bool                 bInjectMode,
                                                                       TStateId             State,
                                                                       float                Strength,
                                                                       float                Radius,
                                                                       float                Falloff,
                                                                       std::uint32_t        TexelSearchRadius,
                                                                       bool                 bHotkeySuppressed)
    {
        const bool bSpaceDown = Window != nullptr && glfwGetKey(Window, GLFW_KEY_SPACE) == GLFW_PRESS;
        const bool bSpacePressed = bSpaceDown && !bWasSpaceDown;
        bWasSpaceDown = bSpaceDown;

        if (!bSpacePressed || !bInjectMode || bHotkeySuppressed)
        {
            return std::nullopt;
        }
        if (State == InvalidStateId || Strength < 0.0F || Radius <= 0.0F || Falloff < 0.0F)
        {
            TLogger::Warning("TInputSystem", "Ignored debug contact because State, Strength, radius, or falloff is invalid.");
            return std::nullopt;
        }

        const glm::vec3 Direction = Camera.GetTarget() - Camera.GetPosition();
        if (glm::dot(Direction, Direction) <= 1.0e-12F)
        {
            TLogger::Warning("TInputSystem", "Ignored debug contact because the camera has no forward direction.");
            return std::nullopt;
        }

        const TSurfaceRayHit Hit = TRaycaster::Cast(Scene, Assets, Camera.GetPosition(), Direction);
        if (!Hit.Hit)
        {
            TLogger::Warning("TInputSystem", "Debug contact missed all front-facing mesh triangles.");
            return std::nullopt;
        }
        if (Hit.InstanceIndex >= Scene.GetStaticMeshInstances().size() ||
            Hit.InstanceIndex >= static_cast<std::size_t>(InvalidSurfaceInstanceID))
        {
            TLogger::Error("TInputSystem", "Raycast returned an invalid Surface instance index.");
            return std::nullopt;
        }

        const TStaticMeshInstance& Instance = Scene.GetStaticMeshInstances()[Hit.InstanceIndex];
        if (!Assets.HasSurfaceData(Instance.GetSurfaceData()))
        {
            TLogger::Warning("TInputSystem", "Raycast hit a Mesh instance without Surface simulation data.");
            return std::nullopt;
        }

        TSurfaceContactInput Contact;
        Contact.TargetInstance = static_cast<TSurfaceInstanceID>(Hit.InstanceIndex);
        Contact.State = State;
        Contact.WorldPosition = Hit.WorldPosition;
        Contact.WorldDirection = glm::normalize(Direction);
        Contact.Radius = Radius;
        Contact.Strength = Strength;
        Contact.Falloff = Falloff;
        Contact.TexelSearchRadius = TexelSearchRadius;
        Contact.bHasSimulationMapping = true;
        Contact.TargetTriangle = Hit.TriangleID;
        Contact.SimulationUV = Hit.SimulationUV;
        return Contact;
    }
} // namespace MDSS
