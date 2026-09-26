/**
 * @file InputSystem.cpp
 * @brief Device/debug input adapters that produce Surface contact events.
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
    namespace
    {
        constexpr float DefaultContactRadius = 0.25F;
    }

    TInputSystem::TInputSystem(GLFWwindow* Window) noexcept : Window(Window)
    {
    }

    std::optional<TSurfaceContactInput> TInputSystem::PollDebugContact(const TScene&       Scene,
                                                                       const TAssetManager& Assets,
                                                                       const TCamera&       Camera,
                                                                       bool                 bInjectMode,
                                                                       TStateId             State,
                                                                       float                Strength,
                                                                       bool                 bKeyboardCaptured)
    {
        const bool bSpaceDown = Window != nullptr && glfwGetKey(Window, GLFW_KEY_SPACE) == GLFW_PRESS;
        const bool bSpacePressed = bSpaceDown && !bWasSpaceDown;
        bWasSpaceDown = bSpaceDown;

        if (!bSpacePressed || !bInjectMode || bKeyboardCaptured)
        {
            return std::nullopt;
        }
        if (State == InvalidStateId || Strength < 0.0F)
        {
            TLogger::Warning("TInputSystem", "Ignored debug contact because State or Strength is invalid.");
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
        Contact.Radius = DefaultContactRadius;
        Contact.Strength = Strength;
        Contact.Falloff = 1.0F;
        Contact.bHasSimulationMapping = true;
        Contact.TargetTriangle = Hit.TriangleID;
        Contact.SimulationUV = Hit.SimulationUV;
        return Contact;
    }
} // namespace MDSS
