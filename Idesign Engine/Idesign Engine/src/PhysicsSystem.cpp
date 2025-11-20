///////////////////////////////////////////////////////////////////////////////
///
/// @file PhysicsSystem.cpp
/// 
/// @brief A physics system which moves entities and detects cube collisions.
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "PhysicsSystem.h"
#include "ECSystemManager.h"
#include "ComponentList.h"
#include "ImguiManager.h"
#include "Terrain.h"
#include "Graphics.h"
#include "PhysicsProperties.h"

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
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CollisionCollector.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h> // <-- ADD THIS INCLUDE
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

// UI enum -> Jolt motion type
// THIS IS YOUR IMMOVABLE IMPLEMENTATION
// 0 = Static (Immovable)
// 1 = Dynamic (Movable with Gravity)
static inline JPH::EMotionType ToJoltMotionType(int motionType) {
    switch (motionType) {
    case 0:  return JPH::EMotionType::Static;    // <--- IMMOVABLE
    case 2:  return JPH::EMotionType::Kinematic; // <--- MOVES BY SCRIPT, PUSHES OTHERS
    case 1:
    default: return JPH::EMotionType::Dynamic;   // <--- MOVABLE (Gravity applies)
    }
}

// UI "category" -> object layer (expand as needed)
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

// Apply props that are safe every frame
static inline void ApplyPropsToBody(JPH::BodyInterface* bi, JPH::BodyID id, const Framework::PhysicsProperties& p) {
    using namespace JPH;

    // --- IMPLEMENTING IMMOVABLE LOGIC HERE ---
    // We use the Integer 'motionType' to determine if it's static.
    const auto want = ToJoltMotionType(p.motionType);

    if (bi->GetMotionType(id) != want) {
        // This switches the body between Static (Immovable) and Dynamic (Movable)
        // in real-time if you change the value in the editor.
        bi->SetMotionType(id, want, EActivation::Activate);
    }
    bi->SetGravityFactor(id, p.gravityFactor);
}

// Broadphase layers (simple 1-layer world)
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
    /// Filter that allows collision with all broadphase layers
    class AllowAllBroadPhaseFilter : public JPH::BroadPhaseLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
    };

    /// Filter that allows collision with all object layers
    class AllowAllObjectFilter : public JPH::ObjectLayerFilter {
    public:
        virtual bool ShouldCollide(JPH::ObjectLayer) const override { return true; }
    };

    /// Filter that allows collision ONLY with a specific object layer
    class SpecificCategoryFilter : public JPH::ObjectLayerFilter {
    public:
        SpecificCategoryFilter(JPH::ObjectLayer layer) : mLayer(layer) {}
        virtual bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
            return mLayer == inLayer;
        }
    private:
        JPH::ObjectLayer mLayer;
    };
} // namespace
// --- END NEW (REVISED HELPER CLASSES) ---

namespace Framework {
    static std::unordered_map<Entity, glm::vec3> s_lastLin, s_lastAng;

    static JPH::BodyID AddTerrainGround(const Terrain& terrain, int gridSize, float cellScale, JPH::PhysicsSystem& physics, JPH::BodyInterface* bi,
        std::vector<JPH::BodyID>& out_createdBodies, const JPH::RVec3& worldOffset = JPH::RVec3(0, 0, 0)) {
        // ... (Assuming you kept your Terrain logic, commented out or not) ...
        // For brevity, assuming standard implementation or commented out as discussed
        return JPH::BodyID();
    }

