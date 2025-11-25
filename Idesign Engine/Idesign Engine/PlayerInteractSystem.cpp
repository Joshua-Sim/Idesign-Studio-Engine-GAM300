#include "pch.h"
#include "PlayerInteractSystem.h"
#include "Camera.h"
#include <glm/glm/gtc/quaternion.hpp>
#include <iostream>
#include "ScriptingSystem.h"
// Include Coordinator for GetEntityName and EntityHasTag
#include "Coordinator.h" 
#include "Inventory.h"
#include "Graphics.h"

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
            TryPickupCollectible();
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

    void PlayerInteractSystem::TryPickupCollectible()
    {
        // 0) Only allow in game view (optional safety)
        if (mGraphicsSystem && mGraphicsSystem->GetMouseTargetView() != 0)
            return;

        // 1) Build ray from the active game camera
        Camera* gameCamera = mGraphicsSystem ? mGraphicsSystem->getCamera() : nullptr;
        if (!gameCamera) return;

        const glm::vec3 origin = gameCamera->position;
        const glm::vec3 dir = glm::normalize(gameCamera->front);

        // 2) Raycast for collectibles
        Framework::RaycastHit hit{};
        constexpr int kCollectibleCategory = 1;           // adjust if your layer differs
        if (!mPhysicsSystem || !mPhysicsSystem->Raycast(origin, dir, mInteractRange, hit, kCollectibleCategory) || !hit.hit)
        {
            std::cout << "[Pickup] Nothing collectible in front.\n";
            return;
        }

        Framework::Entity target = hit.entity;
        if (!ecsInterface.IsEntityValid(target)) return;

        // 3) Require Collectible + Transform
        if (!ecsInterface.HasComponent<CollectibleProperties>(target) ||
            !ecsInterface.HasComponent<TransformComponent>(target))
        {
            std::cout << "[Pickup] Hit entity is not a Collectible.\n";
            return;
        }

        auto& coll = ecsInterface.GetComponent<CollectibleProperties>(target);
        auto& tr = ecsInterface.GetComponent<TransformComponent>(target);

        // 4) Within pickup radius (use actual player distance, not ray t)
        const glm::vec3 toItem = tr.position - origin;
        const float     dist2 = glm::dot(toItem, toItem);
        const float     r2 = coll.pickupRadius * coll.pickupRadius;
        if (dist2 > r2) {
            std::cout << "[Pickup] Too far.\n";
            return;
        }

        // 5) Validate item
        if (coll.itemId < 0 || coll.itemId >= Framework::kItemCount) {
            std::cout << "[Pickup] Invalid itemId: " << coll.itemId << "\n";
            return;
        }

        // 6) Add to Inventory hotbar (first empty, non-stackable path)
        auto& inv = Framework::Inventory::Instance();
        const int slot = inv.AddFirstEmpty(coll.itemId);
        if (slot < 0) {
            std::cout << "[Pickup] Hotbar full; cannot pick up.\n";
            return;
        }

        // 7) Remove physics + destroy entity
        if (mPhysicsSystem) {
            mPhysicsSystem->RemoveBodyForEntity(target);  // ensure this exists in PhysicsSystem
        }
        ecsInterface.DestroyEntity(target);

        // Optional: feedback
        std::cout << "[Pickup] Picked " << Framework::kItemNames[coll.itemId] << " into hotbar slot " << slot << "\n";
    }


}