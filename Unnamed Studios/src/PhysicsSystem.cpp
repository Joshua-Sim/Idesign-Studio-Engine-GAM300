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
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 




extern Framework::Coordinator ecsInterface;

namespace Framework {


    bool printed = false;
    bool printed1 = false;


    // Constructor
    PhysicsSystem::PhysicsSystem()
    {

    }

    // Destructor
    PhysicsSystem::~PhysicsSystem() {
        
    }

    // Initialize the system
    void PhysicsSystem::Initialize() {

        InputHandlerInstance = InputHandler::GetInstance();


        //ecsInterface.RegisterComponent<CollisionComponent>();
        Signature signature;
        ecsInterface.RegisterComponent<MovementComponent>();
        signature.set(ecsInterface.GetComponentType<MovementComponent>());
       // signature.set(ecsInterface.GetComponentType<CollisionComponent>());
        ecsInterface.SetSystemSignature<PhysicsSystem>(signature);
        std::cout << "Signature for Physics system is: " << signature << std::endl;



    }
    // Update the system
    void PhysicsSystem::Update(float deltaTime) {
        moveEntities(deltaTime);
    }

    // Get the name of the system
    std::string PhysicsSystem::GetName() {
        return "Collision";
    }


    glm::vec2 PhysicsSystem::UE_AABBPositionMax(glm::vec2 size, glm::vec2 position) {
        glm::vec2 aabbMax;
        aabbMax.x = position.x + (size.x / 2);
        aabbMax.y = position.y - (size.y / 2);
        return aabbMax;
    }

    glm::vec2 PhysicsSystem::UE_AABBPositionMin(float sizeX, float sizeY, float positionX, float positionY) {
        glm::vec2 aabbMin;
        aabbMin.x = positionX - (sizeX / 2);
        aabbMin.y = positionY + (sizeY / 2);
        return aabbMin;
    }

    void PhysicsSystem::moveEntities(float deltaTime) {
        
        for (auto const& entityId : mEntities) {
            UE_Vector::Vector2D customVectorSize;
            UE_Vector::Vector2D customVectorPosition;

            MovementComponent& movement = ecsInterface.GetComponent<MovementComponent>(entityId);
            CollisionComponent& collision = ecsInterface.GetComponent<CollisionComponent>(entityId);
            TransformComponent& transform = ecsInterface.GetComponent<TransformComponent>(entityId);
            customVectorSize.x = transform.scale.x;
            customVectorSize.y = transform.scale.y;
            customVectorPosition.x = transform.position.x;
            customVectorPosition.y = transform.position.y;
            // Check and apply movement for player objects based on input
            collision.aabb.max = UE_AABBPositionMax(transform.scale, transform.position);
            collision.aabb.min = UE_AABBPositionMin(customVectorSize.x, customVectorSize.y, customVectorPosition.x, customVectorPosition.y);
            if (collision.type == ObjectType::Player) {
               static float baseX = abs(movement.velocity.x);
               static float baseY = abs(movement.velocity.y);

               // Handle movement and update AABB bounds based on player input
               if (InputHandlerInstance->IsKeyHeld(GLFW_KEY_UP)) {
                  movement.velocity.y = baseY;
                   if (!(movement.velocity.y <= 0)) {
                      movement.velocity.y = -(movement.velocity.y);
                   }
               }
               else if (InputHandlerInstance->IsKeyHeld(GLFW_KEY_LEFT)) {
                   movement.velocity.x = baseX;
                   if (!(movement.velocity.x <= 0)) {
                   movement.velocity.x = -(movement.velocity.x);
                   }
               }
               else if (InputHandlerInstance->IsKeyHeld(GLFW_KEY_DOWN)) {
                   movement.velocity.y = baseY;
                   movement.velocity.y = abs(movement.velocity.y);
               }
               else if (InputHandlerInstance->IsKeyHeld(GLFW_KEY_RIGHT)) {
                   movement.velocity.x = baseX;
                   movement.velocity.x = abs(movement.velocity.x);
               }
               else {
                   movement.velocity.x = 0;
                   movement.velocity.y = 0;
               }

               transform.position.y += movement.velocity.y * deltaTime;
               collision.aabb.max.y += movement.velocity.y * deltaTime;
               collision.aabb.min.y += movement.velocity.y * deltaTime;
               transform.position.x += movement.velocity.x * deltaTime;
               collision.aabb.max.x += movement.velocity.x * deltaTime;
               collision.aabb.min.x += movement.velocity.x * deltaTime;
            }
            else {
                transform.position.y += movement.velocity.y * deltaTime;
                collision.aabb.max.y += movement.velocity.y * deltaTime;
                collision.aabb.min.y += movement.velocity.y * deltaTime;
                transform.position.x += movement.velocity.x * deltaTime;
                collision.aabb.max.x += movement.velocity.x * deltaTime;
                collision.aabb.min.x += movement.velocity.x * deltaTime;
            }
        }
    }



}