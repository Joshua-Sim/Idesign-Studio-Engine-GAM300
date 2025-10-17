#pragma once
///////////////////////////////////////////////////////////////////////////////
///
///@file Collision.h
/// 
/// @brief The creation of the Collision class and the declaration of its functions and members
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once //Makes sure this header is only included once

#include "pch.h"
#include "System.h"
#include "MainMath.h"
#include "InputHandler.h"
#include "ComponentList.h"

namespace Framework {

    class Collision : public ISystem {
    public:
        Collision();
        ~Collision();

        void Initialize();
        void Update(float deltaTime) override;
        std::string GetName() override;


        /**
          * @brief Checks whether the circle is colliding
          *
          * This function checks whether two circle objects are colliding based on its radius
          *
          * @param radius1 : radius of the object 1
          * @param position1 : position of object 1
          * @param radius2 : radius of the object 2
          * @param position2 : position of object 2
      */

        bool UE_CollisionCircle(float radius1, glm::vec2 position1, float radius2, glm::vec2 position2);


        /**
            * @brief Checks whether the square is colliding
            *
            * This function checks whether two square shaped objects are colliding using AABB
            *
            * @param aabb1 : max and min of aabb
            * @param vel1 : vel of aabb1
            * @param aabb2 : max and min of aabb
            * @param vel2 : vel of aabb2
            *
        */
        float UE_AabbCollision(AABB aabb1, glm::vec2 vel1, AABB aabb2, glm::vec2 vel2);

        /**
            * @brief Stop Objects on collision
            *
            * This function checks stop the objects when it is collided
            *
            * @param movement : velcocity of the object
            * @param transform : transform of the object
            * @param collide : collide status of the object
            * @param collideTime : collide time of the object
            *
        */

        void stopObject(MovementComponent& movement, TransformComponent& transform, CollisionComponent& collide, float collideTime);

        //Collision between rectangle object
        bool CollisionIntersection_RectRect(const AABB& aabb1,
            float vel1X, float vel1Y,
            const AABB& aabb2,
            float vel2X, float vel2Y,
            float& firstTimeOfCollision);

        //Collision between circle object
        bool CollisionIntersection_CircleCircle(const Circle& circle1,
            float vel1X, float vel1Y,
            const Circle& circle2,
            float vel2X, float vel2Y,
            float& firstTimeOfCollision);

        /**
            * @brief Compare two numbers and return the smaller one
            *
            * This function returns the smaller number between two numbers
            *
            * @param a : first number
            * @param b : second number
            *
        */

        float min(float a, float b);


        /**
            * @brief Compare two numbers and return the bigger one
            *
            * This function returns the bigger number between two numbers
            *
            * @param a : first number
            * @param b : second number
            *
        */
        float max(float a, float b);

        /**
         * @brief Instantaneous overlap test for 3D AABB vs AABB (axis-aligned).
         * @param a First AABB in world space.
         * @param b Second AABB in world space.
         * @return true if volumes overlap at current positions (t=0).
         */
        bool CollisionIntersection_AabbAabb3D(const AABB3& A, const glm::vec3& vA,
            const AABB3& B, const glm::vec3& vB,
            float dt, float& outTHit,
            glm::vec3* outNormal) const;

        /**
         * @brief Instantaneous overlap test for 3D OBB vs OBB using SAT (15 axes).
         * @param a First OBB (world-space).
         * @param b Second OBB (world-space).
         * @param outPenetration Optional. Minimal penetration depth along separating normal.
         * @param outNormal Optional. World-space collision normal (points from a -> b).
         * @return true if volumes overlap at current positions (t=0).
         */
        bool IntersectOBB3D(const OBB3& a, const OBB3& b,
            float* outPenetration = nullptr,
            glm::vec3* outNormal = nullptr) const;

    private:


    };
}