    static void EnsureBodyForEntity(Entity e, Framework::Coordinator& ecs, JPH::BodyInterface* bi, std::vector<JPH::BodyID>& created_list) {
        if (bi == nullptr) return;

        if (!ecs.HasComponent<TransformComponent>(e) || !ecs.HasComponent<PhysicsProperties>(e)) return;
        if (ecs.HasComponent<PhysicsBodyComponent>(e)) return;

        JPH::ShapeRefC shape = nullptr;
        auto& tf = ecs.GetComponent<TransformComponent>(e);

        if (ecs.HasComponent<BoxColliderComponent>(e)) {
            auto& collider = ecs.GetComponent<BoxColliderComponent>(e);
            JPH::Vec3 half_extents(
                std::abs(0.5f * collider.size.x * tf.scale.x),
                std::abs(0.5f * collider.size.y * tf.scale.y),
                std::abs(0.5f * collider.size.z * tf.scale.z)
            );

            const float kMinSize = 0.001f;
            if (half_extents.GetX() < kMinSize || half_extents.GetY() < kMinSize || half_extents.GetZ() < kMinSize) return;

            JPH::BoxShapeSettings bs(half_extents);
            JPH::ShapeSettings::ShapeResult result = bs.Create();
            if (result.HasError()) {
                std::cerr << "[Physics Error] Entity " << e << " Shape failed: " << result.GetError().c_str() << std::endl;
                return;
            }
            shape = result.Get();

            if (collider.center != glm::vec3(0.0f)) {
                JPH::Vec3 jolt_center(collider.center.x, collider.center.y, collider.center.z);
                shape = new JPH::OffsetCenterOfMassShape(shape, jolt_center);
            }
        }

        if (shape == nullptr) return;

        auto& prop = ecs.GetComponent<PhysicsProperties>(e);

        JPH::BodyCreationSettings bcs(
            shape,
            JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
            MakeQuatFromEuler(tf.rotation),
            ToJoltMotionType(prop.motionType), // <--- Determines Immovable vs Movable on creation
            ToObjectLayer(prop.category)
        );

        bcs.mGravityFactor = prop.gravityFactor;
        bcs.mUserData = static_cast<JPH::uint64>(e);

        if (prop.motionType == 1) { // Dynamic
            bcs.mMotionQuality = JPH::EMotionQuality::LinearCast;
            bcs.mAllowSleeping = true;
        }
        else {
            bcs.mMotionQuality = JPH::EMotionQuality::Discrete;
        }

        JPH::EActivation act = prop.startAwake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate;
        JPH::BodyID id = bi->CreateAndAddBody(bcs, act);

        if (id.IsInvalid()) return;

        // Apply velocities if Dynamic
        if (prop.motionType == 1) {
            if (prop.linearVelocity != glm::vec3(0.0f)) {
                bi->SetLinearVelocity(id, JPH::Vec3(prop.linearVelocity.x, prop.linearVelocity.y, prop.linearVelocity.z));
            }
            if (prop.angularVelocity != glm::vec3(0.0f)) {
                bi->SetAngularVelocity(id, JPH::Vec3(prop.angularVelocity.x, prop.angularVelocity.y, prop.angularVelocity.z));
            }
        }

        created_list.push_back(id);
        ecs.AddComponent<PhysicsBodyComponent>(e, PhysicsBodyComponent{ id, shape, true });
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

        // --- NEW ---
        // Set the contact listener's body lock interface
        g_ContactListener.mBodyLockInterface = &physicsSystem.GetBodyLockInterface();
        // --- END NEW ---

        bodyInterface = &physicsSystem.GetBodyInterface();
        g_ContactListener.mBodyInterface = bodyInterface;
        physicsSystem.SetContactListener(&g_ContactListener);
        physicsSystem.SetGravity(JPH::Vec3(0, -9.81f, 0));

        // AddTerrainGround(...); // Commented out as per request

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
            int       motionType;    // 0=Static, 1=Dynamic
            int       category;
            glm::vec3 linVel;
            glm::vec3 angVel;
            float     gravityFactor;
        };

