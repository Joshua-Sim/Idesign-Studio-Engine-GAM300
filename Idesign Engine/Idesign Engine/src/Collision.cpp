///////////////////////////////////////////////////////////////////////////////
///
///@file Collision.cpp
/// 
/// @brief A collision system which compares two objects and checks for collision, using AABB/ circle collision.
///        
///  
///@Authors: Dylan Lau
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "Collision.h"
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 



float dt = 0.0;

extern Framework::Coordinator ecsInterface;



namespace Framework {




    // Constructor
    Collision::Collision()
    {

    }

    // Destructor
    Collision::~Collision() {

    }

    // Initialize the system
    void Collision::Initialize() {


        ecsInterface.RegisterComponent<CollisionComponent>();
        Signature signature;
        signature.set(ecsInterface.GetComponentType<CollisionComponent>());
        ecsInterface.SetSystemSignature<Collision>(signature);
        std::cout << "Signature for Collision system is: " << signature << std::endl;

       

    }
    // Update the system
    void Collision::Update(float deltaTime) {
        dt = deltaTime;

        for (auto const& entityId : mEntities) {
            MovementComponent& entity1Movement = ecsInterface.GetComponent<MovementComponent>(entityId);
            CollisionComponent& entity1Collision = ecsInterface.GetComponent<CollisionComponent>(entityId);
            TransformComponent& entity1Transform = ecsInterface.GetComponent<TransformComponent>(entityId);
            // Collision Detection Loop - check `entityObject` against all other entities
            for (auto const& otherEntityId : mEntities) {
                if (entityId == otherEntityId) continue; // Skip self-collision check

                MovementComponent& entity2Movement = ecsInterface.GetComponent<MovementComponent>(otherEntityId);
                CollisionComponent& entity2Collision = ecsInterface.GetComponent<CollisionComponent>(otherEntityId);
                TransformComponent& entity2Transform = ecsInterface.GetComponent<TransformComponent>(otherEntityId);

                // Check collision between `entityObject` and `otherObject` only if they are both collidable
                if (!(entity1Collision.type == Enemy) || !(entity2Collision.type == Enemy)) {


                    float collideTime = UE_AabbCollision(entity1Collision.aabb, entity1Movement.velocity, entity2Collision.aabb, entity2Movement.velocity);
                    if (collideTime != -1 && collideTime <= deltaTime) {
                        // A collision is predicted to occur within the time step
                        if (!entity1Collision.collided || !entity2Collision.collided) {
                            std::cout << "Collision detected between entity " << entityId << " and entity " << otherEntityId << std::endl;
                        }

                        stopObject(entity1Movement, entity1Transform, entity1Collision, collideTime);
                        stopObject(entity2Movement, entity2Transform, entity2Collision, collideTime);
                    }
                }
            }
        }
    }

    // Get the name of the system
    std::string Collision::GetName() {
        return "testing";
    }

    float Collision::min(float a, float b) {
        return (a < b) ? a : b;
    }

    float Collision::max(float a, float b) {
        return (a > b) ? a : b;
    }
  
