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

#include <glm/glm/glm.hpp>
#include <glm/glm/gtc/quaternion.hpp>

#include <unordered_map>
#include <iostream>
#include <cmath>
#include <vector>

extern Framework::Coordinator ecsInterface;
std::vector<JPH::BodyID> m_createdBodies;

namespace Layers {
    static constexpr JPH::ObjectLayer MOVING = 0;
}

// ---------- Small helpers ----------

// UI enum -> Jolt motion type
static inline JPH::EMotionType ToJoltMotionType(int motionType) {
    switch (motionType) {
    case 0:  return JPH::EMotionType::Static;
    case 2:  return JPH::EMotionType::Kinematic;
    case 1:
    default: return JPH::EMotionType::Dynamic;
    }
}

// UI "category" -> object layer (expand as needed)
static inline JPH::ObjectLayer ToObjectLayer(int category) {
    // example mapping; change to what your game needs
    switch (category) {
    case 0: return 1; // Wall
    case 1: return 2; // Collectible
    case 2: return 3; // Player
    case 3: return 4; // Customer
    default: return Layers::MOVING; // fallback
    }
}

// Build a Jolt box from ECS scale (half-extents)
static inline JPH::ShapeRefC MakeBoxShapeFromScale(const glm::vec3& scale) {
    JPH::Vec3 half(0.5f * scale.x, 0.5f * scale.y, 0.5f * scale.z);
    JPH::BoxShapeSettings bs(half);
    return bs.Create().Get();
}

static inline float DegToRad(float deg) { return deg * 0.01745329251994329577f; }

// Editor Euler (deg) -> Jolt quaternion
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

    // Pitch (X)
    float sinp = 2.0f * (w * x - z * y);
    float pitch = std::abs(sinp) >= 1.0f ? std::copysign(0.5f * 3.1415926535f, sinp) : std::asin(sinp);

    // Yaw (Y)
    float siny_cosp = 2.0f * (w * y + x * z);
    float cosy_cosp = 1.0f - 2.0f * (x * x + y * y);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    // Roll (Z)
    float sinr_cosp = 2.0f * (w * z + x * y);
    float cosr_cosp = 1.0f - 2.0f * (y * y + z * z);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    const float R2D = 57.2957795f;
    return glm::vec3(pitch * R2D, yaw * R2D, roll * R2D);
}


