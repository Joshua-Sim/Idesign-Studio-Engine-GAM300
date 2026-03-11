#include "pch.h"
#include "PhysicsSystem.h"
#include "ECSystemManager.h"
#include "ComponentList.h"
#include "ImguiManager.h"
#include "Terrain.h"
#include "Graphics.h"
#include "PhysicsProperties.h"
#include "EventManager.h"
#include "BoxColliderComponent.h"
#include "Inventory.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/quaternion.hpp>

#include <unordered_map>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

extern Framework::Coordinator ecsInterface;
std::vector<JPH::BodyID> m_createdBodies;
static GameContactListener g_ContactListener;

namespace Layers {
    static constexpr JPH::ObjectLayer MOVING = 0;
}

// ---------- Small helpers ----------

inline bool IsWithinRange(const glm::vec3& a, const glm::vec3& b, float radius)
{
    const glm::vec3 d = a - b;
    return glm::dot(d, d) <= (radius * radius);
}

inline bool GetWorldPos(Framework::Coordinator& ecs, Entity e, glm::vec3& out)
{
    if (!ecs.HasComponent<TransformComponent>(e)) return false;
    out = ecs.GetComponent<TransformComponent>(e).position;
    return true;
}

static inline JPH::EMotionType ToJoltMotionType(int motionType) {
    switch (motionType) {
    case 0:  return JPH::EMotionType::Static;
    case 2:  return JPH::EMotionType::Kinematic;
    case 1:
    default: return JPH::EMotionType::Dynamic;
    }
}

static inline JPH::ObjectLayer ToObjectLayer(int category) {
    switch (category) {
    case 0: return 1; // Wall
    case 1: return 2; // Collectible
    case 2: return 3; // Player
    case 3: return 4; // Customer
    default: return Layers::MOVING; // fallback
    }
}

static inline float DegToRad(float deg) { return deg * 0.01745329251994329577f; }

static inline JPH::Quat MakeQuatFromEuler(const glm::vec3& deg) {
    glm::quat qx = glm::angleAxis(DegToRad(deg.x), glm::vec3(1, 0, 0));
    glm::quat qy = glm::angleAxis(DegToRad(deg.y), glm::vec3(0, 1, 0));
    glm::quat qz = glm::angleAxis(DegToRad(deg.z), glm::vec3(0, 0, 1));
    glm::quat gq = qy * qx * qz;
    return JPH::Quat(gq.x, gq.y, gq.z, gq.w);
}

static inline glm::vec3 QuatToEulerDegrees_Jolt(const JPH::Quat& qj)
{
    const float x = qj.GetX(), y = qj.GetY(), z = qj.GetZ(), w = qj.GetW();
    float sinp = 2.0f * (w * x - z * y);
    float pitch = std::abs(sinp) >= 1.0f ? std::copysign(0.5f * 3.1415926535f, sinp) : std::asin(sinp);
    float siny_cosp = 2.0f * (w * y + x * z);
    float cosy_cosp = 1.0f - 2.0f * (x * x + y * y);
    float yaw = std::atan2(siny_cosp, cosy_cosp);
    float sinr_cosp = 2.0f * (w * z + x * y);
    float cosr_cosp = 1.0f - 2.0f * (y * y + z * z);
    float roll = std::atan2(sinr_cosp, cosr_cosp);
    const float R2D = 57.2957795f;
    return glm::vec3(pitch * R2D, yaw * R2D, roll * R2D);
}

static inline void ApplyPropsToBody(JPH::BodyInterface* bi, JPH::BodyID id, const Framework::PhysicsProperties& p) {
    using namespace JPH;

    const auto wantMotion = ToJoltMotionType(p.motionType);
    EMotionType currentMotion = bi->GetMotionType(id);

    if (currentMotion != wantMotion) {
        if (wantMotion == EMotionType::Static) {
            bi->DeactivateBody(id);
        }
        bi->SetMotionType(id, wantMotion, EActivation::DontActivate);
        if (wantMotion != EMotionType::Static) {
            bi->ActivateBody(id);
        }
    }
    bi->SetGravityFactor(id, p.gravityFactor);
}

