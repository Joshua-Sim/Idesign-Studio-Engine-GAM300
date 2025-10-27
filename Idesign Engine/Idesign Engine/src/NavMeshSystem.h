///////////////////////////////////////////////////////////////////////////////
///
/// @file NavMeshSystem.h
/// @brief System to handle building and managing navigation mesh
/// @authors: Joshua Sim
/// @copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "System.h"
#include "ComponentList.h"

namespace Framework {

    class NavMeshSystem : public ISystem {
    public:
        NavMeshSystem();
        ~NavMeshSystem();

        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override;

        // API to build navmesh from current scene mesh data
        void BuildNavMesh();

        // Optional: debug draw toggle or internal states
        void DebugDraw() const;

    private:
        // Internal helper
        void CollectSourceGeometry();
        void ConvertToRecastFormat(); // Convert collected mesh to recast input mesh format

        // Recast/Detour navmesh data holders (later filled)
        struct rcContext* recastContext = nullptr;
        struct rcHeightfield* heightfield = nullptr;
        struct rcCompactHeightfield* compactHeightfield = nullptr;
        struct rcContourSet* contourSet = nullptr;
        struct rcPolyMesh* polyMesh = nullptr;
        struct rcPolyMeshDetail* polyMeshDetail = nullptr;
        struct dtNavMesh* navMesh = nullptr;
        struct dtNavMeshQuery* navQuery = nullptr;

        // Temporarily store collected geometry before passing to Recast
        std::vector<glm::vec3> collectedPositions;
        std::vector<int> collectedIndices;

        bool isNavMeshBuilt = false;
    };
}
