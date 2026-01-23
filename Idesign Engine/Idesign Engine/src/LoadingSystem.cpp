// --- LoadingSystem.cpp ---
#include "pch.h"
#include "LoadingSystem.h"
#include "EventManager.h"
#include "TextureLoader.h"
#include "ComponentList.h"
#include "PhaseManager.h"

extern Framework::Coordinator ecsInterface;

namespace Framework {
    bool LoadingSystem::OnLoadingStart(void* data) {
        m_IsActive = true;
        m_AnimationTimer = 0.0f;
        m_CurrentFrame = 0;

        m_LoadingEntity = ecsInterface.GetEntityByName("LoadingUI");

        if (m_LoadingEntity == -1 || !ecsInterface.IsEntityValid(m_LoadingEntity)) {
            m_LoadingEntity = ecsInterface.CreateEntity();
            ecsInterface.SetEntityName(m_LoadingEntity, "LoadingUI");

            ecsInterface.AddComponent(m_LoadingEntity, TransformComponent{
                glm::vec3(960.0f, 540.0f, 0.0f),
                glm::vec3(0.0f),
                glm::vec3(1920.0f, 1080.0f, 1.0f)
                });

            TextureProperties tex;
            tex.isUI = true;
            tex.path = m_Frames[0];
            TextureLoader::loadTexture(tex, true);
            ecsInterface.AddComponent(m_LoadingEntity, tex);

            LayerComponent layer;
            layer.layerID = Layer::UI;
            layer.sortID = 999;
            ecsInterface.AddComponent(m_LoadingEntity, layer);
        }
        else
        {
            auto& trans = ecsInterface.GetComponent<TransformComponent>(m_LoadingEntity);
            trans.scale = glm::vec3(1920.0f, 1080.0f, 1.0f);

            auto& tex = ecsInterface.GetComponent<TextureProperties>(m_LoadingEntity);
            tex.path = m_Frames[m_CurrentFrame];
            TextureLoader::loadTexture(tex, true);
        }
        return true; // satisfies C4716 error
    }

    void LoadingSystem::Update(float deltaTime) {
        if (!m_IsActive || !ecsInterface.IsEntityValid(m_LoadingEntity)) return;

        m_AnimationTimer += deltaTime;

        if (m_AnimationTimer >= 0.1f) {
            m_AnimationTimer = 0.0f;
            m_CurrentFrame++;

            if (m_CurrentFrame >= m_Frames.size()) {
                OnLoadingEnd(nullptr);
                return;
            }

            if (ecsInterface.HasComponent<TextureProperties>(m_LoadingEntity)) {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(m_LoadingEntity);
                tex.path = m_Frames[m_CurrentFrame];
                TextureLoader::loadTexture(tex, true);
            }
        }
    }

    bool LoadingSystem::OnLoadingEnd(void* data) {
        m_IsActive = false;

        // NEW: If the transition was to the Upgrade phase, pause the game now
        if (phaseManager && phaseManager->GetCurrentPhase() == GamePhase::Upgrade) {
            phaseManager->ApplyUpgradeState();
        }

        if (ecsInterface.IsEntityValid(m_LoadingEntity)) {
            auto& trans = ecsInterface.GetComponent<TransformComponent>(m_LoadingEntity);
            trans.scale = glm::vec3(0.0f);
        }

        return true;
    }

    void LoadingSystem::Initialize() {
        EventManager::Instance().Subscribe("StartPhaseTransition", [this](void* d) { return OnLoadingStart(d); });
        EventManager::Instance().Subscribe("EndPhaseTransition", [this](void* d) { return OnLoadingEnd(d); });
    }
}