///////////////////////////////////////////////////////////////////////////////
///
///@file PhysicsSystem.h
/// 
/// @brief The creation of the physics class and the declaration of its functions and members
///  
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "ECSystemManager.h"
#include "ComponentList.h"
#include "Terrain.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>

namespace Framework
{
    class PhysicsSystem : public ISystem
    {
    public:
        void Initialize();
        void Update(float dt) override;
        std::string GetName() override;
        void Shutdown();

        //JPH::BodyID     mPickedID;             // whichever body you clicked
        //bool            mRotating = false;
        //glm::vec2       mDragStart{ 0,0 };
        //JPH::Quat       mStartRot;
        //JPH::E
        // mPrevMotionType = JPH::EMotionType::Static;

        //void OnMouseDown(int button, float mx, float my,
        //    const glm::mat4& view, const glm::mat4& proj,
        //    int vpW, int vpH);
        //void OnMouseDrag(float mx, float my,
        //    const glm::vec3& camRight, const glm::vec3& camUp);
        //void OnMouseUp(int button);

    private:
        JPH::TempAllocatorImpl* tempAllocator = nullptr;
        JPH::JobSystemThreadPool* jobSystem = nullptr;
        JPH::PhysicsSystem physicsSystem;
        JPH::BodyInterface* bodyInterface = nullptr;
    };

    // Change 'class' to 'struct' to guarantee all members are public accessible
    struct GameContactListener : public JPH::ContactListener
    {
        // 1. Use the BASE interface type (const JPH::BodyLockInterface*)
        //    This matches what GetBodyLockInterface() returns.
        const JPH::BodyLockInterface* mBodyLockInterface = nullptr;

        // (Optional) Keep this if you need it for other things, 
        // but for locking, we rely on mBodyLockInterface.
        JPH::BodyInterface* mBodyInterface = nullptr;

        // -----------------------------------------------------------------------
        // CONTACT REMOVED
        // -----------------------------------------------------------------------
        virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
        {
            // Check safety on the LOCK interface, not the general interface
            if (mBodyLockInterface == nullptr) return;

            JPH::BodyID b1 = inSubShapePair.GetBody1ID();
            JPH::BodyID b2 = inSubShapePair.GetBody2ID();

            Entity e1 = 0;
            Entity e2 = 0;

            // --- Safely read Body 1 ---
            {
                // Dereference the pointer (*mBodyLockInterface) to get the interface
                JPH::BodyLockRead lock1(*mBodyLockInterface, b1);
                if (lock1.Succeeded()) {
                    e1 = static_cast<Entity>(lock1.GetBody().GetUserData());
                }
                else {
                    return; // Body 1 invalid
                }
            }

            // --- Safely read Body 2 ---
            {
                JPH::BodyLockRead lock2(*mBodyLockInterface, b2);
                if (lock2.Succeeded()) {
                    e2 = static_cast<Entity>(lock2.GetBody().GetUserData());
                }
                else {
                    return; // Body 2 invalid
                }
            }

            std::cout << "[Physics] Contact END: " << e1 << " & " << e2 << std::endl;
        }
        // -----------------------------------------------------------------------
        // CONTACT ADDED (The one you already have)
        // -----------------------------------------------------------------------
        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
            Entity e1 = static_cast<Entity>(inBody1.GetUserData());
            Entity e2 = static_cast<Entity>(inBody2.GetUserData());

            std::cout << "[Physics] Contact START: " << e1 << " & " << e2 << std::endl;
        }
        
    };
}