        // --- FIXED SPAWNS: Changed 0.0f Gravity to 1.0f Gravity ---
        const Spawn spawns[4]{
            { glm::vec3{50.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{60.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{-5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{50.0f, 15.0f, 15.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{40.0f, 10.0f, -5.5f}, glm::vec3{5.0f}, 1, 1, glm::vec3{ 0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 2.0f}, 1.0f },
        };

        // ... (makeCubeEntity helper is same as before) ...
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
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
            return e;
            };

        makeCubeEntity(spawns[0]);
        makeCubeEntity(spawns[1]);
        makeCubeEntity(spawns[2]);
        makeCubeEntity(spawns[3]);

        // Link existing scene entities
        for (Entity e : ecsInterface.GetEntities())
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);

        std::cout << "[Physics] Initialized.\n";
    }

    void PhysicsSystem::Update(float dt) {
        // 0. Safety Check: Prevent running physics if time is paused/invalid
        if (dt <= 0.0001f) return;

        // 1. Create bodies for new entities
        for (Entity e : ecsInterface.GetEntities()) {
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
        }

        // 2. Pre-Physics: Apply ECS properties to Jolt
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) ||
                !ecsInterface.HasComponent<PhysicsProperties>(e)) {
                continue;
            }

            // Inside PhysicsSystem::Update, right at the start of the loop

            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            // If your component still has the 'bool isImmovable' member:
            if (prop.isImmovable == true) {
                prop.motionType = 0; // Force Static
            }
            else {
                // Only default to Dynamic if it was 0. 
                // This prevents overwriting Kinematic (2) if you have that logic.
                if (prop.motionType == 0) prop.motionType = 1;
            }
            // Sync basic properties (Immovable vs Movable, Gravity)
            ApplyPropsToBody(bodyInterface, pb.ID, prop);

