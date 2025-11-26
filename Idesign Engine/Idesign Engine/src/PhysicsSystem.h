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

// Fix E0070: Include GLM Quaternion headers
#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/quaternion.hpp>

namespace Framework
{
    /// @brief Holds information about a raycast intersection.
    struct RaycastHit
    {
        Entity     entity = 0;       // The entity that was hit.
        glm::vec3 point{ 0.0f };    // The world-space point of impact.
        glm::vec3 normal{ 0.0f };   // The world-space normal of the surface hit.
        float      distance = 0.0f; // The distance from the ray's origin to the hit point.
        bool       hit = false;      // True if the raycast hit something.
    };

    class PhysicsSystem : public ISystem
    {
    public:
        void Initialize();
        void Update(float dt) override;
        std::string GetName() override;
        void Shutdown();

        // Used by PhaseManager / Scripts to remove bodies safely
        void RemoveBodyForEntity(Entity e);

        /// @brief Performs a raycast against the physics world.
        bool Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, int targetCategory = -1);

        // --- NEW: Expose BodyInterface for PhaseManager (Teleportation) ---
        JPH::BodyInterface* GetBodyInterface() { return bodyInterface; }
        // ------------------------------------------------------------------

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
        // CONTACT REMOVED
        virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override {
            JPH::BodyID b1 = inSubShapePair.GetBody1ID();
            JPH::BodyID b2 = inSubShapePair.GetBody2ID();

            std::lock_guard<std::mutex> lock(s_TriggerMutex);
            s_TriggerEvents.push_back(TriggerEvent{ TriggerEvent::EXIT, b1, b2 });
        }

        // CONTACT ADDED
        virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override {
            std::cout << "[Jolt] Contact Added!" << std::endl;
            std::lock_guard<std::mutex> lock(s_TriggerMutex);
            s_TriggerEvents.push_back(TriggerEvent{ TriggerEvent::ENTER, inBody1.GetID(), inBody2.GetID() });
        }
    };
}