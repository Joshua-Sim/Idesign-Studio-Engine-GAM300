#include "pch.h"
#include "CameraSystem.h"
#include "Coordinator.h"
#include "Graphics.h"
#include "TransformProperties.h"
#include "CameraComponent.h"

extern Framework::Coordinator ecsInterface;

void CameraSystem::Update(float dt)
{
    Entity activeCam = (Entity)-1;  

    for (auto e : mEntities)
    {
        auto& cam = ecsInterface.GetComponent<CameraComponent>(e);
        if (cam.active)
        {
            activeCam = e;
            break; // Only one active camera needed
        }
    }

    if (activeCam == (Entity)-1)
        return;

    auto& tr = ecsInterface.GetComponent<TransformComponent>(activeCam);
    auto& cam = ecsInterface.GetComponent<CameraComponent>(activeCam);

    // --- Apply mouse look ---
    double dx, dy;
    Graphics::GetMouseDelta(dx, dy);

    cam.yaw += (float)dx * cam.sensitivity;
    cam.pitch -= (float)dy * cam.sensitivity;
    cam.pitch = glm::clamp(cam.pitch, -89.f, 89.f);

    // --- Derive camera forward vector ---
    glm::vec3 forward(
        cos(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch)),
        sin(glm::radians(cam.pitch)),
        sin(glm::radians(cam.yaw)) * cos(glm::radians(cam.pitch))
    );
    forward = glm::normalize(forward);

    glm::vec3 camPos;

    // FIRST PERSON
    if (cam.distance == 0)
    {
        camPos = tr.position + glm::vec3(0, cam.heightOffset, 0);
    }
    // THIRD PERSON (orbit camera)
    else
    {
        camPos =
            tr.position +
            glm::vec3(0, cam.heightOffset, 0) -
            forward * cam.distance;
    }

    // Push into Graphics camera
    Graphics::camera.position = camPos;
    Graphics::camera.front = forward;
    Graphics::camera.updateVectors();
}