            if (prop.motionType == 1) {

                bool hasInputVel = glm::length(prop.linearVelocity) > 0.01f;

                if (hasInputVel) {
                    // [CASE A] Player is Inputting Movement -> Force Velocity
                    JPH::Vec3 currentVel = bodyInterface->GetLinearVelocity(pb.ID);
                    float finalX = prop.linearVelocity.x;
                    float finalY = currentVel.GetY(); // Preserve Gravity
                    float finalZ = prop.linearVelocity.z;

                    bodyInterface->SetLinearVelocity(pb.ID, JPH::Vec3(finalX, finalY, finalZ));
                    bodyInterface->ActivateBody(pb.ID);
                }
                else {
                    // [CASE B] No Input -> STOP INSTANTLY
                    JPH::Vec3 currentVel = bodyInterface->GetLinearVelocity(pb.ID);

                    // Force X and Z to 0. Keep Y (Gravity).
                    bodyInterface->SetLinearVelocity(pb.ID, JPH::Vec3(0.0f, currentVel.GetY(), 0.0f));

                    // Optional: If you want to kill rotation too
                    bodyInterface->SetAngularVelocity(pb.ID, JPH::Vec3(0, 0, 0));
                }

                // Handle Angular Velocity (Rotation)
                if (glm::length(prop.angularVelocity) > 0.01f) {
                    // ... existing rotation code ...
                }
                else {
                    // Optional: Kill rotation when not turning so you don't spin like a top if hit
                    bodyInterface->SetAngularVelocity(pb.ID, JPH::Vec3(0, 0, 0));
                }
            }
            // --- STATIC / KINEMATIC OBJECTS ---
            else {
                // Snap Physics Body to Entity Transform (e.g. moving platform)
                if (ecsInterface.HasComponent<TransformComponent>(e)) {
                    auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
                    bodyInterface->SetPositionAndRotation(
                        pb.ID,
                        JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
                        MakeQuatFromEuler(tf.rotation),
                        JPH::EActivation::Activate
                    );
                }
            }
        }

        // 3. Step Physics
        physicsSystem.Update(dt, 1, tempAllocator, jobSystem);

        // 4. Post-Physics: Sync Jolt positions back to ECS
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) ||
                !ecsInterface.HasComponent<TransformComponent>(e) ||
                !ecsInterface.HasComponent<PhysicsProperties>(e)) {
                continue;
            }

            auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            // Only update Transform if the object is Dynamic
            if (prop.motionType == 1) {
                const JPH::RVec3 p = bodyInterface->GetPosition(pb.ID);
                tf.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());

                JPH::Quat q = bodyInterface->GetRotation(pb.ID);
                tf.rotation = QuatToEulerDegrees_Jolt(q);
            }
        }
    }

    // --- NEW (REVISED FUNCTION V4) ---
    bool PhysicsSystem::Raycast(const glm::vec3& origin, const glm::vec3& direction, float maxDistance, RaycastHit& outHit, int targetCategory)
    {
        // Get the Jolt query interface
        auto* narrowPhaseQuery = &physicsSystem.GetNarrowPhaseQuery();

        // Jolt's RRayCast takes a direction vector scaled by the max distance
        JPH::RRayCast ray(
            JPH::RVec3(origin.x, origin.y, origin.z),
            JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance
        );

        JPH::RayCastResult hit_result; // <-- This line will now compile (C2079 fix)
        bool did_hit = false;

        // --- REVISED LOGIC ---
        // Always allow all broadphase layers
        AllowAllBroadPhaseFilter bp_filter;

        if (targetCategory == -1)
        {
            // Category -1 means hit anything. Use the "AllowAll" object filter.
            AllowAllObjectFilter obj_filter;
            did_hit = narrowPhaseQuery->CastRay( // <-- This line will now compile (C2665 fix)
                ray,
                hit_result,
                bp_filter,
                obj_filter
            );
        }
        else
        {
            // We have a specific category. Use our custom "SpecificCategory" filter.
            SpecificCategoryFilter obj_filter(ToObjectLayer(targetCategory));
            did_hit = narrowPhaseQuery->CastRay( // <-- This line will now compile (C2665 fix)
                ray,
                hit_result,
                bp_filter,
                obj_filter // Pass our specific filter
            );
        }
        // --- END REVISED LOGIC ---

        if (did_hit)
        {
            // A hit occurred, now get the body ID
            JPH::BodyID body_id = hit_result.mBodyID;

            // We need to lock the body to safely read its user data (our Entity ID)
            auto& body_lock_interface = physicsSystem.GetBodyLockInterface();
            JPH::BodyLockRead lock(body_lock_interface, body_id);

            if (lock.Succeeded())
            {
                const JPH::Body& body = lock.GetBody();

                // Populate the outHit struct
                outHit.entity = static_cast<Entity>(body.GetUserData());
                outHit.hit = true;

                // Calculate the world space hit point
                JPH::RVec3 hit_point_world = ray.mOrigin + ray.mDirection * hit_result.mFraction;
                outHit.point = glm::vec3(hit_point_world.GetX(), hit_point_world.GetY(), hit_point_world.GetZ());

                // Calculate the world space normal
                JPH::Vec3 normal_world = body.GetWorldSpaceSurfaceNormal(hit_result.mSubShapeID2, hit_point_world);
                outHit.normal = glm::vec3(normal_world.GetX(), normal_world.GetY(), normal_world.GetZ());

                // The distance is the fraction multiplied by the original max distance
                outHit.distance = hit_result.mFraction * maxDistance;

                return true;
            }
        }

        // No hit or lock failed
        outHit.hit = false;
        return false;
    }
    // --- END NEW (REVISED FUNCTION V4) ---

    std::string PhysicsSystem::GetName() {
        return "PhysicsSystem";
    }

    void PhysicsSystem::Shutdown() {
        std::cout << "[Physics] Cleaning up Jolt...\n";

        // 1. Destroy Jolt Bodies (Existing Code)
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

        // 2. [NEW FIX] Clear ECS Components so we don't use dead IDs next time
        // Iterate over all entities and remove the PhysicsBodyComponent if they have it.
        for (Entity e : ecsInterface.GetEntities()) {
            if (ecsInterface.HasComponent<PhysicsBodyComponent>(e)) {
                ecsInterface.RemoveComponent<PhysicsBodyComponent>(e);
            }
        }

        // 3. Clear Caches (Existing Code)
        s_lastLin.clear();
        s_lastAng.clear();

        delete jobSystem;
        delete tempAllocator;

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}