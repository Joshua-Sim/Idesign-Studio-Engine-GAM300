#include "pch.h"
#include "LoadingSystem.h"
#include "EventManager.h"
#include "TextureLoader.h"
#include "ComponentList.h"

extern Framework::Coordinator ecsInterface;

namespace Framework {
    bool LoadingSystem::OnLoadingStart(void* data) {
        m_IsActive = true;
        m_AnimationTimer = 0.0f;
        m_CurrentFrame = 0;

        // Use GetEntityByName to see if it exists first
        m_LoadingEntity = ecsInterface.GetEntityByName("LoadingUI");

        if (m_LoadingEntity == -1 || !ecsInterface.IsEntityValid(m_LoadingEntity)) {
            m_LoadingEntity = ecsInterface.CreateEntity();
            ecsInterface.SetEntityName(m_LoadingEntity, "LoadingUI");

            // Use AddComponent safely
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
        else {
            // Only access if entity is valid and has the component
            if (ecsInterface.HasComponent<TransformComponent>(m_LoadingEntity)) {
                auto& trans = ecsInterface.GetComponent<TransformComponent>(m_LoadingEntity);
                trans.scale = glm::vec3(1920.0f, 1080.0f, 1.0f);
            }
        }
        return true;
    }

    void LoadingSystem::Update(float deltaTime) {
        if (!m_IsActive || !ecsInterface.IsEntityValid(m_LoadingEntity)) return;

        m_AnimationTimer += deltaTime;

        // Frame duration: 0.5 seconds per frame
        if (m_AnimationTimer >= 0.5f) {
            m_AnimationTimer = 0.0f;

            // Move to the next frame
            m_CurrentFrame++;

            // CHECK: Have we finished all 3 frames?
            if (m_CurrentFrame >= m_Frames.size()) {
                // ONE LOOP COMPLETE: Stop and hide everything
                //std::cout << "[LoadingSystem] One loop finished. Deactivating..." << std::endl;
                OnLoadingEnd(nullptr);
                return;
            }

            // Otherwise, update the texture to the next frame
            if (ecsInterface.HasComponent<TextureProperties>(m_LoadingEntity)) {
                auto& tex = ecsInterface.GetComponent<TextureProperties>(m_LoadingEntity);
                tex.path = m_Frames[m_CurrentFrame];
                TextureLoader::loadTexture(tex, true);
            }
        }
    }

    bool LoadingSystem::OnLoadingEnd(void* data) {
        m_IsActive = false;
        // Hide by scaling to zero
        if (ecsInterface.IsEntityValid(m_LoadingEntity)) {
            auto& trans = ecsInterface.GetComponent<TransformComponent>(m_LoadingEntity);
            trans.scale = glm::vec3(0.0f);
        }

        return true;
    }

    void LoadingSystem::Initialize() {
        // Now you can pass the functions more directly if the signatures match
        EventManager::Instance().Subscribe("StartPhaseTransition", [this](void* d) { return OnLoadingStart(d); });
        EventManager::Instance().Subscribe("EndPhaseTransition", [this](void* d) { return OnLoadingEnd(d); });
    }
}