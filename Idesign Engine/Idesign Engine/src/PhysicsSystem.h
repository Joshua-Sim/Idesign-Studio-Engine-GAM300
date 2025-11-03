#pragma once
///////////////////////////////////////////////////////////////////////////////
///
///@file PhysicsSystem.h
/// 
/// @brief The creation of the physics class and the declaration of its functions and members
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <array>

namespace Framework {

    class PhysicsSystem {
    public:
        void Init();
        void Update(float dt);
        void Shutdown();

        bool CastRay(const JPH::RVec3& origin, const JPH::Vec3& direction, float maxDistance, JPH::RayCastResult& outResult);
        void CubeInteraction(const JPH::RVec3& camPos, const JPH::Vec3& rayDir, bool click, bool hold, float deltaX, float deltaY);

        JPH::PhysicsSystem& Get() { return m_System; }

    private:
        JPH::PhysicsSystem m_System;
        JPH::TempAllocatorImpl* m_Allocator = nullptr;
        JPH::JobSystem* m_JobSystem = nullptr; // base pointer, holds JobSystemThreadPool

        JPH::BodyID m_CubeID;
        std::array<JPH::Vec3, 6> m_FaceDots;     // Positions of dots
        std::array<JPH::Vec3, 6> m_FaceColors;   // RGB color per face
        bool m_Selected = false;
    };

}
