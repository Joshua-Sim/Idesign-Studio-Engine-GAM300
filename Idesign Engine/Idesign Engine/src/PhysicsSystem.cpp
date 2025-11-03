///////////////////////////////////////////////////////////////////////////////
///
///@file PhysicsSystem.cpp
/// 
/// @brief A physics system which move the entities based on velocity and delta time.  
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include "PhysicsSystem.h"
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <iostream>

using namespace JPH;

namespace Framework {

    namespace Layers {
        static constexpr ObjectLayer NON_MOVING = 0;
        static constexpr ObjectLayer MOVING = 1;
    }

    // Basic filters
    class MyBroadPhaseLayerInterface final : public BroadPhaseLayerInterface {
    public:
        uint GetNumBroadPhaseLayers() const override { return 2; }
        BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer layer) const override {
            return (layer == Layers::NON_MOVING) ? BroadPhaseLayer(0) : BroadPhaseLayer(1);
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(BroadPhaseLayer layer) const override {
            return layer.GetValue() == 0 ? "NON_MOVING" : "MOVING";
        }
#endif
    };
    class MyObjectVsBroadPhaseLayerFilter : public ObjectVsBroadPhaseLayerFilter {
    public:
        bool ShouldCollide(ObjectLayer, BroadPhaseLayer) const override { return true; }
    };
    class MyObjectLayerPairFilter : public ObjectLayerPairFilter {
    public:
        bool ShouldCollide(ObjectLayer, ObjectLayer) const override { return true; }
    };

    void PhysicsSystem::Init()
    {
        // =============================
        // Jolt core setup
        // =============================
        RegisterDefaultAllocator();
        Factory::sInstance = new Factory();
        RegisterTypes();

        const uint cMaxBodies = 1024;
        const uint cNumBodyMutexes = 0;
        const uint cMaxBodyPairs = 1024;
        const uint cMaxContactConstraints = 1024;

        m_Allocator = new TempAllocatorImpl(10 * 1024 * 1024);

        uint ThreadCount = std::thread::hardware_concurrency();
        if (ThreadCount == 0) ThreadCount = 4;

        const uint cMaxPhysicsJobs = 1024;
        const uint cMaxPhysicsBarriers = 8;

        m_JobSystem = new JPH::JobSystemThreadPool(cMaxPhysicsJobs, cMaxPhysicsBarriers, ThreadCount);

        // =============================
        // Broadphase / collision filters
        // =============================
        static MyBroadPhaseLayerInterface broadPhase;
        static MyObjectVsBroadPhaseLayerFilter broadPhaseFilter;
        static MyObjectLayerPairFilter pairFilter;

        m_System.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints,
            broadPhase, broadPhaseFilter, pairFilter);
        m_System.SetGravity(Vec3(0, -9.81f, 0));

        // =============================
        // Create cube (for raycast demo)
        // =============================
        BodyInterface& bi = m_System.GetBodyInterface();

        BoxShapeSettings cubeShape(Vec3(1.0f, 1.0f, 1.0f)); // 2x2x2 cube total
        ShapeRefC shape = cubeShape.Create().Get();

        BodyCreationSettings settings(
            shape,
            RVec3(0, 2, 0),            // position
            Quat::sIdentity(),         // rotation
            EMotionType::Static,       // not moving for now
            Layers::NON_MOVING         // our layer
        );

        Body* cube = bi.CreateBody(settings);
        bi.AddBody(cube->GetID(), EActivation::DontActivate);
        m_CubeID = cube->GetID();

        // Initialize face colors (all red)
        for (auto& c : m_FaceColors)
            c = Vec3(1, 0, 0);

        std::cout << "[Physics] Jolt initialized successfully with cube demo.\n";
    }


    void PhysicsSystem::Update(float dt) {
        m_System.Update(dt, 1, m_Allocator, m_JobSystem);
    }

    bool Framework::PhysicsSystem::CastRay(const JPH::RVec3& origin, const JPH::Vec3& direction, float maxDistance, JPH::RayCastResult& outResult)
    {
        using namespace JPH;

        // Create a ray definition (origin + direction * distance)
        RRayCast ray(origin, direction * maxDistance);
        RayCastResult result;

        // Default filters (all optional in your version)
        if (m_System.GetNarrowPhaseQuery().CastRay(ray, result))
        {
            outResult = result;
            return true;
        }


        return false; // No hit
    }

    void PhysicsSystem::CubeInteraction(const RVec3& camPos, const Vec3& rayDir, bool click, bool hold, float deltaX, float deltaY)
    {
        RRayCast ray(camPos, rayDir * 100.0f);
        RayCastResult hit;

        if (m_System.GetNarrowPhaseQuery().CastRay(ray, hit))
        {
            if (hit.mBodyID == m_CubeID)
            {
                // ---- CLICK EVENT ----
                if (click)
                {
                    std::cout << "Cube clicked! Face ID: " << hit.mSubShapeID2.GetValue() << std::endl;

                    // Change color to green for clicked face
                    int face = hit.mSubShapeID2.GetValue() % 6;
                    m_FaceColors[face] = Vec3(0, 1, 0);
                    m_Selected = true;
                }

                // ---- ROTATE WHILE HOLD ----
                if (hold && m_Selected)
                {
                    BodyInterface& bi = m_System.GetBodyInterface();
                    Quat rotation = Quat::sRotation(Vec3::sAxisY(), deltaX * 0.01f) *
                        Quat::sRotation(Vec3::sAxisX(), deltaY * 0.01f);
                    bi.SetRotation(m_CubeID, rotation * bi.GetRotation(m_CubeID), JPH::EActivation::DontActivate);
                }
            }
        }
        else
        {
            m_Selected = false;
        }
    }


    void PhysicsSystem::Shutdown() {
        delete m_Allocator;
        delete m_JobSystem;
        delete Factory::sInstance;
    }

}
