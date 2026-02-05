// --- PhaseManager.cpp ---
#include "pch.h"
#include "PhaseManager.h"
#include "EventManager.h"
#include "PhysicsSystem.h" 
#include "ComponentList.h"
#include <iostream>
#include <Jolt/Jolt.h>
#include <glm/glm/gtc/quaternion.hpp>
#include <Jolt/Physics/Body/BodyInterface.h>
#include "DialogueSystem.h"
#include "Graphics.h"
#include "EngineState.h"
#include "InputHandler.h" 

extern Framework::Coordinator ecsInterface;


namespace Framework {




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
        m_currentPhase = newPhase;

        glm::vec3 targetPos(0.0f);
        glm::vec3 targetRot(0.0f);
        bool shouldTeleport = false;

        // Don't auto-teleport when starting Day phase
        // Let the map UI handle it
        if (m_currentPhase == GamePhase::Day) {
            auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
            if (graphicsSystem) {
                Skybox* skybox = graphicsSystem->getSkybox();
                if (skybox) skybox->setBlendFactor(0.0f);

                Camera* cam = graphicsSystem->getCamera();
                if (cam) {
                    cam->yaw = -180.0f;
                    cam->pitch = 0.0f;
                    cam->updateVectors();
                }
            }

            EventManager::Instance().Fire("OnDayStart", nullptr);
            targetPos = m_daySpawnPos;
            targetRot = m_daySpawnRot;
            shouldTeleport = true;
            //dialogue system
            // TUTORIAL, PLAY ONCE
            if (!dayTutorial) {
                auto dialogueSys = ecsInterface.GetSystem<Framework::DialogueSystem>();
                if (dialogueSys) {
                    engineState.ClearToBase();
                    engineState.PushState(EngineMode::Play); // Allows physics and animation to run
                    dialogueSys->AddLine("Welcome to Runes and Remedies! This is the Day phase where you collect resources by pressing E", "Aunt Mira (frog)", "pear");
                    dialogueSys->AddLine("Click on How To Play from the menu by pressing ESC to find out more!", "Aunt Mira (frog)", "portrait");
                    dialogueSys->StartDialogue();
                }
                dayTutorial = true;
            }
            
        }
        else if (m_currentPhase == GamePhase::Night) {
            targetPos = m_nightSpawnPos;
            targetRot = m_nightSpawnRot;
            TeleportPlayer(m_nightSpawnPos, m_nightSpawnRot);

            if (!nightTutorial) {
                auto dialogueSys = ecsInterface.GetSystem<Framework::DialogueSystem>();
                if (dialogueSys) {
                    engineState.ClearToBase();
                    engineState.PushState(EngineMode::Play); // Allows physics and animation to run
                    dialogueSys->AddLine("This is the night phase!", "Aunt Mira (frog)", "pear");
                    dialogueSys->AddLine("Press R for the recipe book if you don't know the recipe", "Aunt Mira (frog)", "portrait");
                    dialogueSys->AddLine("If you ever misplace an item in the cauldron, you can take it back", "Aunt Mira (frog)", "portrait");
                    dialogueSys->StartDialogue();
                }
                nightTutorial = true;
            }
        }

        // =========================================================
        // INTEGRITY: Day/Night Engine State (Unpauses immediately)
        // =========================================================
        InputHandler* input = InputHandler::GetInstance();

        if (m_currentPhase == GamePhase::Day || m_currentPhase == GamePhase::Night)
        {
            if (input) input->SetUIFocused(false); // Hides cursor
        }

        // =========================================================
        // INTEGRITY: Camera & Graphics Logic
        // =========================================================
        auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
        if (graphicsSystem)
        {
            Skybox* skybox = graphicsSystem->getSkybox();
            Camera* cam = graphicsSystem->getCamera();

            if (m_currentPhase == GamePhase::Day) {
                if (skybox) skybox->setBlendFactor(0.0f);
                if (cam)
                {
                    cam->yaw = -180.0f; // Original Angle
                    cam->pitch = 0.0f;
                    cam->updateVectors();
                }
                EventManager::Instance().Fire("OnDayStart", nullptr);
            }
            else if (m_currentPhase == GamePhase::Night) {
                if (skybox) skybox->setBlendFactor(1.0f);
                if (cam)
                {
                    cam->yaw = 0.0f; // Original Angle
                    cam->pitch = 0.0f;
                    cam->updateVectors();
                }
                EventManager::Instance().Fire("OnNightStart", nullptr);
            }
        }
    }

    // NEW FUNCTION: Only called when the loading animation ends
    void PhaseManager::ApplyUpgradeState()
    {
        if (m_currentPhase == GamePhase::Upgrade)
        {
            InputHandler* input = InputHandler::GetInstance();
            engineState.ClearToBase();
            engineState.PushState(EngineMode::Play);
            engineState.PushState(EngineMode::Paused); // Pauses only now
            engineState.PushState(EngineMode::Upgrade);

            if (input) input->SetUIFocused(true); // Shows cursor
        }
    }

    void PhaseManager::TeleportPlayer(const glm::vec3& position, const glm::vec3& rotation)
    {
        Entity player = ecsInterface.GetEntityByName("Player");
        if (player == -1 || !ecsInterface.IsEntityValid(player)) return;

        if (ecsInterface.HasComponent<TransformComponent>(player))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(player);
            transform.position = position;
            transform.rotation = rotation;
        }

        if (ecsInterface.HasComponent<PhysicsBodyComponent>(player))
        {
            auto& pb = ecsInterface.GetComponent<PhysicsBodyComponent>(player);
            auto physicsSystem = ecsInterface.GetSystem<PhysicsSystem>();
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