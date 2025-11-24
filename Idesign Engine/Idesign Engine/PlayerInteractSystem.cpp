#include "pch.h"
#include "PlayerInteractSystem.h"
#include "Camera.h"
#include <glm/glm/gtc/quaternion.hpp>
#include <iostream>
#include "ScriptingSystem.h"
// Include Coordinator for GetEntityName and EntityHasTag
#include "Coordinator.h" 

// #include "TagManager.h" // <-- No longer needed

namespace Framework
{
    void PlayerInteractSystem::Initialize()
    {
        // Get all the systems we depend on
        mPhysicsSystem = ecsInterface.GetSystem<PhysicsSystem>();
        // mTagManager = ecsInterface.GetSystem<TagManager>(); // <-- No longer needed
        mInputHandler = InputHandler::GetInstance();
        mGraphicsSystem = ecsInterface.GetSystem<Graphics>();

        assert(mPhysicsSystem && "PlayerInteractSystem needs PhysicsSystem");
        // assert(mTagManager && "PlayerInteractSystem needs TagManager"); // <-- No longer needed
        assert(mInputHandler && "PlayerInteractSystem needs InputHandler");
        assert(mGraphicsSystem && "PlayerInteractSystem needs GraphicsSystem");
    }

    void PlayerInteractSystem::Update(float dt)
    {
        // --- All debug tagging code has been removed ---

        // Check for the 'E' key press
        if (mInputHandler->IsKeyPressed(GLFW_KEY_E))
        {
            std::cout << "[PlayerInteractSystem] 'E' key was pressed!" << std::endl;
            TryInteract();
        }
    }

    void PlayerInteractSystem::TryInteract()
    {
        // 1. Get the game camera directly from the Graphics System
        Camera* gameCamera = mGraphicsSystem->getCamera();
        if (gameCamera == nullptr) {
            std::cerr << "PlayerInteractSystem: No camera found in GraphicsSystem!" << std::endl;
            return;
        }

        // 2. Define the raycast parameters
        glm::vec3 origin = gameCamera->position;
        glm::vec3 direction = glm::normalize(gameCamera->front);
        Framework::RaycastHit hit;

        // 3. Perform the raycast (targeting category 1, which is "Collectible")
        bool didHit = mPhysicsSystem->Raycast(origin, direction, mInteractRange, hit, 1);

        if (!didHit)
        {
            // This message is normal when not looking at the frog
            std::cout << "[PlayerInteractSystem] Raycast fired but hit NOTHING (or only hit player)." << std::endl;
        }
        else
        {
            // 4. Get the entity's name
            std::string entityName = ecsInterface.GetEntityName(hit.entity);
            std::cout << "[PlayerInteractSystem] Raycast HIT entity: "
                << hit.entity << " (Name: " << entityName << ")" << std::endl;

            // 4b. Check the tag using the Coordinator's "EntityHasTag" function
            if (ecsInterface.EntityHasTag(hit.entity, "Interactable")) //
            {
                // SUCCESS!
                std::cout << "Interact SUCCESS: Hit entity " << hit.entity
                    << " (Name: " << entityName << ")"
                    << " which is tagged 'Interactable'" << std::endl;

                // --- TODO: Call Lua Function ---
                 // --- CALL THE LUA FUNCTION ---
                auto scripting = ecsInterface.GetSystem<ScriptingSystem>();
                if (scripting)
                {
                    // This assumes you added CallEntityFunction to ScriptingSystem!
                    // We call "OnInteract" on the entity we hit.
                    // We can pass the player ID as an argument if needed.
                    Entity playerID = ecsInterface.GetEntityByName("Player");
                    scripting->CallEntityFunction(hit.entity, "OnInteract", playerID);
                }
            }
            else
            {
                // This will appear if you hit a Category 1 object 
                // that is NOT tagged "Interactable"
                std::cout << "Interact FAIL: Hit entity " << hit.entity
                    << " (Name: " << entityName << ")"
                    << " but it's NOT tagged 'Interactable'" << std::endl;
            }
        }
    }
}