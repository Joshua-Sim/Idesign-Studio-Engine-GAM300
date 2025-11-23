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
// --- NEW ---
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h> // Required for multiple colliders
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
// --- END NEW ---

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/quaternion.hpp>

#include <unordered_map>
#include <iostream>
#include <cmath>
#include <vector>

extern Framework::Coordinator ecsInterface;
std::vector<JPH::BodyID> m_createdBodies;
static GameContactListener g_ContactListener;

namespace Layers {
    static constexpr JPH::ObjectLayer MOVING = 0;
}

// ---------- Small helpers ----------

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

// --- Helper Classes for Raycast ---
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

    // Static Helper for Box Math
    // UPDATED: Now takes a single BoxColliderEntry instead of the whole component
    static bool IsPointInsideBox(const glm::vec3& point, const Framework::TransformComponent& boxTrans, const Framework::BoxColliderEntry& entry) {
        glm::vec3 boxWorldCenter = boxTrans.position + entry.center;
        glm::vec3 halfSize = glm::vec3(
            std::abs(entry.size.x * boxTrans.scale.x),
            std::abs(entry.size.y * boxTrans.scale.y),
            std::abs(entry.size.z * boxTrans.scale.z)
        ) * 0.5f;

        bool x = std::abs(point.x - boxWorldCenter.x) <= halfSize.x;
        bool y = std::abs(point.y - boxWorldCenter.y) <= halfSize.y;
        bool z = std::abs(point.z - boxWorldCenter.z) <= halfSize.z;
        return x && y && z;
    }

    static void EnsureBodyForEntity(Entity e, Framework::Coordinator& ecs, JPH::BodyInterface* bi, std::vector<JPH::BodyID>& created_list) {
        if (bi == nullptr) return;
        if (!ecs.HasComponent<TransformComponent>(e) || !ecs.HasComponent<PhysicsProperties>(e)) return;
        if (ecs.HasComponent<PhysicsBodyComponent>(e)) return;

        JPH::ShapeRefC finalShape = nullptr;
        auto& tf = ecs.GetComponent<TransformComponent>(e);

        // --- NEW: Multiple Collider Support ---
        bool isSensor = false;

        if (ecs.HasComponent<BoxColliderComponent>(e)) {
            auto& component = ecs.GetComponent<BoxColliderComponent>(e);

            // If no colliders in the list, skip
            if (component.colliders.empty()) return;

            // Logic: If ANY collider is solid, the body is solid. 
            // Only if ALL are triggers do we treat the body as a sensor.
            // (You can adjust this logic based on your game needs)
            bool anySolid = false;
            for (const auto& c : component.colliders) {
                if (!c.canPassThrough) anySolid = true;
            }
            isSensor = !anySolid;

            // CASE 1: Single Collider (Optimization)
            if (component.colliders.size() == 1) {
                const auto& entry = component.colliders[0];

                JPH::Vec3 half_extents(
                    std::abs(0.5f * entry.size.x * tf.scale.x),
                    std::abs(0.5f * entry.size.y * tf.scale.y),
                    std::abs(0.5f * entry.size.z * tf.scale.z)
                );

                // Prevent zero-size box crash
                const float kMinSize = 0.001f;
                half_extents = JPH::Vec3::sMax(half_extents, JPH::Vec3(kMinSize, kMinSize, kMinSize));

                JPH::BoxShapeSettings bs(half_extents);
                JPH::ShapeSettings::ShapeResult result = bs.Create();
                if (result.HasError()) return;

                finalShape = result.Get();

                // Apply Offset if needed
                if (entry.center != glm::vec3(0.0f)) {
                    JPH::Vec3 jolt_center(entry.center.x, entry.center.y, entry.center.z);
                    finalShape = new JPH::OffsetCenterOfMassShape(finalShape, jolt_center);
                }
            }
            // CASE 2: Multiple Colliders (Compound Shape)
            else {
                JPH::StaticCompoundShapeSettings compoundSettings;

                for (const auto& entry : component.colliders) {
                    JPH::Vec3 half_extents(
                        std::abs(0.5f * entry.size.x * tf.scale.x),
                        std::abs(0.5f * entry.size.y * tf.scale.y),
                        std::abs(0.5f * entry.size.z * tf.scale.z)
                    );

                    const float kMinSize = 0.001f;
                    half_extents = JPH::Vec3::sMax(half_extents, JPH::Vec3(kMinSize, kMinSize, kMinSize));

                    // Create sub-shape settings
                    JPH::BoxShapeSettings* boxSettings = new JPH::BoxShapeSettings(half_extents);

                    // Add to compound with offset
                    JPH::Vec3 offset(entry.center.x, entry.center.y, entry.center.z);
                    compoundSettings.AddShape(offset, JPH::Quat::sIdentity(), boxSettings);
                }

                JPH::ShapeSettings::ShapeResult result = compoundSettings.Create();
                if (result.HasError()) return;
                finalShape = result.Get();
            }
        }

        if (finalShape == nullptr) return;

        auto& prop = ecs.GetComponent<PhysicsProperties>(e);
        JPH::BodyCreationSettings bcs(
            finalShape,
            JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
            MakeQuatFromEuler(tf.rotation),
            ToJoltMotionType(prop.motionType),
            ToObjectLayer(prop.category)
        );

        bcs.mIsSensor = isSensor;
        bcs.mGravityFactor = prop.gravityFactor;
        bcs.mUserData = static_cast<JPH::uint64>(e);
        bcs.mAllowDynamicOrKinematic = true;

        if (prop.motionType == 1) {
            bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            bcs.mAllowSleeping = true;
        }
        else {
            bcs.mMotionQuality = JPH::EMotionQuality::Discrete;
        }

        JPH::EActivation act = prop.startAwake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
        JPH::BodyID id = bi->CreateAndAddBody(bcs, act);

        if (id.IsInvalid()) return;

        if (prop.motionType == 1) {
            if (prop.linearVelocity != glm::vec3(0.0f)) bi->SetLinearVelocity(id, JPH::Vec3(prop.linearVelocity.x, prop.linearVelocity.y, prop.linearVelocity.z));
            if (prop.angularVelocity != glm::vec3(0.0f)) bi->SetAngularVelocity(id, JPH::Vec3(prop.angularVelocity.x, prop.angularVelocity.y, prop.angularVelocity.z));
        }

        created_list.push_back(id);
        ecs.AddComponent<PhysicsBodyComponent>(e, PhysicsBodyComponent{ id, finalShape, true });
        s_lastLin[e] = prop.linearVelocity;
        s_lastAng[e] = prop.angularVelocity;
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

        // 1. Get Main Body Interface
        bodyInterface = &physicsSystem.GetBodyInterface();

        auto makeMaterial = [] {
            MaterialProperties m{};
            m.ambient = glm::vec3(0.05f, 0.07f, 0.10f);
            m.albedo = glm::vec3(0.80f, 0.60f, 0.30f);
            m.specular = glm::vec3(0.90f, 0.90f, 0.95f);
            m.shininess = 48.0f;
            m.opacity = 1.0f;
            return m;
            };
        MaterialProperties materialComponent = makeMaterial();

        struct Spawn {
            glm::vec3 pos;
            glm::vec3 scale;
            int        motionType;    // 0=Static, 1=Dynamic
            int        category;
            glm::vec3 linVel;
            glm::vec3 angVel;
            float      gravityFactor;
        };

        const Spawn spawns[4]{
            { glm::vec3{50.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{60.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{-5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{50.0f, 15.0f, 15.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{40.0f, 10.0f, -5.5f}, glm::vec3{5.0f}, 1, 1, glm::vec3{ 0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 2.0f}, 1.0f },
        };

        // ... (makeCubeEntity helper omitted for brevity, logic remains same) ...
        auto makeCubeEntity = [&](const Spawn& s) -> Entity {
            TransformComponent tf{};
            tf.position = s.pos;
            tf.scale = s.scale;
            tf.rotation = glm::vec3(0);

            Entity e = ecsInterface.CreateEntity();
            ecsInterface.AddComponent<HierarchyComponent>(e, HierarchyComponent{});
            ecsInterface.AddComponent<TransformComponent>(e, tf);

            MeshProperties mesh{};
            mesh.debugShape = static_cast<DebugShapeType>(1); // Cube
            ecsInterface.AddComponent<MeshProperties>(e, mesh);
            ecsInterface.AddComponent<MaterialProperties>(e, materialComponent);

            PhysicsProperties phys{};
            phys.motionType = s.motionType;
            phys.category = s.category;
            phys.gravityFactor = s.gravityFactor;
            phys.linearVelocity = s.linVel;
            phys.angularVelocity = s.angVel;
            phys.startAwake = true;

            ecsInterface.AddComponent<PhysicsProperties>(e, phys);
            // Default single collider for testing
            BoxColliderComponent bc;
            BoxColliderEntry entry;
            entry.size = glm::vec3(1.0f);
            bc.colliders.push_back(entry);
            ecsInterface.AddComponent<BoxColliderComponent>(e, bc);

            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
            return e;
            };

        makeCubeEntity(spawns[0]);
        makeCubeEntity(spawns[1]);
        makeCubeEntity(spawns[2]);
        makeCubeEntity(spawns[3]);

        std::cout << "[Physics] Initialized.\n";
    }

    void PhysicsSystem::Update(float dt) {
        if (dt <= 0.0001f) return;

        // 1. Create Bodies
        for (Entity e : ecsInterface.GetEntities()) {
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
        }

        // --- [STEP 2: PROCESS JOLT EVENT QUEUE] ---
        {
            std::lock_guard<std::mutex> lock(s_TriggerMutex);
            for (const auto& ev : s_TriggerEvents) {
                if (bodyInterface->IsActive(ev.b1) == false && !bodyInterface->IsAdded(ev.b1)) continue;
                if (bodyInterface->IsActive(ev.b2) == false && !bodyInterface->IsAdded(ev.b2)) continue;

                Entity e1 = (Entity)bodyInterface->GetUserData(ev.b1);
                Entity e2 = (Entity)bodyInterface->GetUserData(ev.b2);
                // Jolt trigger logic here...
            }
            s_TriggerEvents.clear();
        }

        // --- [STEP 3: MANUAL PLAYER TRIGGER LOGIC] ---
        Entity playerEntity = ecsInterface.GetEntityByName("Player");

        if (ecsInterface.IsEntityValid(playerEntity) &&
            ecsInterface.HasComponent<TransformComponent>(playerEntity) &&
            ecsInterface.HasComponent<EventSenderComponent>(playerEntity))
        {
            auto& playerTrans = ecsInterface.GetComponent<TransformComponent>(playerEntity);
            auto& playerSender = ecsInterface.GetComponent<EventSenderComponent>(playerEntity);

            for (Entity e : ecsInterface.GetEntities()) {
                if (e == playerEntity) continue;

                if (!ecsInterface.HasComponent<BoxColliderComponent>(e) ||
                    !ecsInterface.HasComponent<TransformComponent>(e)) {
                    continue;
                }

                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(e);
                auto& transform = ecsInterface.GetComponent<TransformComponent>(e);

                // --- NEW LOOP: CHECK ALL COLLIDERS ON TARGET ENTITY ---
                for (auto& entry : colliderComp.colliders)
                {
                    bool isInside = IsPointInsideBox(playerTrans.position, transform, entry);

                    // CASE: ENTER
                    if (isInside && !entry.wasInsideLastFrame)
                    {
                        // Fire ALL matching senders
                        for (const auto& senderEntry : playerSender.senders)
                        {
                            if (senderEntry.triggerName == "OnCollision")
                            {
                                CollisionEventData payload;
                                payload.senderID = playerEntity;
                                payload.targetID = e;
                                payload.actionType = 1; // ENTER

                                EventManager::Instance().Fire(senderEntry.messageName, &payload);
                            }
                        }
                    }
                    // CASE: EXIT
                    else if (!isInside && entry.wasInsideLastFrame)
                    {
                        // Fire ALL matching senders
                        for (const auto& senderEntry : playerSender.senders)
                        {
                            if (senderEntry.triggerName == "OnCollision")
                            {
                                // std::cout << "[Physics] Player EXITED Entity " << e << std::endl;
                                CollisionEventData payload;
                                payload.senderID = playerEntity;
                                payload.targetID = e;
                                payload.actionType = 0; // EXIT

                                EventManager::Instance().Fire(senderEntry.messageName, &payload);
                            }
                        }
                    }

                    // Update state specifically for THIS collider entry
                    entry.wasInsideLastFrame = isInside;
                }
            }
        }

        // --- [STEP 4: PRE-PHYSICS SYNC] ---
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<PhysicsProperties>(e)) continue;

            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            // Sync 'Sensor' state:
            // If the entity has colliders, we calculate if it should be a sensor.
            if (ecsInterface.HasComponent<BoxColliderComponent>(e)) {
                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(e);
                if (!colliderComp.colliders.empty()) {
                    bool anySolid = false;
                    for (const auto& c : colliderComp.colliders) {
                        if (!c.canPassThrough) anySolid = true;
                    }
                    // If any part is solid, the body is solid.
                    bodyInterface->SetIsSensor(pb.ID, !anySolid);
                }
            }

            if (prop.isImmovable == true) prop.motionType = 0;
            else if (prop.motionType == 0) prop.motionType = 1;

            ApplyPropsToBody(bodyInterface, pb.ID, prop);

            if (prop.motionType == 1) {
                // Apply velocity logic...
                bool hasInputVel = glm::length(prop.linearVelocity) > 0.01f;
                if (hasInputVel) {
                    JPH::Vec3 currentVel = bodyInterface->GetLinearVelocity(pb.ID);
                    bodyInterface->SetLinearVelocity(pb.ID, JPH::Vec3(prop.linearVelocity.x, currentVel.GetY(), prop.linearVelocity.z));
                    bodyInterface->ActivateBody(pb.ID);
                }
                else {
                    JPH::Vec3 currentVel = bodyInterface->GetLinearVelocity(pb.ID);
                    bodyInterface->SetLinearVelocity(pb.ID, JPH::Vec3(0.0f, currentVel.GetY(), 0.0f));
                    bodyInterface->SetAngularVelocity(pb.ID, JPH::Vec3(0, 0, 0));
                }
                if (glm::length(prop.angularVelocity) <= 0.01f) {
                    bodyInterface->SetAngularVelocity(pb.ID, JPH::Vec3(0, 0, 0));
                }
            }
            else {
                // Sync Static/Kinematic objects to transform
                if (ecsInterface.HasComponent<TransformComponent>(e)) {
                    auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
                    bodyInterface->SetPositionAndRotation(pb.ID, JPH::RVec3(tf.position.x, tf.position.y, tf.position.z), MakeQuatFromEuler(tf.rotation), JPH::EActivation::Activate);
                }
            }
        }

        // --- [STEP 5: RUN PHYSICS] ---
        physicsSystem.Update(dt, 1, tempAllocator, jobSystem);

        // --- [STEP 6: POST-PHYSICS SYNC] ---
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<TransformComponent>(e) || !ecsInterface.HasComponent<PhysicsProperties>(e)) continue;

            auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            if (prop.motionType == 1) {
                const JPH::RVec3 p = bodyInterface->GetPosition(pb.ID);
                tf.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());
                JPH::Quat q = bodyInterface->GetRotation(pb.ID);
                tf.rotation = QuatToEulerDegrees_Jolt(q);
            }
        }
    }

    // --- Raycast Implementation ---
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
        s_lastLin.clear();
        s_lastAng.clear();
        delete jobSystem;
        delete tempAllocator;
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}