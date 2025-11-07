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
        //JPH::EMotionType mPrevMotionType = JPH::EMotionType::Static;

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
}
