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
        if (heightfield) rcFreeHeightField(heightfield);
        if (compactHeightfield) rcFreeCompactHeightfield(compactHeightfield);
        if (contourSet) rcFreeContourSet(contourSet);
        if (polyMesh) rcFreePolyMesh(polyMesh);
        if (polyMeshDetail) rcFreePolyMeshDetail(polyMeshDetail);
        if (navQuery) dtFreeNavMeshQuery(navQuery);
        if (navMesh) dtFreeNavMesh(navMesh);
    }

    void NavMeshSystem::Initialize() {
        // Load settings or allocate navmesh objects
        //Initialise and register a navmesh component

     //   ecsInterface.RegisterComponent<NavMeshComponent>();

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
            // Check if entity has NavMeshComponent
            if (!ecsInterface.HasComponent<NavMeshComponent>(entity)) {
                continue;
            }

            auto& navComp = ecsInterface.GetComponent<NavMeshComponent>(entity);

            // Skip if not marked as navmesh source
            if (!navComp.isNavMeshSource) {
                continue;
            }

            // Check if entity has MeshProperties
            if (!ecsInterface.HasComponent<MeshProperties>(entity)) {
                std::cerr << "[NavMeshSystem] Entity " << entity
                    << " marked as navmesh source but has no MeshProperties!" << std::endl;
                continue;
            }

            auto& meshComp = ecsInterface.GetComponent<MeshProperties>(entity);

            // USE THE ENTITY'S MESH DATA DIRECTLY (not AssetManager)
            if (meshComp.positions.empty() || meshComp.indices.empty()) {
                std::cerr << "[NavMeshSystem] Entity " << entity
                    << " has empty mesh data! Path: " << meshComp.path << std::endl;
                continue;
            }

            // Optionally transform positions by entity's transform
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), transform.position)
                * glm::scale(glm::mat4(1.0f), transform.scale);

            // Append mesh positions (with transformation)
            for (const auto& pos : meshComp.positions) {
                glm::vec4 worldPos = modelMatrix * glm::vec4(pos, 1.0f);
                collectedPositions.push_back(glm::vec3(worldPos));
            }

            // Append indices with offset
            for (const auto& idx : meshComp.indices) {
                collectedIndices.push_back(idx + indexOffset);
            }

            indexOffset += static_cast<int>(meshComp.positions.size());

            std::cout << "[NavMeshSystem] Collected mesh from entity " << entity
                << ": " << meshComp.positions.size() << " verts, "
                << meshComp.indices.size() / 3 << " triangles." << std::endl;
        }

        std::cout << "[NavMeshSystem] Total collected geometry: "
            << collectedPositions.size() << " verts, "
            << collectedIndices.size() / 3 << " triangles.\n";
    }


    void NavMeshSystem::ConvertToRecastFormat() {
        if (collectedPositions.empty() || collectedIndices.empty()) return;

        // Convert to flat float array for Recast
        std::vector<float> vertsFlat;
        vertsFlat.reserve(collectedPositions.size() * 3);
        for (const auto& v : collectedPositions) {
            vertsFlat.push_back(v.x);
            vertsFlat.push_back(v.y);
            vertsFlat.push_back(v.z);
        }

        // Calculate bounding box
        float bmin[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
        float bmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        for (const auto& v : collectedPositions) {
            bmin[0] = std::min(bmin[0], v.x);
            bmin[1] = std::min(bmin[1], v.y);
            bmin[2] = std::min(bmin[2], v.z);
            bmax[0] = std::max(bmax[0], v.x);
            bmax[1] = std::max(bmax[1], v.y);
            bmax[2] = std::max(bmax[2], v.z);
        }

        std::cout << "[NavMeshSystem] Bounds: min(" << bmin[0] << "," << bmin[1] << "," << bmin[2]
            << ") max(" << bmax[0] << "," << bmax[1] << "," << bmax[2] << ")" << std::endl;

        // Initialize Recast context
        ::rcContext ctx;

        // Configure navmesh build parameters
        ::rcConfig config{};
        memset(&config, 0, sizeof(config));

        config.cs = 0.3f;  // cell size
        config.ch = 0.2f;  // cell height
        config.walkableSlopeAngle = 45.0f;
        config.walkableHeight = (int)ceilf(2.0f / config.ch);
        config.walkableClimb = (int)floorf(0.9f / config.ch);
        config.walkableRadius = (int)ceilf(0.6f / config.cs);
        config.maxEdgeLen = (int)(12.0f / config.cs);
        config.maxSimplificationError = 1.3f;
        config.minRegionArea = (int)rcSqr(8);
        config.mergeRegionArea = (int)rcSqr(20);
        config.maxVertsPerPoly = 6;
        config.detailSampleDist = 6.0f * config.cs;
        config.detailSampleMaxError = 1.0f * config.ch;

        // Set bounding box
        rcVcopy(config.bmin, bmin);
        rcVcopy(config.bmax, bmax);
        rcCalcGridSize(config.bmin, config.bmax, config.cs, &config.width, &config.height);

        std::cout << "[NavMeshSystem] Grid size: " << config.width << "x" << config.height << std::endl;

        // Step 1: Allocate heightfield
        heightfield = ::rcAllocHeightfield();
        if (!heightfield) {
            std::cerr << "[NavMeshSystem] Failed to allocate heightfield!" << std::endl;
            return;
        }

        if (!::rcCreateHeightfield(&ctx, *heightfield, config.width, config.height,
            config.bmin, config.bmax, config.cs, config.ch)) {
            std::cerr << "[NavMeshSystem] Failed to create heightfield!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Created heightfield" << std::endl;

        // Step 2: Rasterize triangles into heightfield
        std::vector<unsigned char> triAreas(collectedIndices.size() / 3);
        memset(triAreas.data(), 0, triAreas.size());

        ::rcMarkWalkableTriangles(&ctx, config.walkableSlopeAngle,
            vertsFlat.data(), collectedPositions.size(),
            collectedIndices.data(), collectedIndices.size() / 3,
            triAreas.data());

        if (!::rcRasterizeTriangles(&ctx, vertsFlat.data(), collectedPositions.size(),
            collectedIndices.data(), triAreas.data(),
            collectedIndices.size() / 3, *heightfield, config.walkableClimb)) {
            std::cerr << "[NavMeshSystem] Failed to rasterize triangles!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Rasterized triangles" << std::endl;

        // Step 3: Filter heightfield
        ::rcFilterLowHangingWalkableObstacles(&ctx, config.walkableClimb, *heightfield);
        ::rcFilterLedgeSpans(&ctx, config.walkableHeight, config.walkableClimb, *heightfield);
        ::rcFilterWalkableLowHeightSpans(&ctx, config.walkableHeight, *heightfield);
        std::cout << "[NavMeshSystem] ✓ Filtered heightfield" << std::endl;

        // Step 4: Build compact heightfield
        compactHeightfield = ::rcAllocCompactHeightfield();
        if (!compactHeightfield) {
            std::cerr << "[NavMeshSystem] Failed to allocate compact heightfield!" << std::endl;
            return;
        }

        if (!::rcBuildCompactHeightfield(&ctx, config.walkableHeight, config.walkableClimb,
            *heightfield, *compactHeightfield)) {
            std::cerr << "[NavMeshSystem] Failed to build compact heightfield!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built compact heightfield" << std::endl;

        // Free heightfield
        ::rcFreeHeightField(heightfield);
        heightfield = nullptr;

        // Step 5: Erode walkable area
        if (!::rcErodeWalkableArea(&ctx, config.walkableRadius, *compactHeightfield)) {
            std::cerr << "[NavMeshSystem] Failed to erode walkable area!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Eroded walkable area" << std::endl;

        // Step 6: Build distance field and regions
        if (!::rcBuildDistanceField(&ctx, *compactHeightfield)) {
            std::cerr << "[NavMeshSystem] Failed to build distance field!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built distance field" << std::endl;

        if (!::rcBuildRegions(&ctx, *compactHeightfield, 0, config.minRegionArea, config.mergeRegionArea)) {
            std::cerr << "[NavMeshSystem] Failed to build regions!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built regions" << std::endl;

        // Step 7: Build contours
        contourSet = ::rcAllocContourSet();
        if (!contourSet) {
            std::cerr << "[NavMeshSystem] Failed to allocate contour set!" << std::endl;
            return;
        }

        if (!::rcBuildContours(&ctx, *compactHeightfield, config.maxSimplificationError,
            config.maxEdgeLen, *contourSet)) {
            std::cerr << "[NavMeshSystem] Failed to build contours!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built " << contourSet->nconts << " contours" << std::endl;

        // Step 8: Build polygon mesh
        polyMesh = ::rcAllocPolyMesh();
        if (!polyMesh) {
            std::cerr << "[NavMeshSystem] Failed to allocate poly mesh!" << std::endl;
            return;
        }

        if (!::rcBuildPolyMesh(&ctx, *contourSet, config.maxVertsPerPoly, *polyMesh)) {
            std::cerr << "[NavMeshSystem] Failed to build poly mesh!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built poly mesh with " << polyMesh->npolys << " polygons" << std::endl;

        // Step 9: Build detail mesh
        polyMeshDetail = ::rcAllocPolyMeshDetail();
        if (!polyMeshDetail) {
            std::cerr << "[NavMeshSystem] Failed to allocate poly mesh detail!" << std::endl;
            return;
        }

        if (!::rcBuildPolyMeshDetail(&ctx, *polyMesh, *compactHeightfield,
            config.detailSampleDist, config.detailSampleMaxError,
            *polyMeshDetail)) {
            std::cerr << "[NavMeshSystem] Failed to build detail mesh!" << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Built detail mesh" << std::endl;

        // Step 10: Create Detour navmesh
        for (int i = 0; i < polyMesh->npolys; ++i) {
            if (polyMesh->areas[i] == RC_WALKABLE_AREA) {
                polyMesh->areas[i] = 0; // Polyarea type
                polyMesh->flags[i] = 1;  // Walkable flag
            }
        }

        ::dtNavMeshCreateParams params{};
        memset(&params, 0, sizeof(params));

        params.verts = polyMesh->verts;
        params.vertCount = polyMesh->nverts;
        params.polys = polyMesh->polys;
        params.polyAreas = polyMesh->areas;
        params.polyFlags = polyMesh->flags;
        params.polyCount = polyMesh->npolys;
        params.nvp = polyMesh->nvp;
        params.detailMeshes = polyMeshDetail->meshes;
        params.detailVerts = polyMeshDetail->verts;
        params.detailVertsCount = polyMeshDetail->nverts;
        params.detailTris = polyMeshDetail->tris;
        params.detailTriCount = polyMeshDetail->ntris;

        // CRITICAL: Set these parameters
        params.walkableHeight = 2.0f;
        params.walkableRadius = 0.6f;
        params.walkableClimb = 0.9f;
        rcVcopy(params.bmin, polyMesh->bmin);
        rcVcopy(params.bmax, polyMesh->bmax);
        params.cs = config.cs;
        params.ch = config.ch;
        params.buildBvTree = true;

        // IMPORTANT: Set tile coordinates (even for single-tile navmesh)
        params.tileX = 0;
        params.tileY = 0;
        params.tileLayer = 0;

        std::cout << "[NavMeshSystem] Creating navmesh data..." << std::endl;
        std::cout << "  Verts: " << params.vertCount << ", Polys: " << params.polyCount << std::endl;

        unsigned char* navData = nullptr;
        int navDataSize = 0;

        if (!::dtCreateNavMeshData(&params, &navData, &navDataSize)) {
            std::cerr << "[NavMeshSystem] Failed to create Detour navmesh data!" << std::endl;
            std::cerr << "  Check if polygon count is valid: " << params.polyCount << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Created navmesh data (" << navDataSize << " bytes)" << std::endl;

        navMesh = ::dtAllocNavMesh();
        if (!navMesh) {
            ::dtFree(navData);
            std::cerr << "[NavMeshSystem] Failed to allocate navmesh!" << std::endl;
            return;
        }

        dtStatus status = navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA);
        if (dtStatusFailed(status)) {
            ::dtFree(navData);
            std::cerr << "[NavMeshSystem] Failed to initialize Detour navmesh! Status: " << status << std::endl;
            return;
        }
        std::cout << "[NavMeshSystem] ✓ Initialized navmesh" << std::endl;

        navQuery = ::dtAllocNavMeshQuery();
        if (!navQuery) {
            std::cerr << "[NavMeshSystem] Failed to allocate navmesh query!" << std::endl;
            return;
        }

        status = navQuery->init(navMesh, 2048);
        if (dtStatusFailed(status)) {
            std::cerr << "[NavMeshSystem] Failed to initialize navmesh query! Status: " << status << std::endl;
            return;
        }

        std::cout << "[NavMeshSystem] ✅ Successfully built navigation mesh with "
            << polyMesh->npolys << " polygons!" << std::endl;
    }

    // Nav mesh Utilization Functions

    bool NavMeshSystem::FindPath(const glm::vec3& start, const glm::vec3& end,
        std::vector<glm::vec3>& outPath) {
        if (!isNavMeshBuilt || !navQuery) {
            std::cerr << "[NavMeshSystem] Navmesh not built!" << std::endl;
            return false;
        }

        // Find nearest poly to start/end positions
        dtPolyRef startRef, endRef;
        float startNearest[3], endNearest[3];
        float extents[3] = { 2.0f, 4.0f, 2.0f }; // Search radius
        dtQueryFilter filter;

        navQuery->findNearestPoly(&start.x, extents, &filter, &startRef, startNearest);
        navQuery->findNearestPoly(&end.x, extents, &filter, &endRef, endNearest);

        if (!startRef || !endRef) {
            std::cerr << "[NavMeshSystem] Start or end not on navmesh!" << std::endl;
            return false;
        }

        // Find path
        dtPolyRef path[256];
        int pathCount = 0;
        navQuery->findPath(startRef, endRef, startNearest, endNearest,
            &filter, path, &pathCount, 256);

        if (pathCount == 0) {
            std::cerr << "[NavMeshSystem] No path found!" << std::endl;
            return false;
        }

        // Get smooth path (straight path)
        float straightPath[256 * 3];
        unsigned char straightPathFlags[256];
        dtPolyRef straightPathRefs[256];
        int straightPathCount = 0;

        navQuery->findStraightPath(startNearest, endNearest, path, pathCount,
            straightPath, straightPathFlags, straightPathRefs,
            &straightPathCount, 256, DT_STRAIGHTPATH_ALL_CROSSINGS);

        // Convert to glm::vec3
        outPath.clear();
        for (int i = 0; i < straightPathCount; ++i) {
            outPath.emplace_back(
                straightPath[i * 3 + 0],
                straightPath[i * 3 + 1],
                straightPath[i * 3 + 2]
            );
        }

        std::cout << "[NavMeshSystem] Path found with " << straightPathCount << " points" << std::endl;
        return true;
    }

    glm::vec3 NavMeshSystem::GetNearestPointOnNavmesh(const glm::vec3& point) {
        if (!navQuery) return point;

        dtPolyRef nearestRef;
        float nearestPt[3];
        float extents[3] = { 2.0f, 4.0f, 2.0f };
        dtQueryFilter filter;

        navQuery->findNearestPoly(&point.x, extents, &filter, &nearestRef, nearestPt);

        if (nearestRef) {
            return glm::vec3(nearestPt[0], nearestPt[1], nearestPt[2]);
        }
        return point;
    }
    void NavMeshSystem::DebugDraw() const {
        if (!isNavMeshBuilt) return;
        // Optional: use ImGui or debug renderer to show navmesh polygons
    }
}
