#include "pch.h"
#include "PhaseManager.h"
#include "EventManager.h"
#include "PhysicsSystem.h" 
#include "ComponentList.h"
#include <iostream>
#include <Jolt/Jolt.h>
#include <glm/glm/gtc/quaternion.hpp>
#include <Jolt/Physics/Body/BodyInterface.h>
#include "Graphics.h"
#include "EngineState.h"
#include "InputHandler.h" 

extern Framework::Coordinator ecsInterface;

namespace Framework {
    Camera* cam;
    void PhaseManager::Initialize()
    {
        std::cout << "[PhaseManager] Initialized. Default Phase: DAY" << std::endl;
    }

    void PhaseManager::TogglePhase()
    {
        switch (m_currentPhase)
        {
        case GamePhase::Day:
            SetPhase(GamePhase::Night);
            break;
        case GamePhase::Night:
            SetPhase(GamePhase::Upgrade);
            break;
        case GamePhase::Upgrade:
            SetPhase(GamePhase::Day);
            break;
        }
    }

    void PhaseManager::SetPhase(GamePhase newPhase)
    {
        // 1. State Change
        m_currentPhase = newPhase;
        glm::vec3 targetPos, targetRot;
        // ... existing transform logic ...
        if (m_currentPhase == GamePhase::Day) {
            targetPos = m_daySpawnPos;
            targetRot = m_daySpawnRot;
        }
        else if (m_currentPhase == GamePhase::Night) {
            targetPos = m_nightSpawnPos;
            targetRot = m_nightSpawnRot;
        }

        // ... existing teleport logic ...
        TeleportPlayer(targetPos, targetRot);

        // =========================================================
        // [FIX] FORCE CURSOR STATE BASED ON PHASE
        // =========================================================
        InputHandler* input = InputHandler::GetInstance();

        if (m_currentPhase == GamePhase::Day || m_currentPhase == GamePhase::Night)
        {
            // 1. Disable Engine UI States
            engineState.SetUpgrade(false);
            engineState.SetMenu(false);
            engineState.SetPaused(false);

            // 2. Force InputHandler to Game Mode (Cursor Disabled)
            if (input) {
                input->SetUIFocused(false);
            }
        }
        else if (m_currentPhase == GamePhase::Upgrade)
        {
            // Upgrade mode usually needs a cursor
            engineState.SetUpgrade(true);

            // Optional: Force cursor ON for upgrade menu
            if (input) {
                input->SetUIFocused(true);
            }
        }

        // =========================================================
        // EXISTING EVENT LOGIC
        // =========================================================

        if (m_currentPhase == GamePhase::Day) {
            auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
            Skybox* skybox = graphicsSystem->getSkybox();
            if (graphicsSystem)
            {
                skybox->setBlendFactor(0.0f);
                Camera* cam = graphicsSystem->getCamera();
                if (cam)
                {
                    cam->yaw = -180.0f;
                    cam->pitch = 0.0f;
                    cam->updateVectors();
                }
            }
            EventManager::Instance().Fire("OnDayStart", nullptr);
        }
        else if (m_currentPhase == GamePhase::Night) {
            auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
            Skybox* skybox = graphicsSystem->getSkybox();
            if (graphicsSystem)
            {
                skybox->setBlendFactor(1.0f);
                Camera* cam = graphicsSystem->getCamera();
                if (cam)
                {
                    cam->yaw = 0.0f;
                    cam->pitch = 0.0f;
                    cam->updateVectors();
                }
            }
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