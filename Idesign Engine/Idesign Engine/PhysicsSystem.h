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
#pragma once //Makes sure this header is only included once

#include "pch.h"
#include "System.h"
#include "MainMath.h"
#include "InputHandler.h"
#include "ComponentList.h"



namespace Framework {

    class PhysicsSystem : public ISystem {
    public:
        PhysicsSystem();
        ~PhysicsSystem();

        void Initialize();
        void Update(float deltaTime) override;
        std::string GetName() override;



        /**
               * @brief Calculates the Maxpoint of the object for AABB Collision
               *
               * Thisfunction calculates the Max point of the object based on the position and size
               *
               * @param [size] the width and height of the object
               * @param [position] The x and y position of the object
          */

        glm::vec2 UE_AABBPositionMax(glm::vec2 size, glm::vec2 position);

        /**
             * @brief Calculates the Min point of the object for AABB Collision
             *
             * This function calculates the Min point of the object based on the position and size
             *
             * @param [sizeX] the width of the object
             * @param [sizeY] The height of the object
             * @param [positionX] The x position of the object
             * @param [positionY] The y position of the object
        */

        glm::vec2 UE_AABBPositionMin(float sizeX, float sizeY, float positionX, float positionY);

        /**
             * @brief Move the entities based on the delta time
             *
             * @param [size] the width and height of the object
             * @param [position] The x and y position of the object

        */

        void moveEntities(float deltaTime);

        bool InRange3D(const glm::vec3& from,
            const glm::vec3& to,
            float maxDistance) const;

        bool InSight3D(const glm::vec3& eyePos,
            const glm::vec3& forwardDir,
            const glm::vec3& targetPos,
            float maxDistance,
            float fovDegrees,
            const std::function<bool(const glm::vec3&, const glm::vec3&, float&)>& raycast = nullptr) const;

        InputHandler* InputHandlerInstance;



    private:



    };




}