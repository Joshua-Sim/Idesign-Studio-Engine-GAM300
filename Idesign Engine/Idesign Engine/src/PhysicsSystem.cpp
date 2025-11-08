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

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

#include <iostream>
#include <cmath>

extern Framework::Coordinator ecsInterface;
std::vector<JPH::BodyID> m_createdBodies;

namespace Layers {
    static constexpr JPH::ObjectLayer MOVING = 0;
}

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
    public:
        JPH::uint GetNumBroadPhaseLayers() const override { return 1; }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override { return JPH::BroadPhaseLayer(0); }
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

//void PhysicsSystem::OnMouseDown(int button, float mx, float my,
//    const glm::mat4& V, const glm::mat4& P,
//    int W, int H)
//{
//    JPH::RVec3 o; JPH::Vec3 d;
//    MakeRay(mx, my, W, H, V, P, o, d);
//
//    JPH::RayCastResult hit;
//    if (!RayCastClosest(physicsSystem, o, d, 1000.0f, hit)) return;
//
//    // Optional: ignore terrain / non-box shapes
//    JPH::ShapeRefC shape = bodyInterface->GetShape(hit.mBodyID);
//    if (shape->GetSubType() != JPH::EShapeSubType::Box) return;
//
//    if (button == 0) { // LMB: report which face we clicked
//        JPH::RVec3 hitWS = o + JPH::RVec3(d * hit.mFraction);
//        int face = FaceIndexForBox(bodyInterface, hit.mBodyID, hitWS);
//        std::cout << "[Pick] Body " << hit.mBodyID.GetIndex() << " face " << face << "\n";
//        return;
//    }
//
//    if (button == 1) { // RMB: begin drag-rotate
//        mPickedID = hit.mBodyID;
//        // remember original motion type, switch to kinematic so we can set transforms directly
//        mPrevMotionType = bodyInterface->GetMotionType(mPickedID);
//        bodyInterface->SetMotionType(mPickedID, JPH::EMotionType::Kinematic, JPH::EActivation::Activate);
//
//        mRotating = true;
//        mDragStart = { mx, my };
//        mStartRot = bodyInterface->GetRotation(mPickedID);
//        bodyInterface->ActivateBody(mPickedID);
//    }
//}
//
//void PhysicsSystem::OnMouseDrag(float mx, float my,
//    const glm::vec3& camRight, const glm::vec3& camUp)
//{
//    if (!mRotating || mPickedID.IsInvalid()) return;
//
//    float dx = (mx - mDragStart.x) * 0.005f;
//    float dy = (my - mDragStart.y) * 0.005f;
//
//    JPH::Quat qYaw = JPH::Quat::sRotation(JPH::Vec3(camUp.x, camUp.y, camUp.z), -dx);
//    JPH::Quat qPitch = JPH::Quat::sRotation(JPH::Vec3(camRight.x, camRight.y, camRight.z), -dy);
//    JPH::Quat target = qYaw * qPitch * mStartRot;
//
//    JPH::RVec3 pos = bodyInterface->GetPosition(mPickedID);
//    bodyInterface->SetPositionAndRotation(mPickedID, pos, target, JPH::EActivation::Activate);
//}
//
//void PhysicsSystem::OnMouseUp(int button)
//{
//    if (button == 1 && !mPickedID.IsInvalid()) {
//        // restore original motion type
//        bodyInterface->SetMotionType(mPickedID, mPrevMotionType, JPH::EActivation::Activate);
//        mRotating = false;
//        mPickedID = JPH::BodyID(); // invalidate
//    }
//}

