///////////////////////////////////////////////////////////////////////////////
///
/// @file EnemyBehavior.h
/// 
/// @brief This file contains the implementation of various behavior functions
///        for entities in the physics system, including movement and collision
///        handling. It defines behaviors such as chasing the player and oscillating
///        up/down or left/right. The functions use the ECS Coordinator to manage
///        component access and entity interactions.
///  
/// @authors Joshua Sim
/// @copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include <iostream>
#include "Coordinator.h"
#include "EngineState.h"
#include "LogicManager.h"
#include "ComponentList.h"
#include <vector>
#include "glew.h"
#include <cmath>
#include <vector>
#include "EnemyManager.h"

#include "TransformProperties.h"
using namespace Framework;
extern Framework::Coordinator ecsInterface;

void ChasePlayerBehavior(Framework::Entity entity, float deltaTime) {
    // Retrieve the enemy's components
    MovementComponent& entityMovement = ecsInterface.GetComponent<MovementComponent>(entity);
    CollisionComponent& entityCollision = ecsInterface.GetComponent<CollisionComponent>(entity);
    TransformComponent& entityTransform = ecsInterface.GetComponent<TransformComponent>(entity);

    // Loop over all entities tagged as "Player"
    for (auto const& otherEntityId : ecsInterface.GetEntitiesByTag("Player")) {
        if (entity == otherEntityId) continue;  // Skip self-collision check

        CollisionComponent& playerCollision = ecsInterface.GetComponent<CollisionComponent>(otherEntityId);
        TransformComponent& playerTransform = ecsInterface.GetComponent<TransformComponent>(otherEntityId);

        // Check if the entities have the correct ObjectTypes (Enemy and Player)
        if (entityCollision.type == ObjectType::Enemy && playerCollision.type == ObjectType::Player) {
            // Calculate the direction vector from the enemy to the player
            float directionX = playerTransform.position.x - entityTransform.position.x;
            float directionY = playerTransform.position.y - entityTransform.position.y;

            // Calculate the magnitude (distance) of the direction vector
            float magnitude = std::sqrt(directionX * directionX + directionY * directionY);

            // Avoid division by zero
            if (magnitude == 0.0f) continue;

            // Normalize the direction vector
            float normalizedX = directionX / magnitude;
            float normalizedY = directionY / magnitude;

            // Set velocity directly based on normalized direction and a base speed
            float baseSpeed = 100.0f;  // Adjust as needed
            entityMovement.velocity.x = normalizedX * baseSpeed;
            entityMovement.velocity.y = normalizedY * baseSpeed;

            // Update position based on velocity
            entityTransform.position.x += entityMovement.velocity.x * deltaTime;
            entityTransform.position.y += entityMovement.velocity.y * deltaTime;

            // Check collision status to decide on destruction
            if (entityCollision.collided) {
                entitiesToDestroy.push_back(entity);  // Mark entity for deletion
                return;  // Exit the function after deletion
            }
        }
    }
}

// Function to make the entity move up and down within a smaller interval
void MoveUpDownBehavior(Framework::Entity entity, float deltaTime) {
    (void)deltaTime;
    // Retrieve the movement component of the entity
    MovementComponent& movement = ecsInterface.GetComponent<MovementComponent>(entity);
    float amplitude = 150.f; // Adjust this value to control the range of oscillation
    float frequency = 2.0;  // Frequency of oscillation

    // Set the y velocity using a sine wave for smooth up and down movement
    movement.velocity.y = std::sin(frequency * glfwGetTime()) * amplitude;
    // Set x velocity to zero to only allow movement in the y direction
    movement.velocity.x = 0.0f;
}

// Function to make the entity move left and right within a smaller interval
void MoveLeftRightBehavior(Framework::Entity entity, float deltaTime) {
    (void)deltaTime;
    // Retrieve the movement component of the entity
    MovementComponent& movement = ecsInterface.GetComponent<MovementComponent>(entity);
    float amplitude = 150.f; // Adjust this value to control the range of oscillation
    float frequency = 2.0f;  // Frequency of oscillation

    // Set the x velocity using a cosine wave for smooth left and right movement
    movement.velocity.x = std::cos(frequency * glfwGetTime()) * amplitude;
    // Set y velocity to zero to only allow movement in the x direction
    movement.velocity.y = 0.0f;
}


void RegisterEnemyBehaviors() {
    GlobalLogicManager.RegisterFunction("ChasePlayer", ChasePlayerBehavior);
    GlobalLogicManager.RegisterFunction("MoveUpDown", MoveUpDownBehavior);
    GlobalLogicManager.RegisterFunction("MoveLeftRight", MoveLeftRightBehavior);
}