// Broadphase layers
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer) const override { return JPH::BroadPhaseLayer(0); }
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer) const { return "MOVING"; }
};

class AllowAllObjVsBP final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override { return true; }
};

class AllowAllPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override { return true; }
};

namespace {
    class AllowAllBroadPhaseFilter : public JPH::BroadPhaseLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
    };

    class AllowAllObjectFilter : public JPH::ObjectLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer) const override { return true; }
    };

    class SpecificCategoryFilter : public JPH::ObjectLayerFilter {
    public:
        SpecificCategoryFilter(JPH::ObjectLayer layer) : mLayer(layer) {}
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            return mLayer == inLayer;
        }
    private:
        JPH::ObjectLayer mLayer;
    };
}

// --- Physics System Implementation ---
namespace Framework {
    static std::unordered_map<Entity, glm::vec3> s_lastLin, s_lastAng;

    static void EnsureBodyForEntity(Entity e, Framework::Coordinator& ecs, JPH::BodyInterface* bi, std::vector<JPH::BodyID>& created_list)
    {
        if (bi == nullptr) return;
        if (!ecs.HasComponent<TransformComponent>(e) || !ecs.HasComponent<PhysicsProperties>(e)) return;
        if (ecs.HasComponent<PhysicsBodyComponent>(e)) return;

        auto& tf = ecs.GetComponent<TransformComponent>(e);
        auto& prop = ecs.GetComponent<PhysicsProperties>(e);

        struct ColliderConfig {
            std::vector<const BoxColliderEntry*> solids;
            std::vector<const BoxColliderEntry*> triggers;
        };
        ColliderConfig config;

        if (ecs.HasComponent<BoxColliderComponent>(e)) {
            auto& component = ecs.GetComponent<BoxColliderComponent>(e);
            for (const auto& entry : component.colliders) {
                if (entry.canPassThrough) config.triggers.push_back(&entry);
                else config.solids.push_back(&entry);
            }
        }

        if (config.solids.empty() && config.triggers.empty()) return;

        PhysicsBodyComponent newBodyComp;
        newBodyComp.isInPhysicsSystem = true;

        auto CreateJoltBody = [&](const std::vector<const BoxColliderEntry*>& entries, bool isSensor)
            {
                JPH::ShapeRefC shape = nullptr;

                // Focus on Case A (Single box) which is what most slanted ramps use
                if (entries.size() == 1) {
                    const auto* entry = entries[0];
                    JPH::Vec3 half_extents(
                        std::abs(0.5f * entry->size.x * tf.scale.x),
                        std::abs(0.5f * entry->size.y * tf.scale.y),
                        std::abs(0.5f * entry->size.z * tf.scale.z)
                    );
                    half_extents = JPH::Vec3::sMax(half_extents, JPH::Vec3(0.001f, 0.001f, 0.001f));

                    shape = new JPH::BoxShape(half_extents);

                    // If there's an offset, wrap it in an OffsetCenterOfMassShape 
                    // This is WAY more reliable for rotated single boxes than StaticCompoundShape
                    if (glm::length(entry->center) > 0.001f) {
                        JPH::Vec3 joltOffset(
                            entry->center.x * tf.scale.x,
                            entry->center.y * tf.scale.y,
                            entry->center.z * tf.scale.z
                        );
                        shape = new JPH::OffsetCenterOfMassShape(shape, joltOffset);
                    }
                }
                else {
                    // Case B: Compound Shape (Multiple boxes OR single box with offset)
                    // This keeps the Physics Center of Mass aligned with your Entity Pivot
                    JPH::StaticCompoundShapeSettings compoundSettings;

                    for (const auto* entry : entries) {
                        JPH::Vec3 half_extents(
                            std::abs(0.5f * entry->size.x * tf.scale.x),
                            std::abs(0.5f * entry->size.y * tf.scale.y),
                            std::abs(0.5f * entry->size.z * tf.scale.z)
                        );
                        half_extents = JPH::Vec3::sMax(half_extents, JPH::Vec3(0.001f, 0.001f, 0.001f));

                        JPH::Vec3 offset(
                            entry->center.x * tf.scale.x,
                            entry->center.y * tf.scale.y,
                            entry->center.z * tf.scale.z
                        );

                        compoundSettings.AddShape(offset, JPH::Quat::sIdentity(), new JPH::BoxShapeSettings(half_extents));
                    }

                    auto result = compoundSettings.Create();
                    if (result.HasError()) return;
                    shape = result.Get();
                }

                JPH::BodyCreationSettings bcs(
                    shape,
                    JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
                    MakeQuatFromEuler(tf.rotation),
                    ToJoltMotionType(prop.motionType),
                    ToObjectLayer(prop.category)
                );

                bcs.mIsSensor = isSensor;
                bcs.mGravityFactor = prop.gravityFactor;
                bcs.mUserData = static_cast<JPH::uint64>(e);
                bcs.mAllowDynamicOrKinematic = true;
                bcs.mFriction = prop.friction;
                bcs.mRestitution = prop.restitution;

                std::string name = ecs.GetEntityName(e);
                if (name == "Player") {
                    bcs.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
                        JPH::EAllowedDOFs::TranslationY |
                        JPH::EAllowedDOFs::TranslationZ |
                        JPH::EAllowedDOFs::RotationY;
                }
                else {
                    bcs.mAllowedDOFs = JPH::EAllowedDOFs::All;
                }

                if (prop.motionType == 1) {

                    bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
                    bcs.mAllowSleeping = false;
                }
                else {
                    bcs.mMotionQuality = JPH::EMotionQuality::Discrete;
                }

                JPH::EActivation act = prop.startAwake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
                JPH::BodyID id = bi->CreateAndAddBody(bcs, act);

                if (!id.IsInvalid()) {
                    if (prop.motionType == 1) {
                        if (glm::length(prop.linearVelocity) > 0.0f)
                            bi->SetLinearVelocity(id, JPH::Vec3(prop.linearVelocity.x, prop.linearVelocity.y, prop.linearVelocity.z));
                    }
                    created_list.push_back(id);
                    newBodyComp.IDs.push_back(id);
                    s_lastLin[e] = prop.linearVelocity;
                    s_lastAng[e] = prop.angularVelocity;
                }
            };

        if (!config.solids.empty()) CreateJoltBody(config.solids, false);
        if (!config.triggers.empty()) CreateJoltBody(config.triggers, true);

        if (!newBodyComp.IDs.empty()) ecs.AddComponent<PhysicsBodyComponent>(e, newBodyComp);
    }
    PhysicsSystem::~PhysicsSystem()
    {
        std::cout << "[Physics] Destructor called. Cleaning up Jolt...\n";

        // 1. Clean up Interface/Bodies
        if (bodyInterface) {
            // Remove and destroy all bodies this system created
            for (JPH::BodyID id : m_createdBodies) {
                if (!id.IsInvalid()) {
                    bodyInterface->RemoveBody(id);
                    bodyInterface->DestroyBody(id);
                }
            }
            m_createdBodies.clear();
            bodyInterface = nullptr;
        }


        // 3. Delete Jolt Pointers (The Source of your Leak)
        // IMPORTANT: Check for null to avoid double-delete if Initialize failed
        if (jobSystem) {
            delete jobSystem;
            jobSystem = nullptr;
        }

        if (tempAllocator) {
            delete tempAllocator; // <--- THIS FREES THE 10MB LEAK
            tempAllocator = nullptr;
        }

        // 4. Unregister Jolt Types
        JPH::UnregisterTypes();

        // 5. Delete Factory
        if (JPH::Factory::sInstance) {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }
    }
    void PhysicsSystem::Initialize() {
        using namespace JPH;
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();
        tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);
        jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        static BPLayerInterfaceImpl broadPhaseInterface;
        static AllowAllObjVsBP      objVsBP;
        static AllowAllPairFilter   pairFilter;