// Apply props that are safe every frame (no velocity stamping here)
static inline void ApplyPropsToBody(JPH::BodyInterface* bi, JPH::BodyID id, const Framework::PhysicsProperties& p) {
    using namespace JPH;
    const auto want = ToJoltMotionType(p.motionType);
    if (bi->GetMotionType(id) != want) {
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

namespace Framework {
    // ---- Per-entity caches so editor velocity changes apply once ----
    static std::unordered_map<Entity, glm::vec3> s_lastLin, s_lastAng;

    // Terrain -> static triangle mesh
    static JPH::BodyID AddTerrainGround(const Terrain& terrain, int gridSize, float cellScale, JPH::PhysicsSystem& physics, JPH::BodyInterface* bi, 
    std::vector<JPH::BodyID>& out_createdBodies, const JPH::RVec3& worldOffset = JPH::RVec3(0, 0, 0)) {
        using namespace JPH;

        VertexList verts;
        verts.reserve(static_cast<size_t>(gridSize) * gridSize);

        auto idx = [gridSize](int x, int z) { return z * gridSize + x; };

        for (int z = 0; z < gridSize; ++z) {
            float worldZ = z * cellScale + (float)worldOffset.GetZ();
            for (int x = 0; x < gridSize; ++x) {
                float worldX = x * cellScale + (float)worldOffset.GetX();
                float h = terrain.getHeightAt(worldX - (float)worldOffset.GetX(),
                    worldZ - (float)worldOffset.GetZ());
                float worldY = h + (float)worldOffset.GetY();
                verts.emplace_back(worldX, worldY, worldZ);
            }
        }

        IndexedTriangleList tris;
        tris.reserve((size_t)(gridSize - 1) * (gridSize - 1) * 2);
        for (int z = 0; z < gridSize - 1; ++z) {
            for (int x = 0; x < gridSize - 1; ++x) {
                uint32_t i0 = (uint32_t)idx(x, z);
                uint32_t i1 = (uint32_t)idx(x + 1, z);
                uint32_t i2 = (uint32_t)idx(x, z + 1);
                uint32_t i3 = (uint32_t)idx(x + 1, z + 1);

                tris.emplace_back(i0, i2, i1);
                tris.emplace_back(i1, i2, i3);
            }
        }

        MeshShapeSettings ms(verts, tris);
        ShapeRefC shape = ms.Create().Get();

        BodyCreationSettings bcs(shape, worldOffset, Quat::sIdentity(), EMotionType::Static, Layers::MOVING);
        BodyID id = bi->CreateAndAddBody(bcs, EActivation::DontActivate);
        out_createdBodies.push_back(id);

        std::cout << "[Physics] Terrain ground added: tris=" << tris.size() << "\n";
        return id;
    }

    // Ensure a Jolt body exists for (Transform + PhysicsProperties)
    static void EnsureBodyForEntity(Entity e, Framework::Coordinator& ecs, JPH::BodyInterface* bi, std::vector<JPH::BodyID>& created_list) {
        if (!ecs.HasComponent<TransformComponent>(e) || !ecs.HasComponent<PhysicsProperties>(e)) {
            return;
        }

        if (ecs.HasComponent<PhysicsBodyComponent>(e)) {
            return;
        }

        auto& tf = ecs.GetComponent<TransformComponent>(e);
        auto& prop = ecs.GetComponent<PhysicsProperties>(e);

        // Build shape from scale (or your own debugShape switch)
        JPH::ShapeRefC shape = MakeBoxShapeFromScale(tf.scale);

        // Body settings from properties
        JPH::BodyCreationSettings bcs(
            shape,
            JPH::RVec3(tf.position.x, tf.position.y, tf.position.z),
            JPH::Quat::sIdentity(),
            ToJoltMotionType(prop.motionType),
            ToObjectLayer(prop.category)
        );

        bcs.mGravityFactor = prop.gravityFactor;

        JPH::EActivation act = prop.startAwake ? JPH::EActivation::Activate
            : JPH::EActivation::DontActivate;

        JPH::BodyID id = bi->CreateAndAddBody(bcs, act);

        // Initial velocities (once at creation)
        if (prop.linearVelocity != glm::vec3(0.0f)) {
            bi->SetLinearVelocity(id, JPH::Vec3(prop.linearVelocity.x, prop.linearVelocity.y, prop.linearVelocity.z));
        }
        if (prop.angularVelocity != glm::vec3(0.0f)){
            bi->SetAngularVelocity(id, JPH::Vec3(prop.angularVelocity.x, prop.angularVelocity.y, prop.angularVelocity.z));
        }

        created_list.push_back(id);

        // Store ECS linkage
        ecs.AddComponent<PhysicsBodyComponent>(e, PhysicsBodyComponent{ id, shape, true });

        // Seed caches to current values so we don't re-apply immediately
        s_lastLin[e] = prop.linearVelocity;
        s_lastAng[e] = prop.angularVelocity;
    }

    void PhysicsSystem::Initialize() {
        using namespace JPH;

        // --- Jolt world bootstrap ---
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);
        jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

        static BPLayerInterfaceImpl broadPhaseInterface;
        static AllowAllObjVsBP      objVsBP;
        static AllowAllPairFilter   pairFilter;

        physicsSystem.Init(1024, 0, 1024, 1024, broadPhaseInterface, objVsBP, pairFilter);
        bodyInterface = &physicsSystem.GetBodyInterface();
        physicsSystem.SetGravity(JPH::Vec3(0, -9.81f, 0));

        // --- Terrain as static triangle mesh ground ---
        AddTerrainGround(terrain, TerrainSize, TerrainScale, physicsSystem, bodyInterface,
            m_createdBodies, JPH::RVec3(0, 0, 0));

        // ===== Create 4 cubes (ECS) =====
        static int selectedShapeIndex = 1;

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
            int       motionType;    // 0=Static, 1=Dynamic, 2=Kinematic
            int       category;      // your layer mapping
            glm::vec3 linVel;
            glm::vec3 angVel;
            float     gravityFactor;
        };

        const Spawn spawns[4]{
            { glm::vec3{50.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f },
            { glm::vec3{60.5f,  5.0f,  1.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{-5.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.0f },
            { glm::vec3{50.0f, 15.0f, 15.5f}, glm::vec3{5.0f}, 1, 2, glm::vec3{ 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 1.0f },
            { glm::vec3{40.0f, 10.0f, -5.5f}, glm::vec3{5.0f}, 1, 1, glm::vec3{ 0.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 2.0f}, 0.0f },
        };

        auto makeCubeEntity = [&](const Spawn& s) -> Entity {
            TransformComponent tf{};
            tf.position = s.pos;
            tf.scale = s.scale;
            tf.rotation = glm::vec3(0);

            Entity e = ecsInterface.CreateEntity();
            ecsInterface.AddComponent<HierarchyComponent>(e, HierarchyComponent{});
            ecsInterface.AddComponent<TransformComponent>(e, tf);

            MeshProperties mesh{};
            MeshLoader::loadMesh(mesh.path, mesh);
            mesh.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
            ecsInterface.AddComponent<MeshProperties>(e, mesh);
            ecsInterface.AddComponent<MaterialProperties>(e, materialComponent);

            // PhysicsProperties for editor + Jolt creation
            PhysicsProperties phys{};
            phys.motionType = s.motionType;
            phys.category = s.category;
            phys.gravityFactor = s.gravityFactor;
            phys.linearVelocity = s.linVel;
            phys.angularVelocity = s.angVel;
            phys.startAwake = true;

            ecsInterface.AddComponent<PhysicsProperties>(e, phys);

            // Spawn the body now
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
            return e;
            };

        Entity cube1 = makeCubeEntity(spawns[0]);
        Entity cube2 = makeCubeEntity(spawns[1]);
        Entity cube3 = makeCubeEntity(spawns[2]);
        Entity cube4 = makeCubeEntity(spawns[3]);

        // In case other scene entities already had PhysicsProperties:
        for (Entity e : ecsInterface.GetEntities())
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);

        std::cout << "[Physics] Linked ECS cubes and Jolt bodies.\n";
    }

    void PhysicsSystem::Update(float dt) {
        // Make sure new entities that just got PhysicsProperties receive a body.
        for (Entity e : ecsInterface.GetEntities()) {
            EnsureBodyForEntity(e, ecsInterface, bodyInterface, m_createdBodies);
        }

        // ===== ECS -> Jolt: apply inspector edits before stepping =====
        auto changed = [](const glm::vec3& a, const glm::vec3& b) {
                glm::vec3 d = a - b;
                return (d.x * d.x + d.y * d.y + d.z * d.z) > 1e-12f;
            };

        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<PhysicsProperties>(e)) {
                continue;
            }

            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);
            auto& prop = ecsInterface.GetComponent<PhysicsProperties>(e);

            // Safe props each frame
            ApplyPropsToBody(bodyInterface, pb.ID, prop);

            // Dynamic: apply new velocities only when they change
            if (prop.motionType == 1) {
                auto itL = s_lastLin.find(e);
                if (itL == s_lastLin.end() || changed(itL->second, prop.linearVelocity)) {
                    bodyInterface->SetLinearVelocity(pb.ID, JPH::Vec3(prop.linearVelocity.x, prop.linearVelocity.y, prop.linearVelocity.z));
                    s_lastLin[e] = prop.linearVelocity;
                }

                auto itA = s_lastAng.find(e);
                if (itA == s_lastAng.end() || changed(itA->second, prop.angularVelocity)) {
                    bodyInterface->SetAngularVelocity(pb.ID, JPH::Vec3(prop.angularVelocity.x, prop.angularVelocity.y, prop.angularVelocity.z));
                    s_lastAng[e] = prop.angularVelocity;
                }
            }
            else {
                // Not Dynamic: drive pose from Transform to respect editor moves
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

        // ===== Step physics =====
        physicsSystem.Update(dt, 1, tempAllocator, jobSystem);

        // ===== Jolt -> ECS: sync back for rendering =====
        for (Entity e : ecsInterface.GetEntities()) {
            if (!ecsInterface.HasComponent<PhysicsBodyComponent>(e) || !ecsInterface.HasComponent<TransformComponent>(e)) {
                continue;
            }

            auto& tf = ecsInterface.GetComponent<TransformComponent>(e);
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(e);

            const JPH::RVec3 p = bodyInterface->GetPosition(pb.ID);
            tf.position = glm::vec3(p.GetX(), p.GetY(), p.GetZ());

            JPH::Quat q = bodyInterface->GetRotation(pb.ID);
            tf.rotation = QuatToEulerDegrees_Jolt(q);
        }
    }

    std::string PhysicsSystem::GetName() {
        return "PhysicsSystem";
    }

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

        // Clear velocity caches
        s_lastLin.clear();
        s_lastAng.clear();

        delete jobSystem;
        delete tempAllocator;

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