namespace Framework {
    static JPH::BodyID AddTerrainGround(const Terrain& terrain, int gridSize, float cellScale, JPH::PhysicsSystem& physics, JPH::BodyInterface* bi,
    std::vector<JPH::BodyID>& out_createdBodies,const JPH::RVec3& worldOffset = JPH::RVec3(0, 0, 0)) {
        using namespace JPH;

        VertexList verts;
        verts.reserve(static_cast<size_t>(gridSize) * gridSize);

        auto idx = [gridSize](int x, int z) { 
            return z * gridSize + x; 
        };

        for (int z = 0; z < gridSize; ++z) {
            float worldZ = z * cellScale + (float)worldOffset.GetZ();
            for (int x = 0; x < gridSize; ++x) {
                float worldX = x * cellScale + (float)worldOffset.GetX();
                float h = terrain.getHeightAt(worldX - (float)worldOffset.GetX(), worldZ - (float)worldOffset.GetZ());
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

    // screen -> world ray
    //static void MakeRay(float mx, float my, int W, int H,
    //    const glm::mat4& V, const glm::mat4& P,
    //    JPH::RVec3& outO, JPH::Vec3& outDirN)
    //{
    //    float nx = 2.0f * mx / float(W) - 1.0f;
    //    float ny = 1.0f - 2.0f * my / float(H);
    //    glm::mat4 invVP = glm::inverse(P * V);
    //    glm::vec4 p0 = invVP * glm::vec4(nx, ny, 0.0f, 1.0f);
    //    glm::vec4 p1 = invVP * glm::vec4(nx, ny, 1.0f, 1.0f);
    //    p0 /= p0.w; p1 /= p1.w;
    //    glm::vec3 o = glm::vec3(p0);
    //    glm::vec3 d = glm::normalize(glm::vec3(p1 - p0));
    //    outO = JPH::RVec3(o.x, o.y, o.z);
    //    outDirN = JPH::Vec3(d.x, d.y, d.z);
    //}
    //static bool RayCastClosest(JPH::PhysicsSystem& ps,
    //    const JPH::RVec3& o, const JPH::Vec3& dirN, float maxDist,
    //    JPH::RayCastResult& outHit)
    //{
    //    JPH::RayCast rc;
    //    rc.mOrigin = o;
    //    rc.mDirection = dirN * maxDist; // length-scaled
    //    return ps.GetNarrowPhaseQuery().CastRay(rc, outHit);
    //}
    //// figure which cube face got clicked (1..6)
    //static int FaceIndexForBox(const JPH::BodyInterface* bi, JPH::BodyID id, const JPH::RVec3& hitWS)
    //{
    //    JPH::RMat44 xf = bi->GetCenterOfMassTransform(id);
    //    JPH::RVec3  rel = hitWS - xf.GetTranslation();
    //    JPH::Vec3   local = xf.GetRotation().Conjugated() * JPH::Vec3(rel);
    //    JPH::Vec3 a(JPH::Abs(local.GetX()), JPH::Abs(local.GetY()), JPH::Abs(local.GetZ()));
    //    if (a.GetX() >= a.GetY() && a.GetX() >= a.GetZ()) return local.GetX() > 0 ? 1 : 4; // +X / -X
    //    if (a.GetY() >= a.GetZ())                         return local.GetY() > 0 ? 2 : 5; // +Y / -Y
    //    return local.GetZ() > 0 ? 3 : 6;   // +Z / -Z
    //}

    void PhysicsSystem::Initialize() {
        using namespace JPH;

        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        tempAllocator = new TempAllocatorImpl(10 * 1024 * 1024);
        jobSystem = new JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);
        static BPLayerInterfaceImpl broadPhaseInterface;
        static AllowAllObjVsBP    objVsBP;
        static AllowAllPairFilter pairFilter;

        physicsSystem.Init(1024, 0, 1024, 1024,broadPhaseInterface,objVsBP,pairFilter); 
        bodyInterface = &physicsSystem.GetBodyInterface();
        physicsSystem.SetGravity(JPH::Vec3(0, -9.81f, 0));

        AddTerrainGround(terrain, TerrainSize, TerrainScale, physicsSystem, bodyInterface, m_createdBodies, JPH::RVec3(0, 0, 0) );

        // Create Cube
        static int selectedShapeIndex = 1;

        MaterialProperties materialComponent;
        materialComponent.ambient = glm::vec3(0.05f, 0.07f, 0.1f);   // Slightly cool ambient tone
        materialComponent.albedo = glm::vec3(0.8f, 0.6f, 0.3f);      // Warm diffuse color (like wood or bronze)
        materialComponent.specular = glm::vec3(0.9f, 0.9f, 0.95f);   // Strong specular for shiny surface
        materialComponent.shininess = 48.0f;                         // Fairly glossy
        materialComponent.opacity = 1.0f;                           // Slightly transparent

        TransformComponent cube1Transform;
        cube1Transform.position = glm::vec3(50.5f, 5.0f, 1.5f);
        cube1Transform.scale = glm::vec3(5.0f);
        cube1Transform.rotation = glm::vec3(0);

        Entity cube1 = ecsInterface.CreateEntity();
        ecsInterface.AddComponent<HierarchyComponent>(cube1, HierarchyComponent{});
        ecsInterface.AddComponent<TransformComponent>(cube1, cube1Transform);
        MeshProperties cube1mesh{};
        MeshLoader::loadMesh(cube1mesh.path, cube1mesh);
        cube1mesh.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
        ecsInterface.AddComponent<MeshProperties>(cube1, cube1mesh);
        ecsInterface.AddComponent<MaterialProperties>(cube1, materialComponent);

        TransformComponent cube2Transform;
        cube2Transform.position = glm::vec3(60.5f, 5.0f, 1.5f);
        cube2Transform.scale = glm::vec3(5.0f);
        cube2Transform.rotation = glm::vec3(0);

        Entity cube2 = ecsInterface.CreateEntity();
        ecsInterface.AddComponent<HierarchyComponent>(cube2, HierarchyComponent{});
        ecsInterface.AddComponent<TransformComponent>(cube2, cube2Transform);
        MeshProperties cube2mesh{};
        MeshLoader::loadMesh(cube2mesh.path, cube2mesh);
        cube2mesh.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
        ecsInterface.AddComponent<MeshProperties>(cube2, cube2mesh);
        ecsInterface.AddComponent<MaterialProperties>(cube2, materialComponent);


        TransformComponent cube3Transform;
        cube3Transform.position = glm::vec3(50.0f, 15.0f, 15.5f);
        cube3Transform.scale = glm::vec3(5.0f);
        cube3Transform.rotation = glm::vec3(0);

        Entity cube3 = ecsInterface.CreateEntity();
        ecsInterface.AddComponent<HierarchyComponent>(cube3, HierarchyComponent{});
        ecsInterface.AddComponent<TransformComponent>(cube3, cube3Transform);
        MeshProperties cube3mesh{};
        MeshLoader::loadMesh(cube3mesh.path, cube3mesh);
        cube3mesh.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
        ecsInterface.AddComponent<MeshProperties>(cube3, cube3mesh);
        ecsInterface.AddComponent<MaterialProperties>(cube3, materialComponent);

        TransformComponent cube4Transform;
        cube4Transform.position = glm::vec3(40.0f, 10.0f, -5.5f);
        cube4Transform.scale = glm::vec3(5.0f);
        cube4Transform.rotation = glm::vec3(0);

        Entity cube4 = ecsInterface.CreateEntity();
        ecsInterface.AddComponent<HierarchyComponent>(cube4, HierarchyComponent{});
        ecsInterface.AddComponent<TransformComponent>(cube4, cube4Transform);
        MeshProperties cube4mesh{};
        MeshLoader::loadMesh(cube4mesh.path, cube4mesh);
        cube4mesh.debugShape = static_cast<DebugShapeType>(selectedShapeIndex);
        ecsInterface.AddComponent<MeshProperties>(cube4, cube4mesh);
        ecsInterface.AddComponent<MaterialProperties>(cube4, materialComponent);

        // Shared
        BoxShapeSettings cubeShapeSettings(Vec3(2.5f, 2.5f, 2.5f));
        ShapeRefC cubeShape = cubeShapeSettings.Create().Get();

        // Linked
        BodyCreationSettings body1Settings( cubeShape, RVec3(cube1Transform.position.x, cube1Transform.position.y, cube1Transform.position.z), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
        body1Settings.mGravityFactor = 0.0f;
        BodyID body1ID = bodyInterface->CreateAndAddBody(body1Settings, EActivation::Activate);
        m_createdBodies.push_back(body1ID);
        bodyInterface->SetLinearVelocity(body1ID, Vec3(5.0f, 0.0f, 0.0f));

        BodyCreationSettings body2Settings(cubeShape, RVec3(cube2Transform.position.x, cube2Transform.position.y, cube2Transform.position.z), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
        body2Settings.mGravityFactor = 0.0f;
        BodyID body2ID = bodyInterface->CreateAndAddBody(body2Settings, EActivation::Activate);
        m_createdBodies.push_back(body2ID);
        bodyInterface->SetLinearVelocity(body2ID, Vec3(-5.0f, 0.0f, 0.0f));

        BodyCreationSettings body3Settings(cubeShape, RVec3(cube3Transform.position.x, cube3Transform.position.y, cube3Transform.position.z), Quat::sIdentity(), EMotionType::Dynamic, Layers::MOVING);
        BodyID body3ID = bodyInterface->CreateAndAddBody(body3Settings, EActivation::Activate);
        m_createdBodies.push_back(body3ID);
        bodyInterface->SetLinearVelocity(body3ID, Vec3(0.0f, 0.0f, 0.0f));

        BodyCreationSettings body4Settings(cubeShape, RVec3(cube4Transform.position.x, cube4Transform.position.y, cube4Transform.position.z), Quat::sIdentity(), EMotionType::Kinematic, Layers::MOVING);
        BodyID body4ID = bodyInterface->CreateAndAddBody(body4Settings, EActivation::Activate);
        m_createdBodies.push_back(body4ID);
        bodyInterface->SetLinearVelocity(body4ID, Vec3(0.0f, 0.0f, 0.0f));

        ecsInterface.AddComponent<PhysicsBodyComponent>(cube1, PhysicsBodyComponent{ body1ID, cubeShape });
        ecsInterface.AddComponent<PhysicsBodyComponent>(cube2, PhysicsBodyComponent{ body2ID, cubeShape });
        ecsInterface.AddComponent<PhysicsBodyComponent>(cube3, PhysicsBodyComponent{ body3ID, cubeShape });
        ecsInterface.AddComponent<PhysicsBodyComponent>(cube4, PhysicsBodyComponent{ body4ID, cubeShape });

        std::cout << "[Physics] Linked ECS cubes and Jolt bodies.\n";
    }

    void PhysicsSystem::Update(float dt) {
        physicsSystem.Update(dt, 1, tempAllocator, jobSystem);

        for (Entity entity : ecsInterface.GetEntities()) {
            if (ecsInterface.HasComponent<TransformComponent>(entity) && ecsInterface.HasComponent<PhysicsBodyComponent>(entity)) {
                auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
                auto& phys = ecsInterface.GetComponent<PhysicsBodyComponent>(entity);

                const JPH::RVec3 pos = bodyInterface->GetPosition(phys.ID);
                transform.position = glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ());
            }
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

        delete jobSystem;
        delete tempAllocator;

        JPH::UnregisterTypes();

        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}
