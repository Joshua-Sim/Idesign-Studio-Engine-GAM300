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


        std::vector<PickupNotification> mActiveNotifications;


        //set key press once per press
        bool   m_EWasDown = false;        // track last-frame state
        double m_nextPickupTime = 0.0;    // debounce timer
        double m_pickupCooldown = 0.15;   // seconds (tweak to taste)
        Entity mPromptTextEntity = -1;
        Entity mPromptIconEntity = -2;
    };
}