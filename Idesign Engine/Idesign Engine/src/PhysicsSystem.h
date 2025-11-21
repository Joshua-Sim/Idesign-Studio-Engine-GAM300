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
#include "BoxColliderComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>

#include <glm.hpp> // --- NEW --- Added for glm::vec3 in RaycastHit

namespace Framework
{
    // --- NEW ---
    /// @brief Holds information about a raycast intersection.
    struct RaycastHit
    {
        Entity    entity = 0;      // The entity that was hit.
        glm::vec3 point{ 0.0f };   // The world-space point of impact.
        glm::vec3 normal{ 0.0f };  // The world-space normal of the surface hit.
        float     distance = 0.0f; // The distance from the ray's origin to the hit point.
        bool      hit = false;     // True if the raycast hit something.
    };
    // --- END NEW ---

    class PhysicsSystem : public ISystem
    {
    public:
        void Initialize();
        void Update(float dt) override;
        std::string GetName() override;
        void Shutdown();

        // --- NEW ---
        /// @brief Performs a raycast against the physics world.
        /// @param origin The starting point of the ray in world space.
        /// @param direction The direction of the ray (should be normalized).
        /// @param maxDistance The maximum length of the ray.
        /// @param outHit The structure to store hit information.
        /// @param targetCategory (Optional) The specific category (from PhysicsProperties) to hit. -1 means hit anything.
        /// @return True if the ray hit something, false otherwise.
        bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, int targetCategory = -1);
        // --- END NEW ---


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

    // Define the Queue and Mutex globally or in the class
    struct TriggerEvent {
        enum Type { ENTER, EXIT };
        Type type;
        JPH::BodyID b1;
        JPH::BodyID b2;
    };

    static std::mutex s_TriggerMutex;
    static std::vector<TriggerEvent> s_TriggerEvents;

    struct GameContactListener : public JPH::ContactListener
    {
        // We don't need ANY interfaces here anymore.

        // -----------------------------------------------------------------------
        // CONTACT REMOVED (Safe Version)
        // -----------------------------------------------------------------------
        virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
            JPH::BodyID b1 = inSubShapePair.GetBody1ID();
            JPH::BodyID b2 = inSubShapePair.GetBody2ID();

            std::lock_guard<std::mutex> lock(s_TriggerMutex);

            // [FIX] Add 'TriggerEvent' before the curly braces
            s_TriggerEvents.push_back(TriggerEvent{ TriggerEvent::EXIT, b1, b2 });
        }

        // -----------------------------------------------------------------------
        // CONTACT ADDED
        // -----------------------------------------------------------------------
        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
            if (inBody1.IsSensor() || inBody2.IsSensor()) {
                std::lock_guard<std::mutex> lock(s_TriggerMutex);

                // [FIX] Add 'TriggerEvent' before the curly braces
                s_TriggerEvents.push_back(TriggerEvent{ TriggerEvent::ENTER, inBody1.GetID(), inBody2.GetID() });
            }
        }
    };
}