    float Collision::UE_AabbCollision(AABB aabb1,  glm::vec2 vel1, AABB aabb2,  glm::vec2 vel2) {
        float dfirst = 0.0;
        float dlast = dt;
        float dfirstX = 0.0;
        float dlastX = dt;
        float dfirstY = 0.0;
        float dlastY = dt;
        bool xCollided = false;
        //bool yCollided = false;
        glm::vec2 relativeVel;
        relativeVel.x = vel2.x - vel1.x;
        relativeVel.y = vel2.y - vel1.y;
        if ((aabb1.max.x < aabb2.min.x || aabb1.min.x > aabb2.max.x) || (aabb1.max.y > aabb2.min.y || aabb1.min.y < aabb2.max.y)) {

            if (relativeVel.x < 0) {
                // Moving left
                if (aabb1.min.x > aabb2.max.x) {
                    return -1; // No collision
                }
                if (aabb1.max.x < aabb2.min.x) {
                    dfirstX = max((aabb1.min.x - aabb2.max.x) / relativeVel.x, dfirstX);
                }
                if (aabb1.min.x < aabb2.max.x) {
                    dlastX = min((aabb1.min.x - aabb2.max.x) / relativeVel.x, dlastX); // Added parentheses
                }
            }
            else if (relativeVel.x > 0) {

                // Moving right
                if (aabb1.min.x > aabb2.max.x) {
                    dfirstX = max((aabb1.min.x - aabb2.max.x) / relativeVel.x, dfirstX);
                }
                if (aabb1.max.x > aabb2.min.x) {
                    dlastX = min((aabb1.max.x - aabb2.min.x) / relativeVel.x, dlastX); // Added parentheses
                }
                if (aabb1.max.x < aabb2.min.x) {
                    return -1; // No collision
                }
            }
            else if (relativeVel.x == 0) {
                if (aabb1.max.x < aabb2.min.x) {
                    return -1;
                }
                else if (aabb1.min.x > aabb2.max.x) {
                    return -1;
                }
                else {
                    xCollided = true;
                }
            }
            if (dfirstX > dlastX) {
                return -1;
            }
            xCollided = true;
            //std::cout << relativeVel.y << std::endl;
            if (relativeVel.y < 0) {
                //box is moving from bottom to top
                if (aabb1.max.y > aabb2.min.y) {
                    return -1; // No collision
                }
                if (aabb1.max.y < aabb2.min.y) {

                    dfirstY = max((aabb1.min.y - aabb2.max.y) / relativeVel.y, dfirstY);

                }
                if (aabb1.min.y < aabb2.max.y) {
                    dlastY = min((aabb1.min.y - aabb2.max.y) / relativeVel.y, dlastY); // Added parentheses
                }
            }
            else if (relativeVel.y > 0) {
                // box is moving from top to bottom

                if (aabb1.min.y > aabb2.max.y) {
                    dfirstY = max((aabb1.min.y - aabb2.max.y) / relativeVel.y, dfirstY);
                }
                if (aabb1.max.y > aabb2.min.y) {
                    dlastY = min((aabb1.max.y - aabb2.min.y) / relativeVel.y, dlastY); // Added parentheses
                }
                if (aabb1.max.y < aabb2.min.y) {
                    return -1; // No collision
                }

            }

            else if (relativeVel.y == 0) {

                if (aabb1.max.y > aabb2.min.y) {
                    return -1;
                }
                else if (aabb1.min.y < aabb2.max.y) {
                    return -1;
                }
                else {
                    if (xCollided) {
                        dfirst = max(dfirstX, dfirstY);
                        dlast = min(dlastX, dlastY);
                        return dfirst;
                    }
                }

            }

            if (dfirstY > dlastY) {
                return -1;
            }

            if (xCollided) {
                dfirst = max(dfirstX, dfirstY);
                dlast = min(dlastX, dlastY);
                return dfirst;
            }


        }

        return dfirst;

    }

    bool Collision::UE_CollisionCircle(float radius1, glm::vec2 position1, float radius2, glm::vec2 position2) {
        // Calculate the difference in each dimension
        float dx = position1.x - position2.x;
        float dy = position1.y - position2.y;

        // Calculate the distance using the Pythagorean theorem
        float totalDistance = sqrt(dx * dx + dy * dy);
        totalDistance = totalDistance - radius1 -radius2;

        if (totalDistance <= 0) {
            return true;
        }
        return false;

    }

    void Collision::stopObject(MovementComponent& movement, TransformComponent& transform, CollisionComponent& collide, float collideTime) {
        // Move both objects up to just before the collision point
        transform.position.x += movement.velocity.x * collideTime;
        transform.position.y += movement.velocity.y * collideTime;
        collide.aabb.max.x += movement.velocity.x * collideTime;
        collide.aabb.max.y += movement.velocity.y * collideTime;
        collide.aabb.min.x += movement.velocity.x * collideTime;
        collide.aabb.min.y += movement.velocity.y * collideTime;
        movement.velocity.x = 0;
        movement.velocity.y = 0;

        // Mark both objects as collided
        collide.collided = true;
    }

}