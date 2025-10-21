#pragma once
///////////////////////////////////////////////////////////////////////////////
///
/// @file SceneManager.h
/// 
/// @brief Declaration of the SceneManager system. Responsible for managing
///        scene lifecycle (loading, saving, switching, unloading) and 
///        tracking entities that belong to each scene.
///  
/// @authors Joshua
/// @copyright 2025
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "System.h"
#include "ComponentList.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Framework {

    class SceneManager : public ISystem {
    public:
        SceneManager();
        ~SceneManager();

        // Core ISystem functions
        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override;

        // Scene control functions 
        void LoadScene(const std::string& path, bool additive = false);     // Load a scene from JSON
        void SaveScene(const std::string& path);     // Save current scene to JSON
        void UnloadScene(const std::string& name);   // Unload entities belonging to a scene (Replace with guid in input afterwards or load via properties? inside func maybe)
    
        // Scene access
        std::string GetCurrentSceneName() const;

        void RegisterEntityToScene(Entity entity, const std::string& sceneName);

        const std::unordered_map<std::string, std::vector<Entity>>& GetSceneEntities() const {
            return m_SceneEntities;
        }

        std::unordered_map<std::string, std::vector<Entity>>& GetSceneEntities() { return m_SceneEntities; }

    private:
        std::string m_CurrentSceneName;
        
        //Mapping of scene names to entities belonging to it
        std::unordered_map<std::string, std::vector<Entity>> m_SceneEntities; // sceneName -> entities

        // Deferred operations for removing entity safely at end of systems loop.
        std::vector<Entity> m_PendingEntityDeletes;
        std::vector<std::string> m_PendingSceneUnloads;

        // Load pending scenes for refresh at end of system loop
        struct PendingLoad {
            std::string fullPath;
            std::string sceneName;
            bool additive;
        };


        std::vector<PendingLoad> m_PendingSceneLoads;
    };

    // Declare the single global SceneManager
    extern SceneManager sceneManager;
}
