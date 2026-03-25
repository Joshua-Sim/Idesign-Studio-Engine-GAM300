#pragma once

#include "ECSystemManager.h"
#include "PhysicsSystem.h" 
#include "InputHandler.h" 
#include "ComponentList.h"
#include "Graphics.h" 
#include "Camera.h"

// Forward declarations
extern Framework::Coordinator ecsInterface;

namespace Framework
{

    struct PickupNotification {
        Entity bg;
        Entity icon;
        Entity text;
        float timer;
    };


    class PlayerInteractSystem : public ISystem
    {
    public:
        void Initialize();
        void Update(float dt) override;
        std::string GetName() override { return "PlayerInteractSystem"; }
        
        void HandlePlayerMovementandJump(float dt);
        void SpawnPickupNotification(int itemId, const std::string& itemName);
        void ClearAllPickupNotifications();
        void SpawnZipPickup(int itemId, const glm::vec3& spawnPos);
        Entity GetLookAtEntity() const { return mLookAtEntity; }

    private:
        void PerformInteract(Entity targetEntity);
        void SpawnInteractText();
        bool mWasEPressed = false; // Track previous frame state
        bool mGrounded = false;
        // --- System Pointers ---
        std::shared_ptr<PhysicsSystem> mPhysicsSystem;
        // std::shared_ptr<TagManager> mTagManager; // <-- No longer needed
        InputHandler* mInputHandler;
        std::shared_ptr<Graphics> mGraphicsSystem;

        // --- Interaction Properties ---
        const float mInteractRange = 3.0f;
        const float mLookRange     = 5.0f;   // Wider range used only for customer text visibility
        Entity      mLookAtEntity  = 0;       // Entity currently under the look-at ray (0 = none)

        std::vector<PickupNotification> mActiveNotifications;


        //set key press once per press
        bool   m_EWasDown = false;        // track last-frame state
        double m_nextPickupTime = 0.0;    // debounce timer
        double m_pickupCooldown = 0.15;   // seconds (tweak to taste)
        Entity mPromptTextEntity = -3;
        Entity mPromptIconEntity = -2;
        Entity mBackpackTitleTextEntity = (Entity) - 1;

    };
}