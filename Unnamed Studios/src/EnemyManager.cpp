///////////////////////////////////////////////////////////////////////////////
///
///@file EnemyManager.cpp
/// 
/// @brief An enemy system which controls the enemy movement
///        
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "EnemyManager.h"
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 
#include "LogicManager.h"

extern Framework::Coordinator ecsInterface;
std::vector<Framework::Entity> entitiesToDestroy;

namespace Framework {




    // Constructor
    EnemyManager::EnemyManager()
    {

    }

    // Destructor
    EnemyManager::~EnemyManager() {

    }

    // Initialize the system
    void EnemyManager::Initialize() {

        ecsInterface.RegisterComponent<EnemyComponent>();
        Signature signature;
        signature.set(ecsInterface.GetComponentType<EnemyComponent>());
        ecsInterface.SetSystemSignature<EnemyManager>(signature);
        std::cout << "Signature for Enemy system is: " << signature << std::endl;



    }
    // Update the system
    void EnemyManager::Update(float deltaTime) {

        for (auto const& entityId : mEntities) {
            // Retrieve the EnemyComponent for this entity
            CollisionComponent& Collision = ecsInterface.GetComponent<CollisionComponent>(entityId);

            if (Collision.type == ObjectType::Player) {
                if (!ecsInterface.HasTag(entityId, "Player"))
                    ecsInterface.AddTag(entityId, "Player");
            }
        } // assign tag inefficiently

        for (auto const& entityId : mEntities) {
            // Retrieve the EnemyComponent for this entity
            EnemyComponent& enemyComponent = ecsInterface.GetComponent<EnemyComponent>(entityId);

            // Check if there’s a behavior function assigned
            if (enemyComponent.behavior) {
                // Call the assigned behavior function, passing in the entity ID and delta time
                enemyComponent.behavior(entityId, deltaTime);
            }
            else {
                // Optional: Log an error or fallback behavior if no function is assigned
                std::cerr << "Error: No behavior assigned to entity " << entityId << std::endl;
            }
        }


        // Destroy all entities marked for deletion after the main loop
        for (auto const& id : entitiesToDestroy) {
            ecsInterface.DestroyEntity(id);
        }
    }

    // Get the name of the system
    std::string EnemyManager::GetName() {
        return "Enemy Manager";
    }

    //void EnemyManager::moveEnenmyToPlayer(float deltaTime) {
    //    (void)deltaTime;
    //    //std::vector<Entity> entitiesToDestroy;  // Vector to store entities marked for destruction

    //    for (auto const& entityId : mEntities) {
    //        MovementComponent& entity1Movement = ecsInterface.GetComponent<MovementComponent>(entityId);
    //        CollisionComponent& entity1Collision = ecsInterface.GetComponent<CollisionComponent>(entityId);
    //        TransformComponent& entity1Transform = ecsInterface.GetComponent<TransformComponent>(entityId);

    //        for (auto const& otherEntityId : mEntities) {
    //            if (entityId == otherEntityId) continue; // Skip self-collision check
    //            CollisionComponent& entity2Collision = ecsInterface.GetComponent<CollisionComponent>(otherEntityId);
    //            TransformComponent& entity2Transform = ecsInterface.GetComponent<TransformComponent>(otherEntityId);

    //            if (entity1Collision.type == ObjectType::Enemy && entity2Collision.type == ObjectType::Player) {
    //                static float baseSpeedX = entity1Movement.velocity.x;
    //                static float baseSpeedY = entity1Movement.velocity.y;

    //                // Calculate the direction vector from the enemy to the player
    //                float directionX = entity2Transform.position.x - entity1Transform.position.x;
    //                float directionY = entity2Transform.position.y - entity1Transform.position.y;

    //                // Calculate the magnitude (distance) of the direction vector
    //                float magnitude = std::sqrt(directionX * directionX + directionY * directionY);

    //                // Avoid division by zero
    //                if (magnitude == 0.0f) continue;

    //                // Normalize the direction vector
    //                float normalizedX = directionX / magnitude;
    //                float normalizedY = directionY / magnitude;

    //                // Set the velocity based on the normalized direction and a consistent speed
    //                if (!entity1Collision.collided) {
    //                    entity1Movement.velocity.x = normalizedX * baseSpeedX;
    //                    entity1Movement.velocity.y = normalizedY * baseSpeedY;
    //                }
    //                else {
    //                    entitiesToDestroy.push_back(entityId);  // Mark entity for deletion
    //                }
    //            }
    //        }
    //    }

    //    // Destroy all entities marked for deletion after the main loop
    //    for (auto const& id : entitiesToDestroy) {
    //        ecsInterface.DestroyEntity(id);
    //    }
    //}

}