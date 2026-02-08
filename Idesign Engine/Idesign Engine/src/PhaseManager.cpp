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
        EventManager::Instance().Subscribe("EndPhaseTransition", [this](void* d) -> bool {
            this->HandleEndPhaseTransition(d);
            return true; // or false, depending on your event system's needs
            });
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

    void PhaseManager::HandleEndPhaseTransition(void* data)
    {
        auto dialogueSys = ecsInterface.GetSystem<Framework::DialogueSystem>();
        if (!dialogueSys) return;

        bool tutorialToPlay = false;

        // --- NIGHT PHASE TUTORIAL ---
        if (m_currentPhase == GamePhase::Day && !m_dayTutorialTriggered)
        {

            dialogueSys->AddLine("Welcome to Runes and Remedies! This is the Day phase where you collect resources by pressing E", "Aunt Mira (frog)", "pear");
            dialogueSys->AddLine("Click on How To Play from the menu by pressing ESC to find out more!", "Aunt Mira (frog)", "portrait");
            dialogueSys->AddLine("When you're done with gathering or your inventory is full, interact with the gate to go to the night phase", "Aunt Mira (frog)", "portrait");

            dialogueSys->StartDialogue();
            m_dayTutorialTriggered = true;
            tutorialToPlay = true;
        }

        // --- NIGHT PHASE TUTORIAL ---
        if (m_currentPhase == GamePhase::Night && !m_nightTutorialTriggered)
        {

            dialogueSys->AddLine("Look alive, child! The moon is high; 'tis the Night Phase now. Don't just stand there catching flies!", "Aunt Mira (frog)", "pear");
            dialogueSys->AddLine("Staring at the pot won't tell you what to brew. Press 'R' to consult the Recipe Book, or have you forgotten your letters too?", "Aunt Mira (frog)", "portrait");
            dialogueSys->AddLine("Clumsy hands! If you toss the wrong root into my cauldron, you'd best fish it back out before you ruin the whole batch!", "Aunt Mira (frog)", "portrait");

            dialogueSys->StartDialogue();
            m_nightTutorialTriggered = true;
            tutorialToPlay = true;
        }

        if (tutorialToPlay)
        {
            // This prevents the system from "clearing" itself when nothing is happening
            engineState.ClearToBase();
            engineState.PushState(EngineMode::Play);
            engineState.PushState(EngineMode::Dialogue); // Ensure Dialogue mode is active

            dialogueSys->StartDialogue();
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
                glm::vec3 playerRot = ecsInterface.GetComponent<TransformComponent>(ecsInterface.GetEntityByName("Player")).rotation;
            }

            EventManager::Instance().Fire("OnDayStart", nullptr);
            targetPos = m_daySpawnPos;
            targetRot = m_daySpawnRot;
            shouldTeleport = true;
            //dialogue system
            
        }
        else if (m_currentPhase == GamePhase::Night) {
            auto graphicsSystem = ecsInterface.GetSystem<Graphics>();
            if (graphicsSystem) {
                Skybox* skybox = graphicsSystem->getSkybox();
                if (skybox) skybox->setBlendFactor(1.0f);
                glm::vec3 playerRot = ecsInterface.GetComponent<TransformComponent>(ecsInterface.GetEntityByName("Player")).rotation;
            }
            targetPos = m_nightSpawnPos;
            targetRot = m_nightSpawnRot;
            TeleportPlayer(m_nightSpawnPos, m_nightSpawnRot);
            EventManager::Instance().Fire("OnNightStart", nullptr);
        }

        // =========================================================
        // INTEGRITY: Day/Night Engine State (Unpauses immediately)
        // =========================================================
        InputHandler* input = InputHandler::GetInstance();

        if (m_currentPhase == GamePhase::Day || m_currentPhase == GamePhase::Night)
        {
            if (input) input->SetUIFocused(false); // Hides cursor
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

        // 1. Force ECS Transform update immediately
        if (ecsInterface.HasComponent<TransformComponent>(player))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(player);
            transform.position = position;
            transform.rotation = rotation;
        }

        // 2. Physics Body Update
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

                    // FIX: GLM quat constructor is (w, x, y, z) while Jolt is (x, y, z, w)
                    // Using euler angles to quat conversion to be safe
                    glm::quat q = glm::quat(glm::radians(rotation));
                    JPH::Quat jRot(q.x, q.y, q.z, q.w);

                    for (JPH::BodyID id : pb.IDs)
                    {
                        if (!id.IsInvalid())
                        {
                            // FIX: Activate the body and set position
                            bodyInterface->SetPositionAndRotation(id, jPos, jRot, JPH::EActivation::Activate);

                            // FIX: Completely kill all momentum (Linear and Angular)
                            // This prevents the player from "sliding" or "spinning" upon landing
                            bodyInterface->SetLinearVelocity(id, JPH::Vec3::sZero());
                            bodyInterface->SetAngularVelocity(id, JPH::Vec3::sZero());

                            // FIX: Reset motion properties if the body is kinematic or has a controller
                            bodyInterface->ResetSleepTimer(id);
                        }
                    }
                }
            }
        }
    }
}