        physicsSystem.Init(1024, 0, 1024, 1024, broadPhaseInterface, objVsBP, pairFilter);
        physicsSystem.SetContactListener(&g_ContactListener);
        bodyInterface = &physicsSystem.GetBodyInterface();

        // ... (Spawning logic omitted for brevity, keep yours) ...

        std::cout << "[Physics] Initialized.\n";
    }

    static float ComputeEntityHalfHeight(Framework::Coordinator& ecs, Entity e)
    {
        if (!ecs.HasComponent<BoxColliderComponent>(e) || !ecs.HasComponent<TransformComponent>(e))
            return 0.5f;

        const auto& tf = ecs.GetComponent<TransformComponent>(e);
        const auto& bc = ecs.GetComponent<BoxColliderComponent>(e);

        float maxY = 0.5f;
        for (const auto& c : bc.colliders)
        {
            // Only consider solid colliders for grounding
            if (c.canPassThrough) continue;

            float halfY = 0.5f * std::abs(c.size.y * tf.scale.y);
            float centerY = std::abs(c.center.y * tf.scale.y);
            maxY = std::max(maxY, centerY + halfY);
        }
        return std::max(maxY, 0.05f);
    }

    static bool IsWalkableGround(const glm::vec3& n, float maxSlopeAngleDeg)
    {
        // slope angle from up axis: angle = arccos(n.y)
        // walkable if angle <= maxSlope
        float cosMax = std::cos(maxSlopeAngleDeg * 0.01745329251994329577f);
        return n.y >= cosMax;
    }


    void PhysicsSystem::Update(float dt) {
        if (dt <= 0.0001f) return;

        // 1. Create Bodies
        for (Entity e : ecsInterface.GetEntities()) {
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
        }

        // --- [STEP 2: PROCESS JOLT TRIGGER EVENTS] ---
        {
            std::lock_guard<std::mutex> lock(s_TriggerMutex);

            for (const auto& ev : s_TriggerEvents) {

                if (!bodyInterface->IsAdded(ev.b1) || !bodyInterface->IsAdded(ev.b2)) continue;

                Entity e1 = (Entity)bodyInterface->GetUserData(ev.b1);
                Entity e2 = (Entity)bodyInterface->GetUserData(ev.b2);

                if (!ecsInterface.IsEntityValid(e1) || !ecsInterface.IsEntityValid(e2)) continue;

                // Inside PhysicsSystem::Update -> Step 2 loop

                auto ProcessTrigger = [&](Entity triggerEntity, Entity visitorEntity)
                    {
                        std::string tName = ecsInterface.GetEntityName(triggerEntity);
                        std::string vName = ecsInterface.GetEntityName(visitorEntity);

                        // Debug 1: Detection
                        // std::cout << "[Physics] Checking Potential Trigger: " << tName << " hit by " << vName << std::endl;

                        // Check A: Visitor Name
                        if (vName != "Player") {
                            return;
                        }

                        // Check B: Component
                        if (!ecsInterface.HasComponent<BoxColliderComponent>(triggerEntity)) {
                            return;
                        }

                        auto& boxComp = ecsInterface.GetComponent<BoxColliderComponent>(triggerEntity);

                        // Check C: Iterate Colliders
                        bool foundValidTrigger = false;
                        for (const auto& entry : boxComp.colliders)
                        {
                            if (entry.canPassThrough && !entry.triggerMessage.empty())
                            {
                                CollisionEventData payload;
                                payload.senderID = visitorEntity; // Player
                                payload.targetID = triggerEntity; // Herb
                                payload.actionType = (ev.type == TriggerEvent::ENTER) ? 1 : 0;

                                EventManager::Instance().Fire(entry.triggerMessage, &payload);
                                foundValidTrigger = true;
                                break;
                            }
                        }

                        if (!foundValidTrigger) {
                            std::cout << "   -> Fail: No collider on " << tName << " was (IsTrigger=True AND had TriggerMessage)." << std::endl;
                        }
                    };

                // Run Checks Both Ways
                ProcessTrigger(e1, e2); // Is e1 the Trigger?
                ProcessTrigger(e2, e1); // Is e2 the Trigger?
            }
            s_TriggerEvents.clear();
        }

        // --- [GROUND CHECK (raycast)] ---
        for (Entity e : ecsInterface.GetEntities())
        {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) ||
                !ecsInterface.HasComponent<PhysicsProperties>(e))
                continue;

            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);
            if (pb.IDs.empty()) continue;

            // Get current physics position
            JPH::RVec3 jp = bodyInterface->GetPosition(pb.IDs[0]);
            glm::vec3 pos(jp.GetX(), jp.GetY(), jp.GetZ());

            float halfH = ComputeEntityHalfHeight(ecsInterface, e);

            // Start just above the feet, not near body center
            const float footProbeBelowBody = 0.05f;

            // Start slightly BELOW the player's bottom so we don't hit our own body
            glm::vec3 origin = pos - glm::vec3(0.0f, halfH + footProbeBelowBody, 0.0f);

            // Short ray downward from just below feet
            float rayLen = prop.groundCheckExtra + 0.15f;

            RaycastHit hit;
            bool didHit = Raycast(origin, glm::vec3(0.0f, -1.0f, 0.0f), rayLen, hit, -1);

            /*if (ecsInterface.GetEntityName(e) == "Player")
            {
                std::cout
                    << "[GroundCheck] pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ") "
                    << "halfH=" << halfH
                    << " origin=(" << origin.x << ", " << origin.y << ", " << origin.z << ") "
                    << " groundExtra=" << prop.groundCheckExtra
                    << " rayLen=" << rayLen
                    << " didHit=" << didHit;

                if (didHit && hit.hit)
                {
                    std::cout
                        << " hitDist=" << hit.distance
                        << " normal=(" << hit.normal.x << ", " << hit.normal.y << ", " << hit.normal.z << ")"
                        << " entity=" << (int)hit.entity;
                }

                std::cout << std::endl;
            }*/

            if (didHit && hit.hit &&
                hit.normal.y > 0.5f &&
                hit.entity != e)
            {
                prop.groundNormal = hit.normal;
                prop.isGrounded = IsWalkableGround(hit.normal, prop.maxSlopeAngleDeg);
            }
            else
            {
                prop.isGrounded = false;
                prop.groundNormal = glm::vec3(0.0f, 1.0f, 0.0f);
            }
        }

        // --- [STEP 3: PRE-PHYSICS SYNC] ---
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<PhysicsProperties>(e)) continue;

            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            // Keep motion type consistent with immovable flag
            if (prop.isImmovable == true) prop.motionType = 0;
            else if (prop.motionType == 0) prop.motionType = 1;

            for (JPH::BodyID id : pb.IDs) {
                ApplyPropsToBody(bodyInterface, id, prop);

                if (prop.motionType == 1) {
                    // --- Dynamic body update ---

                    // Make runtime gravityFactor toggles actually take effect
                    bodyInterface->SetGravityFactor(id, prop.gravityFactor);

                    // Current velocity from Jolt
                    JPH::Vec3 cur = bodyInterface->GetLinearVelocity(id);

                    // Inputs from gameplay
                    const float eps = 0.01f;
                    const bool hasXZ = (std::abs(prop.linearVelocity.x) > eps) || (std::abs(prop.linearVelocity.z) > eps);
                    const bool hasYImpulse = (std::abs(prop.linearVelocity.y) > eps);   // e.g., jump for one frame

                    //// Decide new velocity components
                    //const float newX = hasXZ ? prop.linearVelocity.x : 0.0f;
                    //const float newZ = hasXZ ? prop.linearVelocity.z : 0.0f;
                    //const float newY = hasYImpulse ? prop.linearVelocity.y : cur.GetY();

                    //if (hasXZ || hasYImpulse) {
                    //    bodyInterface->SetLinearVelocity(id, JPH::Vec3(newX, newY, newZ));
                    //    bodyInterface->ActivateBody(id);
                    //}
                    //else {
                    //    // No gameplay input: keep Y (gravity) but stop planar drift
                    //    bodyInterface->SetLinearVelocity(id, JPH::Vec3(0.0f, cur.GetY(), 0.0f));
                    //    bodyInterface->SetAngularVelocity(id, JPH::Vec3(0, 0, 0));
                    //}

                    glm::vec3 curV(cur.GetX(), cur.GetY(), cur.GetZ());

                    glm::vec3 desiredXZ(prop.linearVelocity.x, 0.0f, prop.linearVelocity.z);

                    // If grounded, project desired movement onto the slope plane (prevents “pop/hop” on stop)
                    if (prop.isGrounded)
                    {
                        glm::vec3 n = glm::normalize(prop.groundNormal);
                        desiredXZ = desiredXZ - glm::dot(desiredXZ, n) * n; // remove into-normal component
                    }

                    // Build final velocity
                    glm::vec3 finalV = curV;

                    // Jump impulse (one-shot Y)
                    if (hasYImpulse)
                    {
                        finalV.y = prop.linearVelocity.y;
                    }
                    else
                    {
                        // No input: stop sliding down slope if grounded
                        if (prop.isGrounded)
                        {
                            // FIX: Only zero out X and Z. 
                            // If we just jumped, finalV.y will be positive; we MUST NOT zero it.
                            finalV.x = 0.0f;
                            finalV.z = 0.0f;

                            // Only apply the "stick to ground" bias if we aren't currently jumping
                            if (!hasYImpulse && finalV.y <= 0.0f)
                            {
                                finalV.y = std::min(finalV.y, prop.stickToGroundVel);
                            }
                        }
                    }
                    // Apply input motion
                    if (hasXZ)
                    {
                        finalV.x = desiredXZ.x;
                        finalV.z = desiredXZ.z;
                    }
                    else
                    {

                            finalV.x = 0.0f;
                            finalV.z = 0.0f;

                    }

                    bodyInterface->SetLinearVelocity(id, JPH::Vec3(finalV.x, finalV.y, finalV.z));
                    bodyInterface->ActivateBody(id);

                    // Consume one-shot Y impulse (so jump only applies once)
                    if (hasYImpulse)
                        prop.linearVelocity.y = 0.0f;

                    // Zero tiny angular velocity noise unless you explicitly set it elsewhere
                    if (glm::length(prop.angularVelocity) <= 0.01f) {
                        bodyInterface->SetAngularVelocity(id, JPH::Vec3(0, 0, 0));
                    }

                }
                else {
                    // --- Kinematic/static: drive transform from ECS ---
                    if (ecsInterface.HasComponent<TransformComponent>(e)) {
                        auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
                        bodyInterface->SetPositionAndRotation(
                            id,
                            JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
                            MakeQuatFromEuler(tf.rotation),
                            JPH::EActivation::Activate
                        );
                    }
                }
            }
        }
        // 1. Define the target physics rate (60 Hz = 0.0166s)
        const float fixedTimeStep = 1.0f / 60.0f;

        // 2. Accumulate time (Use 'static' to remember time between frames)
        static float physicsAccumulator = 0.0f;
        physicsAccumulator += dt;

        // 3. Safety Clamp: Prevent "Death Spiral" if game hangs
        if (physicsAccumulator > 0.25f) physicsAccumulator = 0.25f;

        // 4. Run as many fixed steps as needed to catch up
        // (At 20 FPS, this loop runs 3 times per frame. At 60 FPS, it runs 1 time.)
        while (physicsAccumulator >= fixedTimeStep)
        {
            // Run Jolt with a constant 1/60th second step
            physicsSystem.Update(fixedTimeStep, 1, tempAllocator, jobSystem);

            physicsAccumulator -= fixedTimeStep;
        }

        // --- [STEP 5: POST-PHYSICS SYNC] ---
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<TransformComponent>(e) || !ecsInterface.HasComponent<PhysicsProperties>(e)) continue;

            auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            if (prop.motionType == 1 && !pb.IDs.empty()) {
                const JPH::RVec3 p = bodyInterface->GetPosition(pb.IDs[0]);
                tf.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());
                JPH::Quat q = bodyInterface->GetRotation(pb.IDs[0]);
                tf.rotation = QuatToEulerDegrees_Jolt(q);
            }
        }
    }

    // --- Raycast (Keep as is) ---
    bool PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, int targetCategory)
    {
        auto* narrowPhaseQuery = &physicsSystem.GetNarrowPhaseQuery();
        JPH::RRayCast ray(
            JPH::RVec3(origin.x, origin.y, origin.z),
            JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance
        );

        JPH::RayCastResult hit_result;
        bool did_hit = false;
        AllowAllBroadPhaseFilter bp_filter;

        if (targetCategory == -1) {
            AllowAllObjectFilter obj_filter;
            did_hit = narrowPhaseQuery->CastRay(ray, hit_result, bp_filter, obj_filter);
        }
        else {
            SpecificCategoryFilter obj_filter(ToObjectLayer(targetCategory));
            did_hit = narrowPhaseQuery->CastRay(ray, hit_result, bp_filter, obj_filter);
        }

        if (did_hit) {
            JPH::BodyID body_id = hit_result.mBodyID;
            auto& body_lock_interface = physicsSystem.GetBodyLockInterface();
            JPH::BodyLockRead lock(body_lock_interface, body_id);

            if (lock.Succeeded()) {
                const JPH::Body& body = lock.GetBody();
                outHit.entity = static_cast<Entity>(body.GetUserData());
                outHit.hit = true;

                JPH::RVec3 hit_point_world = ray.mOrigin + ray.mDirection * hit_result.mFraction;
                outHit.point = glm::vec3(hit_point_world.GetX(), hit_point_world.GetY(), hit_point_world.GetZ());

                JPH::Vec3 normal_world = body.GetWorldSpaceSurfaceNormal(hit_result.mSubShapeID2, hit_point_world);
                outHit.normal = glm::vec3(normal_world.GetX(), normal_world.GetY(), normal_world.GetZ());
                outHit.distance = hit_result.mFraction * maxDistance;
                return true;
            }
        }
        outHit.hit = false;
        return false;
    }


    // --- Remove Body ---
    void PhysicsSystem::RemoveBodyForEntity(Entity e) {
        if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e)) return;
        auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);

        for (JPH::BodyID id : pb.IDs) {
            bodyInterface->RemoveBody(id);
            bodyInterface->DestroyBody(id);

            // Efficient removal
            auto it = std::find(m_createdBodies.begin(), m_createdBodies.end(), id);
            if (it != m_createdBodies.end()) {
                std::iter_swap(it, m_createdBodies.end() - 1);
                m_createdBodies.pop_back();
            }
        }
        ecsInterface.RemoveComponent<PhysicsBodyComponent>(e);
    }

    std::string PhysicsSystem::GetName() { return "PhysicsSystem"; }

    void PhysicsSystem::Shutdown() {
        std::cout << "[Physics] Cleaning up Jolt...\n";
        if (bodyInterface) {
            for (JPH::BodyID id : m_createdBodies) {
                if (!id.IsInvalid()) {
                    bodyInterface->RemoveBody(id);
                    bodyInterface->DestroyBody(id);
                }
            }
            m_createdBodies.clear();
            bodyInterface = nullptr;
        }
        for (Entity e : ecsInterface.GetEntities()) {
            if (ecsInterface.HasComponent<PhysicsBodyComponent>(e)) ecsInterface.RemoveComponent<PhysicsBodyComponent>(e);
        }

        delete jobSystem;
        delete tempAllocator;
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }



}