#include "pch.h"
#include "PlayerInteractSystem.h"
#include "Camera.h"
#include "Coordinator.h"
#include "ScriptingSystem.h"
#include "EventManager.h"
#include "Inventory.h" // Assuming you have this header
#include "BoxColliderComponent.h"
#include "ItemComponent.h"
#include "ScriptingSystem.h" // Assuming you have this
#include <iostream>

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    void PlayerInteractSystem::Initialize()
    {
        mPhysicsSystem = ecsInterface.GetSystem<PhysicsSystem>();
        //mTagManager = ecsInterface.GetSystem<TagManager>();
        mInputHandler = InputHandler::GetInstance();
        mGraphicsSystem = ecsInterface.GetSystem<Graphics>();
    }

    void PlayerInteractSystem::Update(float dt)
    {
        // 1. Get current state
        bool isEPressed = mInputHandler->IsKeyPressed(GLFW_KEY_E);

        // 2. Only fire if pressed NOW but wasn't pressed BEFORE
        if (isEPressed && !mWasEPressed)
        {
            TryInteract();
        }

        // 3. Update state for the next frame
        mWasEPressed = isEPressed;
    }

    void PlayerInteractSystem::TryInteract()
    {
        std::cout << "[Debug] TryInteract called..." << std::endl;

        Camera* gameCamera = mGraphicsSystem ? mGraphicsSystem->getCamera() : nullptr;
        if (!gameCamera) return;

        glm::vec3 origin = gameCamera->position;
        glm::vec3 direction = glm::normalize(gameCamera->front);
        Framework::RaycastHit hit;

        // 1. Check Raycast
        bool didHit = mPhysicsSystem->Raycast(origin, direction, mInteractRange, hit, 1);

        if (!didHit) {
            std::cout << "[Debug] Raycast MISSED everything (Category 1)." << std::endl;
            return;
        }

        std::cout << "[Debug] Raycast HIT Entity: " << hit.entity << std::endl;

        // 2. Check Script Override
        bool handledByScript = false;
        auto scripting = ecsInterface.GetSystem<ScriptingSystem>();

        if (scripting && ecsInterface.HasComponent<ScriptProperties>(hit.entity))
        {
            std::cout << "[Debug] Entity has ScriptComponent. Attempting 'OnPickup'..." << std::endl;

            // This will print errors inside ScriptingSystem if function is missing
            handledByScript = scripting->CallEntityFunctionRetBool(hit.entity, "OnPickup", hit.entity);

            if (handledByScript) {
                std::cout << "[Debug] SUCCESS: Lua Script handled the interaction." << std::endl;
                return;
            }
            else {
                std::cout << "[Debug] Script found, but 'OnPickup' returned false or wasn't found." << std::endl;
            }
        }
        else {
            std::cout << "[Debug] Entity does NOT have a ScriptComponent." << std::endl;
        }

        // 3. Check Fallback
        if (ecsInterface.HasComponent<ItemComponent>(hit.entity))
        {
            std::cout << "[Debug] Found ItemComponent. Attempting generic pickup..." << std::endl;
            // ... generic logic ...
            std::cout << "[Debug] Generic Pickup Success." << std::endl;
        }
        else {
            std::cout << "[Debug] Entity has no ItemComponent. Interaction ignored." << std::endl;
        }
    }
}