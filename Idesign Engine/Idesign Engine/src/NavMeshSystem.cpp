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
#include "NavMeshAgentProperties.h"

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
        CleanupNavData();
        if (heightfield) rcFreeHeightField(heightfield);
        if (compactHeightfield) rcFreeCompactHeightfield(compactHeightfield);
        if (contourSet) rcFreeContourSet(contourSet);
        if (polyMesh) rcFreePolyMesh(polyMesh);
        if (polyMeshDetail) rcFreePolyMeshDetail(polyMeshDetail);
        if (navQuery) dtFreeNavMeshQuery(navQuery);
        if (navMesh) dtFreeNavMesh(navMesh);
    }

    void NavMeshSystem::Initialize() {

        // REgister here next time
        //ecsInterface.RegisterComponent<NavMeshComponent>();

        auto navSystem = ecsInterface.GetSystem<NavMeshSystem>();
        if (navSystem) {
            navSystem->BuildNavMesh();
        }

        // Build immediately for demo
        
        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<TransformComponent>()); // Register transform so i have access to all entities, then retrieve via checks agent and navmesh comp
        //loop for agent

        ecsInterface.SetSystemSignature<NavMeshSystem>(signature);
        //std::cout << "Signature for Navmesh system is: " << signature << std::endl;
    }

    void NavMeshSystem::Update(float deltaTime) {
        // Loop through all entities (all have TransformComponent due to signature)
        for (auto entity : mEntities) {
            // Check if entity has NavMeshAgentComponent
            if (!ecsInterface.HasComponent<NavMeshAgentComponent>(entity)) {
                continue; // Skip entities without agent component
            }

            auto& agent = ecsInterface.GetComponent<NavMeshAgentComponent>(entity);
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);

            if (isNavMeshBuilt && navQuery) {
                glm::vec3 currentXZ = transform.position;
                glm::vec3 snapped = GetNearestPointOnNavmesh(currentXZ);
                transform.position.y = snapped.y;
            }

            // If agent doesn't have a path, skip
            if (!agent.hasPath || agent.currentPath.empty()) {
                agent.isMoving = false;
                agent.currentSpeed = 0.0f;
                continue;
            }

            // Agent has a path, move towards current waypoint
            agent.isMoving = true;

            // Get current waypoint
            if (agent.currentWaypointIndex >= agent.currentPath.size()) {
                // Reached end of path
                agent.hasPath = false;
                agent.isMoving = false;
                agent.currentPath.clear();
                agent.currentWaypointIndex = 0;
                agent.currentSpeed = 0.0f;
              //  std::cout << "[NavMeshSystem] Entity " << entity << " reached destination!" << std::endl;
                continue;
            }

            glm::vec3 waypoint = agent.currentPath[agent.currentWaypointIndex];
            glm::vec3 direction = waypoint - transform.position;
            float distanceToWaypoint = glm::length(direction);

            // Check if reached waypoint
            if (distanceToWaypoint < agent.stoppingDistance) {
                agent.currentWaypointIndex++;

                // If that was the last waypoint
                if (agent.currentWaypointIndex >= agent.currentPath.size()) {
                    if (agent.stopOnTargetReached) {
                        agent.hasPath = false;
                        agent.isMoving = false;
                        agent.currentPath.clear();
                        agent.currentWaypointIndex = 0;
                        agent.currentSpeed = 0.0f;
               //         std::cout << "[NavMeshSystem] Entity " << entity << " reached final destination!" << std::endl;
                    }
                }
                continue;
            }

            // Normalize direction
            direction = glm::normalize(direction);

            // Apply acceleration (smooth speed changes)
            if (agent.currentSpeed < agent.moveSpeed) {
                agent.currentSpeed += agent.acceleration * deltaTime;
                agent.currentSpeed = std::min(agent.currentSpeed, agent.moveSpeed);
            }

            // Move towards waypoint
            glm::vec3 movement = direction * agent.currentSpeed * deltaTime;

            // Don't overshoot the waypoint
            if (glm::length(movement) > distanceToWaypoint) {
                transform.position = waypoint;
            }
            else {
                transform.position += movement;
            }

            // Optional: Rotate agent to face movement direction (around Y axis)
            if (agent.angularSpeed > 0.0f && glm::length(direction) > 0.001f) {
                // Calculate target yaw
                float targetYaw = glm::degrees(atan2f(direction.x, direction.z));

                //angle offset
                targetYaw += agent.rotationOffset;  // Apply model-specific offset

                // Smoothly rotate towards target
                float currentYaw = transform.rotation.y;
                float angleDiff = targetYaw - currentYaw;

                // Normalize angle difference to [-180, 180]
                while (angleDiff > 180.0f) angleDiff -= 360.0f;
                while (angleDiff < -180.0f) angleDiff += 360.0f;

                float maxRotation = agent.angularSpeed * deltaTime;
                if (fabs(angleDiff) < maxRotation) {
                    transform.rotation.y = targetYaw;
                }
                else {
                    transform.rotation.y += (angleDiff > 0 ? maxRotation : -maxRotation);
                }
            }
        }
    }

    std::string NavMeshSystem::GetName() {
        return "NavMeshSystem";
    }

    void NavMeshSystem::BuildNavMesh() {
        CleanupNavData();
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
        // In ConvertToRecastFormat(), try more aggressive settings:
        config.cs = 0.4f;  // Bigger cells = less detail but more robust
        config.ch = 0.3f;
        config.walkableSlopeAngle = 60.0f;  // Allow steeper slopes
        config.walkableHeight = (int)ceilf(1.5f / config.ch);  // Lower height requirement
        config.walkableRadius = (int)ceilf(0.4f / config.cs);  // Smaller radius
        config.minRegionArea = (int)rcSqr(3);  // Allow smaller regions
        config.walkableClimb = (int)floorf(0.9f / config.ch);

        config.maxEdgeLen = (int)(12.0f / config.cs);
        config.maxSimplificationError = 1.3f;
        config.minRegionArea = (int)rcSqr(3);
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

       // std::cout << "[FindPath] Searching path from (" << start.x << ", " << start.y << ", " << start.z
        //    << ") to (" << end.x << ", " << end.y << ", " << end.z << ")" << std::endl;

        dtPolyRef startRef = 0, endRef = 0;
        float startNearest[3], endNearest[3];

        // CRITICAL FIX: Much larger Y extent to cover entire navmesh
        float extents[3] = { 10.0f, 50.0f, 10.0f }; // Increased Y from 8 to 50!
        dtQueryFilter filter;

        float startPos[3] = { start.x, start.y, start.z };
        float endPos[3] = { end.x, end.y, end.z };

        dtStatus startStatus = navQuery->findNearestPoly(startPos, extents, &filter, &startRef, startNearest);
        dtStatus endStatus = navQuery->findNearestPoly(endPos, extents, &filter, &endRef, endNearest);

     //   std::cout << "[FindPath] Start poly search:" << std::endl;
     //   std::cout << "  Status: " << (dtStatusSucceed(startStatus) ? "SUCCESS" : "FAILED") << std::endl;
      //  std::cout << "  PolyRef: " << startRef << " (0 = not found)" << std::endl;


  //      std::cout << "[FindPath] End poly search:" << std::endl;
   //     std::cout << "  Status: " << (dtStatusSucceed(endStatus) ? "SUCCESS" : "FAILED") << std::endl;
    //    std::cout << "  PolyRef: " << endRef << " (0 = not found)" << std::endl;
        if (endRef) {
      //      std::cout << "  Nearest point: (" << endNearest[0] << ", " << endNearest[1] << ", " << endNearest[2] << ")" << std::endl;
        }

        if (!startRef || !endRef) {
        //    std::cerr << "[NavMeshSystem] Start or end not on navmesh!" << std::endl;
//            if (!startRef) std::cerr << "  - Start position not found (try larger search area)" << std::endl;
  //          if (!endRef) std::cerr << "  - End position not found (try larger search area)" << std::endl;
            return false;
        }

        // Rest of the function stays the same...
        dtPolyRef path[256];
        int pathCount = 0;
        dtStatus pathStatus = navQuery->findPath(startRef, endRef, startNearest, endNearest,
            &filter, path, &pathCount, 256);

     //   std::cout << "[FindPath] Path search: " << pathCount << " polygons" << std::endl;

        if (pathCount == 0) {
  //          std::cerr << "[NavMeshSystem] No path found between points!" << std::endl;
            return false;
        }

        float straightPath[256 * 3];
        unsigned char straightPathFlags[256];
        dtPolyRef straightPathRefs[256];
        int straightPathCount = 0;

        navQuery->findStraightPath(startNearest, endNearest, path, pathCount,
            straightPath, straightPathFlags, straightPathRefs,
            &straightPathCount, 256, DT_STRAIGHTPATH_ALL_CROSSINGS);

        outPath.clear();
        for (int i = 0; i < straightPathCount; ++i) {
            outPath.emplace_back(
                straightPath[i * 3 + 0],
                straightPath[i * 3 + 1],
                straightPath[i * 3 + 2]
            );
        }

  //      std::cout << "[NavMeshSystem] ✓ Path found with " << straightPathCount << " waypoints" << std::endl;
        return true;
    }

    void NavMeshSystem::PrintNavMeshInfo() const {
        if (!navMesh) {
            std::cout << "[NavMesh] No navmesh built!" << std::endl;
            return;
        }

        std::cout << "\n=== NAVMESH INFO ===" << std::endl;
        std::cout << "Tile count: " << navMesh->getMaxTiles() << std::endl;

        for (int i = 0; i < navMesh->getMaxTiles(); ++i) {
            const dtMeshTile* tile = navMesh->getTile(i);
            if (!tile || !tile->header) continue;

            std::cout << "Tile " << i << ":" << std::endl;
            std::cout << "  Polys: " << tile->header->polyCount << std::endl;
            std::cout << "  Verts: " << tile->header->vertCount << std::endl;
            std::cout << "  Bounds: min(" << tile->header->bmin[0] << ", " << tile->header->bmin[1] << ", " << tile->header->bmin[2] << ")" << std::endl;
            std::cout << "         max(" << tile->header->bmax[0] << ", " << tile->header->bmax[1] << ", " << tile->header->bmax[2] << ")" << std::endl;
        }

        std::cout << "Query object: " << (navQuery ? "EXISTS" : "NULL") << std::endl;
        std::cout << "====================\n" << std::endl;
    }

    glm::vec3 NavMeshSystem::GetNearestPointOnNavmesh(const glm::vec3& point) {
        if (!navQuery) return point;

        dtPolyRef nearestRef;
        float nearestPt[3];
        float extents[3] = { 10.0f, 50.0f, 10.0f }; // Match FindPath extents!
        dtQueryFilter filter;

        float queryPos[3] = { point.x, point.y, point.z };
        navQuery->findNearestPoly(queryPos, extents, &filter, &nearestRef, nearestPt);

        if (nearestRef) {
            return glm::vec3(nearestPt[0], nearestPt[1], nearestPt[2]);
        }
        return point;
    }

    void NavMeshSystem::SetAgentDestination(Entity entity, const glm::vec3& destination) {
        if (!ecsInterface.HasComponent<NavMeshAgentComponent>(entity)) {
            std::cerr << "[NavMeshSystem] Entity " << entity << " has no NavMeshAgentComponent!" << std::endl;
            return;
        }

        if (!ecsInterface.HasComponent<TransformComponent>(entity)) {
            std::cerr << "[NavMeshSystem] Entity " << entity << " has no TransformComponent!" << std::endl;
            return;
        }

        auto& agent = ecsInterface.GetComponent<NavMeshAgentComponent>(entity);
        auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);

        // Update target position
        agent.targetPosition = destination;

        // Find path from current position to destination
        std::vector<glm::vec3> path;
        if (FindPath(transform.position, destination, path)) {
            agent.currentPath = path;
            agent.currentWaypointIndex = 0;
            agent.hasPath = true;
            agent.isMoving = true;
            agent.currentSpeed = 0.0f; // Reset speed for smooth acceleration

  //          std::cout << "[NavMeshSystem] Entity " << entity << " path set with "
    //            << path.size() << " waypoints" << std::endl;
        }
        else {
            std::cerr << "[NavMeshSystem] Failed to find path for entity " << entity << std::endl;
            agent.hasPath = false;
            agent.isMoving = false;
        }
    }

    void NavMeshSystem::DebugDraw() const {
        if (!isNavMeshBuilt) return;
        // Optional: use ImGui or debug renderer to show navmesh polygons
    }

    void ScriptPathfindTo(const std::string& agentName, const std::string& targetName)
    {

        auto navSystem = ecsInterface.GetSystem<Framework::NavMeshSystem>();
        if (!navSystem)
            return;

        Entity agent = ecsInterface.GetEntityByName(agentName);
        Entity target = ecsInterface.GetEntityByName(targetName);

        if (!ecsInterface.IsEntityValid(agent) || !ecsInterface.IsEntityValid(target))
        {
            std::cerr << "[Lua] Invalid agent or target name!" << std::endl;
            return;
        }

        if (!ecsInterface.HasComponent<NavMeshAgentComponent>(agent) ||
            !ecsInterface.HasComponent<TransformComponent>(agent) ||
            !ecsInterface.HasComponent<TransformComponent>(target))
        {
            std::cerr << "[Lua] Missing required components!" << std::endl;
            return;
        }

        auto& agentXform = ecsInterface.GetComponent<TransformComponent>(agent);
        auto& targetXform = ecsInterface.GetComponent<TransformComponent>(target);

        glm::vec3 from = navSystem->GetNearestPointOnNavmesh(agentXform.position);
        glm::vec3 to = navSystem->GetNearestPointOnNavmesh(targetXform.position);

   //     std::cout << "[Lua] Pathfinding from '" << agentName << "' to '" << targetName << "'..." << std::endl;

        navSystem->SetAgentDestination(agent, to);
    }

    void NavMeshSystem::CleanupNavData() {
        if (heightfield) { rcFreeHeightField(heightfield); heightfield = nullptr; }
        if (compactHeightfield) { rcFreeCompactHeightfield(compactHeightfield); compactHeightfield = nullptr; }
        if (contourSet) { rcFreeContourSet(contourSet); contourSet = nullptr; }
        if (polyMesh) { rcFreePolyMesh(polyMesh); polyMesh = nullptr; }
        if (polyMeshDetail) { rcFreePolyMeshDetail(polyMeshDetail); polyMeshDetail = nullptr; }
        if (navQuery) { dtFreeNavMeshQuery(navQuery); navQuery = nullptr; }
        if (navMesh) { dtFreeNavMesh(navMesh); navMesh = nullptr; }

        isNavMeshBuilt = false;
        std::cout << "[NavMeshSystem] Cleaned up old navmesh data." << std::endl;
    }
}
