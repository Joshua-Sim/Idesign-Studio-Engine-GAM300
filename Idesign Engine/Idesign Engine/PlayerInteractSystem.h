#pragma once

#include "ECSystemManager.h"
#include "PhysicsSystem.h" 
#include "TagManager.h"    
#include "InputHandler.h" 
#include "ComponentList.h"
#include "Graphics.h" 
#include "Camera.h"

// Forward declarations
extern Framework::Coordinator ecsInterface;

namespace Framework
{
    class PlayerInteractSystem : public ISystem
    {
    public:
        void Initialize();
        void Update(float dt) override;
        std::string GetName() override { return "PlayerInteractSystem"; }

    private:
        void TryInteract();

        // --- System Pointers ---
        std::shared_ptr<PhysicsSystem> mPhysicsSystem;
        std::shared_ptr<TagManager>    mTagManager;
        InputHandler* mInputHandler;
        std::shared_ptr<Graphics> mGraphicsSystem;

        // --- Interaction Properties ---
        const float mInteractRange = 3.0f;

        // --- ADD THIS LINE ---
        bool mDebugFrogTagged = false;
        // ---------------------
    };
}