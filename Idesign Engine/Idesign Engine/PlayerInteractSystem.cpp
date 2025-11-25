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
        // 1. Standard Raycast Setup
        Camera* gameCamera = mGraphicsSystem ? mGraphicsSystem->getCamera() : nullptr;
        if (!gameCamera) return;

        glm::vec3 origin = gameCamera->position;
        glm::vec3 direction = glm::normalize(gameCamera->front);
        Framework::RaycastHit hit;

        bool didHit = mPhysicsSystem->Raycast(origin, direction, mInteractRange, hit, 1);

        if (!didHit) return;

        Entity targetEntity = hit.entity;
        std::cout << "[Interact] Hit Entity: " << targetEntity << std::endl;

        // --- PRIORITY 1: EVENT TRIGGER (Buttons, Chests, Scripted Items) ---
        // Does the collider say "I am interactable"?
        if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity))
        {
            auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(targetEntity);

            for (const auto& entry : colliderComp.colliders)
            {
                // Check the boolean and ensure there is a message to send
                if (entry.isInteractable && !entry.interactMessage.empty())
                {
                    std::cout << "[Interact] Firing Event: " << entry.interactMessage << std::endl;

                    // Construct Payload
                    CollisionEventData payload;
                    payload.senderID = ecsInterface.GetEntityByName("Player");
                    payload.targetID = targetEntity; // The object being hit
                    payload.actionType = 1;          // 1 = Interact

                    // FIRE AND FORGET
                    // The EventSystem will pick this up and call the Lua script attached to the object
                    EventManager::Instance().Fire(entry.interactMessage, &payload);

                    return; // Stop here. The Event System handles the rest.
                }
            }
        }

        // --- PRIORITY 2: GENERIC ITEM FALLBACK (Rocks, Sticks) ---
        // If no event was fired, check if it's just a simple item to pick up silently.
        if (ecsInterface.HasComponent<ItemComponent>(targetEntity))
        {
            auto& item = ecsInterface.GetComponent<ItemComponent>(targetEntity);
            int itemId = Framework::ItemNameToId(item.name);

            if (itemId >= 0)
            {
                int slot = Framework::Inventory::Instance().AddFirstEmpty(itemId);
                if (slot >= 0) {
                    if (item.toDestroyWhenPicked) {
                        mPhysicsSystem->RemoveBodyForEntity(targetEntity);
                        ecsInterface.DestroyEntity(targetEntity);
                    }
                    std::cout << "[Interact] Generic Item Picked Up." << std::endl;
                }
            }
        }
    }
}