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
        EventManager::Instance().Fire("StartPhaseTransition", nullptr);

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

        // [FIX 1] Initialize variables to prevent garbage data
        glm::vec3 targetPos(0.0f);
        glm::vec3 targetRot(0.0f);
        bool shouldTeleport = false; // Flag to control teleportation

        // 2. Determine Target Transform based on Phase
        if (m_currentPhase == GamePhase::Day) {
            targetPos = m_daySpawnPos;
            targetRot = m_daySpawnRot;
            shouldTeleport = true;
        }
        else if (m_currentPhase == GamePhase::Night) {
            targetPos = m_nightSpawnPos;
            targetRot = m_nightSpawnRot;
            shouldTeleport = true;
        }
        // [NOTE] We do NOT set shouldTeleport = true for Upgrade phase, 
        // preventing the garbage data crash.

        // 3. Teleport Player (Only if valid)
        if (shouldTeleport) {
            TeleportPlayer(targetPos, targetRot);
        }

        // =========================================================
        // FORCE CURSOR STATE
        // =========================================================
        InputHandler* input = InputHandler::GetInstance();

        if (m_currentPhase == GamePhase::Day || m_currentPhase == GamePhase::Night)
        {
            engineState.ClearToBase();
            engineState.PushState(EngineMode::Play);

            if (input) input->SetUIFocused(false);
        }
        else if (m_currentPhase == GamePhase::Upgrade)
        {
            engineState.ClearToBase();
            engineState.PushState(EngineMode::Play);
            engineState.PushState(EngineMode::Paused);
            engineState.PushState(EngineMode::Upgrade);

            if (input) input->SetUIFocused(true);
        }

        // =========================================================
        // EVENT & GRAPHICS LOGIC
        // =========================================================

        if (m_currentPhase == GamePhase::Day) {
            auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
            // [FIX 2] Check System BEFORE accessing Skybox
            if (graphicsSystem)
            {
                Skybox* skybox = graphicsSystem->getSkybox();
                if (skybox) skybox->setBlendFactor(0.0f); // [FIX 3] Check Skybox validity

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
            if (graphicsSystem)
            {
                Skybox* skybox = graphicsSystem->getSkybox();
                if (skybox) skybox->setBlendFactor(1.0f);

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
        Entity player = ecsInterface.GetEntityByName("Player");

        // 1. Validate Entity
        if (player == -1 || !ecsInterface.IsEntityValid(player)) {
            std::cerr << "[PhaseManager] ERROR: Player entity not found!" << std::endl;
            return;
        }

        // 2. Validate Transform
        if (ecsInterface.HasComponent<TransformComponent>(player))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(player);
            transform.position = position;
            transform.rotation = rotation;
        }

        // 3. Validate Physics
        if (ecsInterface.HasComponent<PhysicsBodyComponent>(player))
        {
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(player);
            auto physicsSystem = ecsInterface.GetSystem<PhysicsSystem>();

            // Ensure we actually have physics IDs to move
            if (physicsSystem && !pb.IDs.empty())
            {
                JPH::BodyInterface* bodyInterface = physicsSystem->GetBodyInterface();
                if (bodyInterface)
                {
                    JPH::RVec3 jPos(position.x, position.y, position.z);
                    glm::quat q = glm::quat(glm::radians(rotation));
                    JPH::Quat jRot(q.x, q.y, q.z, q.w);

                    for (JPH::BodyID id : pb.IDs)
                    {
                        if (!id.IsInvalid())
                        {
                            bodyInterface->SetPositionAndRotation(id, jPos, jRot, JPH::EActivation::Activate);
                            bodyInterface->SetLinearVelocity(id, JPH::Vec3::sZero());
                        }
                    }
                }
            }
        }
    }
}