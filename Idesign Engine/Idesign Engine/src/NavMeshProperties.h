///////////////////////////////////////////////////////////////////////////////
///
/// @file   : NavMeshComponent.h
/// @brief  : Runtime navmesh generation settings and transient runtime data
/// 
/// @authors: Joshua Sim 
/// @copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef _NAVMESH_COMPONENT_H_
#define _NAVMESH_COMPONENT_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <vector>
#include <string>
#include <glm.hpp>

namespace Framework
{
    struct NavMeshComponent
    {
        std::string sourceMeshGUID;  // Reference to MeshProperties
        bool isNavMeshSource = false;
        bool runtimeGenerated = false;

        // Recast config settings
        float agentRadius = 0.5f;
        float agentHeight = 2.0f;
        float cellSize = 0.3f;
        float cellHeight = 0.2f;
        float maxSlope = 45.0f;

        // Non-serialized runtime buffers (used for navmesh generation or pathfinding)
        std::vector<glm::vec3> positions;     // Reconstructed from MeshProperties on demand
        std::vector<int> indices;             // Triangle indices

        // Optional: store dtNavMesh*, dtNavMeshQuery* pointers separately if used
    };

    // Serialized properties (only these go into JSON)
    // Serialized properties (only these go into JSON)
    inline constexpr std::array g_NavMeshPropsArray =
    {
        PROP_STRING(sourceMeshGUID, NavMeshComponent),
        PROP_BOOL(isNavMeshSource, NavMeshComponent),
        PROP_BOOL(runtimeGenerated, NavMeshComponent),
        PROP_FLOAT(agentRadius, NavMeshComponent),
        PROP_FLOAT(agentHeight, NavMeshComponent),
        PROP_FLOAT(cellSize, NavMeshComponent),
        PROP_FLOAT(cellHeight, NavMeshComponent),
        PROP_FLOAT(maxSlope, NavMeshComponent),
    };

    inline constexpr prop_list g_NavMeshProps{ "NavMeshComponent", g_NavMeshPropsArray };
    inline global_registration g_NavMeshPropsReg(&g_NavMeshProps);
}

#endif // !_NAVMESH_COMPONENT_H_
