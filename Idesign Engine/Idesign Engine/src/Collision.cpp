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

    bool Collision::CollisionIntersection_RectRect(const AABB& aabb1,
        float vel1X, float vel1Y,
        const AABB& aabb2,
        float vel2X, float vel2Y,
        float& firstTimeOfCollision) {

        // Check if there is no overlap between the AABBs in any dimension
        if (aabb1.max.x <= aabb2.min.x || aabb2.max.x <= aabb1.min.x || aabb1.max.y <= aabb2.min.y || aabb2.max.y <= aabb1.min.y) {
            return false;
        }

        // Initialize time of first and last collision
        float tFirst = 0.0f;
        float tLast = dt; // g_dt represents the time step

        //// Calculate relative velocity between the two objects
        float VrelX = vel2X - vel1X;
        float VrelY = vel2Y = vel1Y;

        // Calculate collision times for X-axis
        if (VrelX != 0) {
            float tEnterX = (VrelX > 0) ? ((aabb2.min.x - aabb1.max.x) / VrelX) : ((aabb2.max.x - aabb1.min.x) / VrelX);
            float tExitX = (VrelX > 0) ? ((aabb2.max.x - aabb1.min.x) / VrelX) : ((aabb2.min.x - aabb1.max.x) / VrelX);
            // Swap tEnterX and tExitX if necessary to ensure tEnterX <= tExitX
            if (tEnterX > tExitX) {
                float temp = tEnterX;
                tEnterX = tExitX;
                tExitX = temp;
            }

            // Update tFirst and tLast based on collision times
            tFirst = (tEnterX > tFirst) ? tEnterX : tFirst;
            tLast = (tExitX < tLast) ? tExitX : tLast;
        }

        // Calculate collision times for Y-axis
        if (VrelY != 0) {
            float tEnterY = (VrelY > 0) ? ((aabb2.min.y - aabb1.max.y) / VrelY) : ((aabb2.max.y - aabb1.min.y) / VrelY);
            float tExitY = (VrelY > 0) ? ((aabb2.max.y - aabb1.min.y) / VrelY) : ((aabb2.min.y - aabb1.max.y) / VrelY);
            // Swap tEnterY and tExitY if necessary to ensure tEnterY <= tExitY
            if (tEnterY > tExitY) {
                float temp = tEnterY;
                tEnterY = tExitY;
                tExitY = temp;
            }
            // Update tFirst and tLast based on collision times
            tFirst = (tEnterY > tFirst) ? tEnterY : tFirst;
            tLast = (tExitY < tLast) ? tExitY : tLast;
        }

        //Check if there is a collision within the movement interval
        if (tFirst > tLast) {
            return false; // No collision within movement interval
        }

        // Update the time of first collision
        firstTimeOfCollision = tFirst;
        return true; // Collision detected
    }

    bool Collision::CollisionIntersection_CircleCircle(const Circle& circle1,
        float vel1X, float vel1Y,
        const Circle& circle2,
        float vel2X, float vel2Y,
        float& firstTimeOfCollision) {
        // Calculate relative velocity
        float VrelX = vel2X - vel1X;
        float VrelY = vel2Y - vel1Y;

        // Calculate initial distance between centers
        float dx = circle2.m_center.x - circle1.m_center.x;
        float dy = circle2.m_center.y - circle1.m_center.y;
        float distSquared = dx * dx + dy * dy;

        // Calculate sum of radii
        float combinedRadius = circle1.m_radius + circle2.m_radius;
        float combinedRadiusSquared = combinedRadius * combinedRadius;

        // Check if circles are currently overlapping
        if (distSquared <= combinedRadiusSquared) {
            firstTimeOfCollision = 0.0f;
            return true; // Circles are already colliding
        }

        // Calculate relative velocity squared
        float VrelSquared = VrelX * VrelX + VrelY * VrelY;

        // Early exit if relative velocity is zero (parallel motion)
        if (VrelSquared == 0) {
            return false;
        }

        // Calculate the dot product of the relative velocity and the vector between centers
        float dotProduct = dx * VrelX + dy * VrelY;

        // Check if circles are moving towards each other
        if (dotProduct > 0) {
            return false; // Circles are moving apart
        }

        // Calculate the discriminant of the quadratic equation
        float discriminant = dotProduct * dotProduct - VrelSquared * (distSquared - combinedRadiusSquared);

        // If discriminant is negative, no collision occurs
        if (discriminant < 0) {
            return false;
        }

        // Calculate the time of collision
        float sqrtDiscriminant = sqrt(discriminant);
        float tEnter = (-dotProduct - sqrtDiscriminant) / VrelSquared;

        // Ensure collision happens within the time step g_dt
        if (tEnter >= 0.0f && tEnter <= dt) {
            firstTimeOfCollision = tEnter;
            return true; // Collision detected within the time step
        }

        return false; // No collision within the time step
    }


    bool Collision::CollisionIntersection_AabbAabb3D(const AABB3& A, const glm::vec3& vA,
        const AABB3& B, const glm::vec3& vB,
        float dt, float& outTHit,
        glm::vec3* outNormal) const
    {
        const glm::vec3 rv = vB - vA;

        // Overlap at t=0?
        const bool overlapX0 = !(A.max.x <= B.min.x || A.min.x >= B.max.x);
        const bool overlapY0 = !(A.max.y <= B.min.y || A.min.y >= B.max.y);
        const bool overlapZ0 = !(A.max.z <= B.min.z || A.min.z >= B.max.z);
        if (overlapX0 && overlapY0 && overlapZ0) {
            outTHit = 0.0f;
            if (outNormal) *outNormal = glm::vec3(0.0f); // unknown normal at initial overlap
            return true;
        }

        float tEnterX, tExitX, tEnterY, tExitY, tEnterZ, tExitZ;

        auto slabTimes = [](float Amin, float Amax, float Bmin, float Bmax, float rvComp,
            float& tEnter, float& tExit, bool overlap0) -> bool
            {
                if (rvComp > 0.0f) {
                    tEnter = (Bmin - Amax) / rvComp;
                    tExit = (Bmax - Amin) / rvComp;
                }
                else if (rvComp < 0.0f) {
                    tEnter = (Bmax - Amin) / rvComp;
                    tExit = (Bmin - Amax) / rvComp;
                }
                else {
                    if (!overlap0) return false; // never meet along this axis
                    tEnter = -std::numeric_limits<float>::infinity();
                    tExit = std::numeric_limits<float>::infinity();
                }
                return true;
            };

        if (!slabTimes(A.min.x, A.max.x, B.min.x, B.max.x, rv.x, tEnterX, tExitX, overlapX0)) return false;
        if (!slabTimes(A.min.y, A.max.y, B.min.y, B.max.y, rv.y, tEnterY, tExitY, overlapY0)) return false;
        if (!slabTimes(A.min.z, A.max.z, B.min.z, B.max.z, rv.z, tEnterZ, tExitZ, overlapZ0)) return false;

        const float tEnter = std::max(tEnterX, std::max(tEnterY, tEnterZ));
        const float tExit = std::min(tExitX, std::min(tExitY, tExitZ));

        if (tEnter > tExit)  return false; // slabs don't overlap in time
        if (tExit < 0.0f)   return false; // happened in the past
        if (tEnter > dt)     return false; // after this frame

        outTHit = std::max(0.0f, tEnter);

        if (outNormal) {
            // Normal corresponds to the axis with maximal entry time
            if (tEnter == tEnterX) {
                (*outNormal) = glm::vec3((rv.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f);
            }
            else if (tEnter == tEnterY) {
                (*outNormal) = glm::vec3(0.0f, (rv.y >= 0.0f) ? 1.0f : -1.0f, 0.0f);
            }
            else {
                (*outNormal) = glm::vec3(0.0f, 0.0f, (rv.z >= 0.0f) ? 1.0f : -1.0f);
            }
        }
        return true;
    }

    //bool Collision::IntersectOBB3D(const OBB3& A, const OBB3& B,
    //    float* outPenetration,
    //    glm::vec3* outNormal) const
    //{
    //    constexpr float EPS = 1e-6f;

    //    // GLM mat3 is column-major: orientation[c] is axis c
    //    const glm::vec3 Au[3] = { A.orientation[0], A.orientation[1], A.orientation[2] };
    //    const glm::vec3 Bu[3] = { B.orientation[0], B.orientation[1], B.orientation[2] };

    //    // Rotation matrix R[i][j] = dot(Au[i], Bu[j]) and its abs with epsilon
    //    float R[3][3], absR[3][3];
    //    for (int i = 0; i < 3; ++i)
    //        for (int j = 0; j < 3; ++j) {
    //            R[i][j] = glm::dot(Au[i], Bu[j]);
    //            absR[i][j] = std::fabs(R[i][j]) + EPS;
    //        }

    //    // Translation B - A in world, then express in A's frame
    //    const glm::vec3 tWorld = B.center - A.center;
    //    const glm::vec3 tA = { glm::dot(tWorld, Au[0]),
    //                           glm::dot(tWorld, Au[1]),
    //                           glm::dot(tWorld, Au[2]) };

    //    float minPen = std::numeric_limits<float>::infinity();
    //    glm::vec3 bestN(0.0f);

    //    auto considerAxis = [&](float overlap, const glm::vec3& axisWorld, float signedDir) {
    //        if (overlap < minPen) {
    //            minPen = overlap;
    //            bestN = axisWorld * signedDir; // ensure normal points A -> B
    //        }
    //        };

    //    // --- 1) Axes of A: Au[i]
    //    for (int i = 0; i < 3; ++i) {
    //        const float ra = A.half[i];
    //        const float rb = B.half.x * absR[i][0] + B.half.y * absR[i][1] + B.half.z * absR[i][2];
    //        const float dist = std::fabs(tA[i]);
    //        const float ovl = (ra + rb) - dist;
    //        if (ovl < 0.0f) return false;
    //        const float sgn = (tA[i] >= 0.0f) ? 1.0f : -1.0f;
    //        considerAxis(ovl, Au[i], sgn);
    //    }

    //    // --- 2) Axes of B: Bu[j]
    //    for (int j = 0; j < 3; ++j) {
    //        const float ra = A.half.x * absR[0][j] + A.half.y * absR[1][j] + A.half.z * absR[2][j];
    //        const float rb = B.half[j];
    //        const float proj = tA.x * R[0][j] + tA.y * R[1][j] + tA.z * R[2][j];
    //        const float dist = std::fabs(proj);
    //        const float ovl = (ra + rb) - dist;
    //        if (ovl < 0.0f) return false;
    //        const float sgn = (proj >= 0.0f) ? 1.0f : -1.0f;
    //        considerAxis(ovl, Bu[j], sgn);
    //    }

    //    // --- 3) 9 cross-product axes: Au[i] x Bu[j]
    //    for (int i = 0; i < 3; ++i) {
    //        for (int j = 0; j < 3; ++j) {
    //            glm::vec3 L = glm::cross(Au[i], Bu[j]);
    //            const float len2 = glm::dot(L, L);
    //            if (len2 < EPS) continue; // nearly parallel axes skip

    //            const glm::vec3 Ln = L / std::sqrt(len2);

    //            // Projected radii
    //            const float ra =
    //                A.half.x * std::fabs(glm::dot(Au[0], Ln)) +
    //                A.half.y * std::fabs(glm::dot(Au[1], Ln)) +
    //                A.half.z * std::fabs(glm::dot(Au[2], Ln));

    //            const float rb =
    //                B.half.x * std::fabs(glm::dot(Bu[0], Ln)) +
    //                B.half.y * std::fabs(glm::dot(Bu[1], Ln)) +
    //                B.half.z * std::fabs(glm::dot(Bu[2], Ln));

    //            const float dist = std::fabs(glm::dot(tWorld, Ln));
    //            const float ovl = (ra + rb) - dist;
    //            if (ovl < 0.0f) return false;

    //            const float sgn = (glm::dot(tWorld, Ln) >= 0.0f) ? 1.0f : -1.0f;
    //            considerAxis(ovl, Ln, sgn);
    //        }
    //    }

    //    if (outPenetration) *outPenetration = minPen;
    //    if (outNormal)      *outNormal = bestN;
    //    return true;
    //}
}