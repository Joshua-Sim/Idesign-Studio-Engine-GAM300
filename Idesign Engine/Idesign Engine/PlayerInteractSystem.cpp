#include "pch.h"
#include "PlayerInteractSystem.h"
#include "Camera.h"
#include <glm/glm/gtc/quaternion.hpp>
#include <iostream>
#include "Coordinator.h" 

namespace Framework
{
    void PlayerInteractSystem::Initialize()
    {
        // (This function is unchanged, you can remove any
        //  debug tagging code I gave you earlier)
        mPhysicsSystem = ecsInterface.GetSystem<PhysicsSystem>();
        mTagManager = ecsInterface.GetSystem<TagManager>();
        mInputHandler = InputHandler::GetInstance();
        mGraphicsSystem = ecsInterface.GetSystem<Graphics>();

        assert(mPhysicsSystem && "PlayerInteractSystem needs PhysicsSystem");
        assert(mTagManager && "PlayerInteractSystem needs TagManager");
        assert(mInputHandler && "PlayerInteractSystem needs InputHandler");
        assert(mGraphicsSystem && "PlayerInteractSystem needs GraphicsSystem");
    }

    // --- THIS FUNCTION IS UPDATED ---
    void PlayerInteractSystem::Update(float dt)
    {
        // --- NEW: ONE-TIME DEBUG CODE TO FORCE-TAG THE FROG ---
        if (!mDebugFrogTagged)
        {
            // We loop through the graphics system's entities, as it's
            // a reliable list of all loaded entities.
            for (Entity entity : mGraphicsSystem->mEntities)
            {
                if (ecsInterface.GetEntityName(entity) == "Frog")
                {
                    mTagManager->AddTag(entity, "Interactable");
                    std::cout << "[PlayerInteractSystem] DEBUG: Found and C++ tagged Entity "
                        << entity << " (Name: Frog)" << std::endl;
                    mDebugFrogTagged = true; // Mark as done
                    break; // We found the frog, no need to keep looping
                }
            }
        }
        // --- END OF NEW DEBUG CODE ---


        // (The rest of the function is unchanged)
        if (mInputHandler->IsKeyPressed(GLFW_KEY_E))
        {
            std::cout << "[PlayerInteractSystem] 'E' key was pressed!" << std::endl;
            TryInteract();
        }
    }

    void PlayerInteractSystem::TryInteract()
    {
        // (This function is unchanged)

        // 1. Get Camera
        Camera* gameCamera = mGraphicsSystem->getCamera();
        if (gameCamera == nullptr) { /* ... */ return; }

        // 2. Define Ray
        glm::vec3 origin = gameCamera->position;
        glm::vec3 direction = glm::normalize(gameCamera->front);
        Framework::RaycastHit hit;

        // 3. Perform Raycast (targeting category 1)
        bool didHit = mPhysicsSystem->Raycast(origin, direction, mInteractRange, hit, 1);

        if (!didHit)
        {
            std::cout << "[PlayerInteractSystem] Raycast fired but hit NOTHING (or only hit player)." << std::endl;
        }
        else
        {
            // 4. Check Tag
            std::string entityName = ecsInterface.GetEntityName(hit.entity);
            std::cout << "[PlayerInteractSystem] Raycast HIT entity: "
                << hit.entity << " (Name: " << entityName << ")" << std::endl;

            if (mTagManager->HasTag(hit.entity, "Interactable"))
            {
                // SUCCESS!
                std::cout << "Interact SUCCESS: Hit entity " << hit.entity
                    << " (Name: " << entityName << ")"
                    << " which is tagged 'Interactable'" << std::endl;
            }
            else
            {
                // FAIL
                std::cout << "Interact FAIL: Hit entity " << hit.entity
                    << " (Name: " << entityName << ")"
                    << " but it's NOT tagged 'Interactable'" << std::endl;
            }
        }
    }
}