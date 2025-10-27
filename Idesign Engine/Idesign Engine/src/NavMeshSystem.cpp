///////////////////////////////////////////////////////////////////////////////
///
///@file NavmeshSystem.cpp
/// 
/// @brief A collision system which compares two objects and checks for collision, using AABB/ circle collision.
///        
///  
///@Authors: Joshua Sim 
///Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "Collision.h"
#include <iostream>
#include "Coordinator.h"
#include "InputHandler.h"
#include "FontSystem.h" 

#include "NavMeshSystem.h"
#include "ComponentList.h"
#include "NavMeshProperties.h"
#include "AssetManager.h" // If needed to fetch mesh from GUID

#include "Recast/Recast.h"
#include "Recast/DetourNavMesh.h"
#include "Recast/DetourNavMeshBuilder.h"
#include "Recast/DetourNavMeshQuery.h"

extern Framework::Coordinator ecsInterface;


namespace Framework {

    NavMeshSystem::NavMeshSystem() {
        // Initialize any memory or Recast context
    }

    NavMeshSystem::~NavMeshSystem() {
        // Clean up Recast/Detour memory
    }

    void NavMeshSystem::Initialize() {
        // Load settings or allocate navmesh objects
        //Initialise and register a navmesh component

        ecsInterface.RegisterComponent<NavMeshComponent>();

        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>()); // Register transform so i have access to all entities, then retrieve via checks agent and navmesh comp
        //loop for agent

        ecsInterface.SetSystemSignature<NavMeshSystem>(signature);
        std::cout << "Signature for Navmesh system is: " << signature << std::endl;
    }

    void NavMeshSystem::Update(float deltaTime) {
        // For now, might do nothing. Agents will go here in the future.
    }

    std::string NavMeshSystem::GetName() {
        return "NavMeshSystem";
    }

    void NavMeshSystem::BuildNavMesh() {
        CollectSourceGeometry();
        ConvertToRecastFormat();
        // Proceed with standard Recast navmesh build pipeline
        // Store result in navMesh
        isNavMeshBuilt = true;
    }

    void NavMeshSystem::CollectSourceGeometry() {
       collectedPositions.clear();
        collectedIndices.clear();

        int indexOffset = 0;

        for (auto entity : mEntities) {
            auto& navComp = ecsInterface.GetComponent<NavMeshComponent>(entity);

            if (!navComp.isNavMeshSource) continue;

            // Fetch mesh asset from GUID
            MeshProperties* mesh = AssetManager::Instance().Get<MeshProperties>(navComp.sourceMeshGUID);
            if (!mesh) {
                std::cerr << "[NavMeshSystem] Mesh not found for entity: " << entity << std::endl;
                continue;
            }

            // Append mesh.positions and adjusted indices
            for (const auto& pos : mesh->positions)
                collectedPositions.push_back(pos);  // Optionally transform pos if needed

            for (const auto& idx : mesh->indices)
                collectedIndices.push_back(idx + indexOffset);  // Offset for combining multiple meshes

            indexOffset += static_cast<int>(mesh->positions.size());
        }

        std::cout << "[NavMeshSystem] Collected geometry: "
            << collectedPositions.size() << " verts, "
            << collectedIndices.size() / 3 << " triangles.\n";
    }


    void NavMeshSystem::ConvertToRecastFormat() {
        if (collectedPositions.empty() || collectedIndices.empty()) return;

        std::vector<float> vertsFlat;
        vertsFlat.reserve(collectedPositions.size() * 3);
        for (const auto& v : collectedPositions) {
            vertsFlat.push_back(v.x);
            vertsFlat.push_back(v.y);
            vertsFlat.push_back(v.z);
        }

        // Store pointer-based versions if needed for Recast
        ::rcContext ctx;
        rcConfig config{};
        // Set default config values (cell size, height, agentRadius, etc.)
        config.cs = 0.3f;
        config.ch = 0.2f;
        config.walkableSlopeAngle = 45.0f;

        // TODO: Allocate and build heightfield etc.

        std::cout << "[NavMeshSystem] Converted "
            << collectedPositions.size() << " vertices and "
            << collectedIndices.size() / 3 << " triangles to Recast input.\n";
    }


    void NavMeshSystem::DebugDraw() const {
        if (!isNavMeshBuilt) return;
        // Optional: use ImGui or debug renderer to show navmesh polygons
    }
}
