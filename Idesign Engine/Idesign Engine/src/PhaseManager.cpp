#include "pch.h"
#include "PhaseManager.h"
#include "EventManager.h"
#include "PhysicsSystem.h" 
#include "ComponentList.h"
#include <iostream>
#include <Jolt/Jolt.h>
#include <glm/glm/gtc/quaternion.hpp>
#include <Jolt/Physics/Body/BodyInterface.h>

extern Framework::Coordinator ecsInterface;

namespace Framework {

    void PhaseManager::Initialize()
    {
        std::cout << "[PhaseManager] Initialized. Default Phase: DAY" << std::endl;
    }

    void PhaseManager::TogglePhase()
    {
        if (m_currentPhase == GamePhase::Day)
            SetPhase(GamePhase::Night);
        else
            SetPhase(GamePhase::Day);
    }

    void PhaseManager::SetPhase(GamePhase newPhase)
    {
        // 1. State Change
        m_currentPhase = newPhase;

        // 2. Determine Target Transform
        glm::vec3 targetPos = (m_currentPhase == GamePhase::Day) ? m_daySpawnPos : m_nightSpawnPos;
        glm::vec3 targetRot = (m_currentPhase == GamePhase::Day) ? m_daySpawnRot : m_nightSpawnRot;

        std::string phaseName = (m_currentPhase == GamePhase::Day) ? "DAY" : "NIGHT";
        std::cout << "[PhaseManager] Transitioning to " << phaseName << " phase." << std::endl;

        // 3. Teleport Player
        TeleportPlayer(targetPos, targetRot);

        // 4. Fire Events
        if (m_currentPhase == GamePhase::Day) {
            EventManager::Instance().Fire("OnDayStart", nullptr);
        }
        else {
            EventManager::Instance().Fire("OnNightStart", nullptr);
        }
    }

    void PhaseManager::TeleportPlayer(const glm::vec3& position, const glm::vec3& rotation)
    {
        // 1. Find Player Entity
        Entity player = ecsInterface.GetEntityByName("Player");

        if (!ecsInterface.IsEntityValid(player)) {
            std::cerr << "[PhaseManager] Error: Player entity not found!" << std::endl;
            return;
        }

        // 2. Update Visual Transform
        if (ecsInterface.HasComponent<TransformComponent>(player))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(player);
            transform.position = position;
            transform.rotation = rotation;
        }

        // 3. Update Physics Bodies (CRITICAL FOR JOLT)
        if (ecsInterface.HasComponent<PhysicsBodyComponent>(player))
        {
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(player);
            auto physicsSystem = ecsInterface.GetSystem<PhysicsSystem>();

            if (physicsSystem)
            {
                JPH::BodyInterface* bodyInterface = physicsSystem->GetBodyInterface();
                if (bodyInterface)
                {
                    // --- CONVERT DATA ONCE ---
                    // Convert GLM Position to Jolt
                    JPH::RVec3 jPos(position.x, position.y, position.z);

                    // Convert Euler (Degrees) to GLM Quat -> Jolt Quat
                    glm::quat q = glm::quat(glm::radians(rotation));
                    JPH::Quat jRot(q.x, q.y, q.z, q.w);

                    // --- APPLY TO ALL BODIES ---
                    // The player might have a Solid body AND a Sensor body. 
                    // We must move both.
                    for (JPH::BodyID id : pb.IDs)
                    {
                        if (!id.IsInvalid())
                        {
                            // Force set position and wake up
                            bodyInterface->SetPositionAndRotation(id, jPos, jRot, JPH::EActivation::Activate);

                            // Kill velocity so they don't carry momentum into the new phase
                            bodyInterface->SetLinearVelocity(id, JPH::Vec3::sZero());
                        }
                    }
                }
            }
        }
    }
}