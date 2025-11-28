///////////////////////////////////////////////////////////////////////////////
///
/// @file NavMeshSystem.h
/// @brief System to handle building and managing navigation mesh
/// @authors: Joshua Sim
/// @copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pch.h"
#include "System.h"
#include "ComponentList.h"

// Forward declare Recast/Detour types from global namespace
struct rcContext;
struct rcHeightfield;
struct rcCompactHeightfield;
struct rcContourSet;
struct rcPolyMesh;
struct rcPolyMeshDetail;
struct dtNavMesh;
struct dtNavMeshQuery;

namespace Framework {
    class NavMeshSystem : public ISystem {
    public:
        NavMeshSystem();
        ~NavMeshSystem();

        static NavMeshSystem& Instance()
        {
            static NavMeshSystem instance;
            return instance;
        }

        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override;

        // API to build navmesh from current scene mesh data
        void BuildNavMesh();
        void SetAgentDestination(Entity entity, const glm::vec3& destination);

        // In NavMeshSystem.h (public section):
        void PrintNavMeshInfo() const;

        // Optional: debug draw toggle or internal states
        void DebugDraw() const;

        //Navmesh Functions
        bool FindPath(const glm::vec3& start, const glm::vec3& end,
            std::vector<glm::vec3>& outPath);

        bool IsPointOnNavmesh(const glm::vec3& point, float searchRadius = 2.0f);

        glm::vec3 GetNearestPointOnNavmesh(const glm::vec3& point);


    private:
        //clean up
        void CleanupNavData();
        // Internal helper
        void CollectSourceGeometry();
        void ConvertToRecastFormat(); // Convert collected mesh to recast input mesh format

        // Recast/Detour navmesh data holders (use :: to reference global namespace)
        ::rcContext* recastContext = nullptr;
        ::rcHeightfield* heightfield = nullptr;
        ::rcCompactHeightfield* compactHeightfield = nullptr;
        ::rcContourSet* contourSet = nullptr;
        ::rcPolyMesh* polyMesh = nullptr;
        ::rcPolyMeshDetail* polyMeshDetail = nullptr;
        ::dtNavMesh* navMesh = nullptr;
        ::dtNavMeshQuery* navQuery = nullptr;

        // Temporarily store collected geometry before passing to Recast
        std::vector<glm::vec3> collectedPositions;
        std::vector<int> collectedIndices;

        bool isNavMeshBuilt = false;
    };
}