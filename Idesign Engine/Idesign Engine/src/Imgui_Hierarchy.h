///////////////////////////////////////////////////////////////////////////////
///
///	\file Imgui_Hierarchy.h
/// 
/// * This H files include helper functions for handing Inspector entity hierarchy functionalities for imgui_manager.
/// 
/// 
///	Authors: Joshua Sim Yue Chen
///	Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once // Included all necessary h, pragma once should handle everything well but not good practice.
#include "pch.h"
#include <windows.h>
#include <commdlg.h>
#include "Graphics.h"
#include <algorithm>
#include <InputHandler.h>
#include "ComponentList.h"
#include "FontSystem.h"
#include "Coordinator.h"
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "EngineState.h"
#include "Debugger.h"
#include <ImguiManager.h>
#include "SceneManager.h"



  // ------ Hierarchy helper functions ----- //

extern Framework::Coordinator ecsInterface;


// ---------- Helpers for hierarchy reordering ---------- //

// Renumbers sortIndex sequentially for a parent's children or for a scene root if parentGUID empty.
static void UpdateSiblingSortIndices(const std::string& parentGUID, const std::string& sceneName)
{
    if (!parentGUID.empty())
    {
        Entity parent = ecsInterface.GetEntityByGUID(parentGUID);
        if (!ecsInterface.IsEntityValid(parent)) return;
        auto& children = ecsInterface.GetComponent<HierarchyComponent>(parent).childrenGUIDs;

        for (int i = 0; i < (int)children.size(); ++i)
        {
            Entity child = ecsInterface.GetEntityByGUID(children[i]);
            if (!ecsInterface.IsEntityValid(child)) continue;
            auto& chHierarchy = ecsInterface.GetComponent<HierarchyComponent>(child);
            chHierarchy.sortIndex = i;
        }
    }
    else
    {
        // scene-level root ordering
        // if sceneName provided, renumber that scene; otherwise renumber all scenes where entity may appear
        if (!sceneName.empty())
        {
            auto sceneMap = Framework::sceneManager.GetSceneEntities();
            auto it = sceneMap.find(sceneName);
            if (it == sceneMap.end()) return;

            auto& roots = it->second;
            for (int i = 0; i < (int)roots.size(); ++i)
            {
                Entity rootE = roots[i];
                if (!ecsInterface.IsEntityValid(rootE)) continue;
                auto& h = ecsInterface.GetComponent<HierarchyComponent>(rootE);
                h.sortIndex = i;
            }
        }
        else
        {
            // fallback: update all scenes' roots
            for (const auto& [sname, ents] : Framework::sceneManager.GetSceneEntities())
            {
                UpdateSiblingSortIndices("", sname);
            }
        }
    }
}

static std::string FindSceneNameForEntity(Entity e)
{
    // brute-force search; fine for editor use
    for (const auto& [sceneName, ents] : Framework::sceneManager.GetSceneEntities())
    {
        for (Entity se : ents)
            if (se == e) return sceneName;
    }
    return std::string{};
}

// Get all non-scene root entities sorted by sortIndex
static std::vector<Entity> GetNonSceneRootEntities()
{
    std::vector<Entity> nonSceneRoots;

    // Build set of all entities registered to scenes
    std::unordered_set<Entity> registeredEntities;
    for (const auto& [sceneName, entities] : Framework::sceneManager.GetSceneEntities())
    {
        registeredEntities.insert(entities.begin(), entities.end());
    }

    // Find all root entities not in any scene
    for (Entity entity : ecsInterface.GetEntities())
    {
        if (!registeredEntities.count(entity) && ecsInterface.IsEntityValid(entity))
        {
            const auto& h = ecsInterface.GetComponent<HierarchyComponent>(entity);
            if (h.parentGUID.empty())
            {
                nonSceneRoots.push_back(entity);
            }
        }
    }

    // Sort by sortIndex
    std::sort(nonSceneRoots.begin(), nonSceneRoots.end(), [](Entity a, Entity b) {
        return ecsInterface.GetComponent<HierarchyComponent>(a).sortIndex <
            ecsInterface.GetComponent<HierarchyComponent>(b).sortIndex;
        });

    return nonSceneRoots;
}

// Update sortIndex for all non-scene root entities sequentially
static void UpdateNonSceneSortIndices()
{
    auto nonSceneRoots = GetNonSceneRootEntities();
    for (int i = 0; i < (int)nonSceneRoots.size(); ++i)
    {
        auto& h = ecsInterface.GetComponent<HierarchyComponent>(nonSceneRoots[i]);
        h.sortIndex = i;
    }
}

// Reorder a non-scene root entity to a specific index
static void ReorderNonSceneEntity(const std::string& childGUID, int targetIndex)
{
    Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
    if (!ecsInterface.IsEntityValid(childEntity)) return;

    auto nonSceneRoots = GetNonSceneRootEntities();

    // Find current position
    int currentIndex = -1;
    for (int i = 0; i < (int)nonSceneRoots.size(); ++i)
    {
        if (nonSceneRoots[i] == childEntity)
        {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == -1) return;

    // Calculate target position in the array AFTER conceptual removal
    // The array currently has 'size' elements, after removal it will have 'size - 1'
    // Valid target positions are [0, size-1] where size-1 means "at the end"
    if (targetIndex >= (int)nonSceneRoots.size())
    {
        targetIndex = (int)nonSceneRoots.size() - 1; // Clamp to last position
    }

    // If we're moving to the same position, nothing to do
    if (currentIndex == targetIndex) return;

    // Perform the reordering by adjusting sortIndices
    if (currentIndex < targetIndex)
    {
        // Moving down: shift entities between current and target up
        for (int i = currentIndex + 1; i <= targetIndex; ++i)
        {
            auto& h = ecsInterface.GetComponent<HierarchyComponent>(nonSceneRoots[i]);
            h.sortIndex--;
        }
    }
    else
    {
        // Moving up: shift entities between target and current down
        for (int i = targetIndex; i < currentIndex; ++i)
        {
            auto& h = ecsInterface.GetComponent<HierarchyComponent>(nonSceneRoots[i]);
            h.sortIndex++;
        }
    }

    // Set new sortIndex for the moved entity
    auto& h = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
    h.sortIndex = targetIndex;
}

// Remove `childGUID` from its current parent's children vector (or scene's root list).
static void RemoveChildFromParent(const std::string& childGUID)
{
    Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
    if (!ecsInterface.IsEntityValid(childEntity)) return;

    auto& childHierarchy = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
    std::string oldParentGUID = childHierarchy.parentGUID;

    if (!oldParentGUID.empty())
    {
        // Child has a parent - remove from parent's children list
        Entity oldParent = ecsInterface.GetEntityByGUID(oldParentGUID);
        if (ecsInterface.IsEntityValid(oldParent))
        {
            auto& oldChildren = ecsInterface.GetComponent<HierarchyComponent>(oldParent).childrenGUIDs;
            oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), childGUID), oldChildren.end());
            UpdateSiblingSortIndices(oldParentGUID, "");
        }
    }
    else
    {
        // Child is a root - check if it's in a scene or non-scene
        std::string sceneName = FindSceneNameForEntity(childEntity);

        if (!sceneName.empty())
        {
            // Remove from scene list
            auto& sceneVec = Framework::sceneManager.GetSceneEntities().at(sceneName);
            auto it = std::find(sceneVec.begin(), sceneVec.end(), childEntity);
            if (it != sceneVec.end())
            {
                sceneVec.erase(it);
                UpdateSiblingSortIndices("", sceneName);
            }
        }
        else
        {
            // Non-scene entity - just update sortIndices
            UpdateNonSceneSortIndices();
        }
    }
}

// Insert childGUID into parent's children vector at index (0..n). If parentGUID empty -> insert into scene root list using sceneName.
static void InsertChildAt(const std::string& parentGUID, const std::string& childGUID, int index, const std::string& sceneName = "")
{
    Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
    if (!ecsInterface.IsEntityValid(childEntity)) return;

    if (!parentGUID.empty())
    {
        // Has a parent - standard parenting logic
        Entity parentEntity = ecsInterface.GetEntityByGUID(parentGUID);
        if (!ecsInterface.IsEntityValid(parentEntity)) return;
        auto& children = ecsInterface.GetComponent<HierarchyComponent>(parentEntity).childrenGUIDs;

        index = std::max(0, std::min<int>(index, (int)children.size()));
        children.insert(children.begin() + index, childGUID);

        ecsInterface.GetComponent<HierarchyComponent>(childEntity).parentGUID = parentGUID;
        UpdateSiblingSortIndices(parentGUID, "");
    }
    else
    {
        // No parent - becoming a root entity
        if (sceneName.empty())
        {
            // Non-scene entity - just set sortIndex
            auto& h = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
            h.parentGUID.clear();
            h.sortIndex = index;
            UpdateNonSceneSortIndices();
            return;
        }

        // Scene entity - insert into scene vector
        auto& sceneMap = Framework::sceneManager.GetSceneEntities();
        auto sceneIt = sceneMap.find(sceneName);
        if (sceneIt == sceneMap.end()) return;

        auto& rootEntities = sceneIt->second;
        index = std::max(0, std::min<int>(index, (int)rootEntities.size()));

        rootEntities.insert(rootEntities.begin() + index, childEntity);

        ecsInterface.GetComponent<HierarchyComponent>(childEntity).parentGUID.clear();
        UpdateSiblingSortIndices("", sceneName);
    }
}

// Reparent and insert at index. If index == -1 -> append to end. If newParentGUID empty and sceneName provided, place at scene root.
static void ReparentEntityWithIndex(const std::string& childGUID, const std::string& newParentGUID, int index = -1, const std::string& sceneName = "")
{
    Entity child = ecsInterface.GetEntityByGUID(childGUID);
    if (!ecsInterface.IsEntityValid(child)) return;

    // Special handling for non-scene entities
    if (newParentGUID.empty() && sceneName.empty())
    {
        // Check if this is a non-scene entity reorder
        std::string childScene = FindSceneNameForEntity(child);
        if (childScene.empty())
        {
            // Both child and target are non-scene entities
            // Just reorder using sortIndex
            if (index == -1)
            {
                auto nonSceneRoots = GetNonSceneRootEntities();
                index = (int)nonSceneRoots.size(); // Append to end
            }

            ReorderNonSceneEntity(childGUID, index);
            return;
        }
    }

    // Original logic for scene-based or parented entities
    RemoveChildFromParent(childGUID);

    if (index == -1)
        index = INT_MAX;

    InsertChildAt(newParentGUID, childGUID, index, sceneName);
}


void ReparentEntity(const std::string& childGUID, const std::string& newParentGUID)
{
    Entity child = ecsInterface.GetEntityByGUID(childGUID);
    Entity newParent = ecsInterface.GetEntityByGUID(newParentGUID);
    if (!ecsInterface.IsEntityValid(child) || !ecsInterface.IsEntityValid(newParent))
        return;

    auto& childHierarchy = ecsInterface.GetComponent<HierarchyComponent>(child);
    std::string oldParentGUID = childHierarchy.parentGUID;

    // Remove from old parent's children
    if (!oldParentGUID.empty())
    {
        Entity oldParent = ecsInterface.GetEntityByGUID(oldParentGUID);
        auto& oldChildren = ecsInterface.GetComponent<HierarchyComponent>(oldParent).childrenGUIDs;
        oldChildren.erase(std::remove(oldChildren.begin(), oldChildren.end(), childGUID), oldChildren.end());
    }

    // Assign new parent
    childHierarchy.parentGUID = newParentGUID;
    auto& newChildren = ecsInterface.GetComponent<HierarchyComponent>(newParent).childrenGUIDs;
    newChildren.push_back(childGUID);
}

// ---------- Scene Transfer Helpers ---------- //

// Recursively collect all entities in a hierarchy (entity + all descendants)
static void CollectHierarchy(const std::string& rootGUID, std::vector<Entity>& outEntities)
{
    Entity entity = ecsInterface.GetEntityByGUID(rootGUID);
    if (!ecsInterface.IsEntityValid(entity)) return;

    outEntities.push_back(entity);

    auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
    for (const std::string& childGUID : hierarchy.childrenGUIDs)
    {
        CollectHierarchy(childGUID, outEntities);
    }
}

static void RemoveEntityFromSceneTracking(Entity entity)
{
    // Search all scenes for this entity
    for (auto& [sceneName, entities] : Framework::sceneManager.GetSceneEntities())
    {
        auto it = std::find(entities.begin(), entities.end(), entity);
        if (it != entities.end())
        {
            entities.erase(it);
            return;
        }
    }
}

// Move entity (and all its children) to a different scene
// If targetSceneName is empty, moves to non-scene (orphan) status
static void MoveEntityToScene(const std::string& entityGUID, const std::string& targetSceneName)
{
    Entity entity = ecsInterface.GetEntityByGUID(entityGUID);
    if (!ecsInterface.IsEntityValid(entity)) return;

    // First, detach from parent if it has one
    auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
    std::string oldParentGUID = hierarchy.parentGUID;

    if (!oldParentGUID.empty())
    {
        Entity oldParent = ecsInterface.GetEntityByGUID(oldParentGUID);
        if (ecsInterface.IsEntityValid(oldParent))
        {
            auto& parentChildren = ecsInterface.GetComponent<HierarchyComponent>(oldParent).childrenGUIDs;
            parentChildren.erase(std::remove(parentChildren.begin(), parentChildren.end(), entityGUID),
                parentChildren.end());
            UpdateSiblingSortIndices(oldParentGUID, "");
        }
    }

    // Collect the entire hierarchy (entity + all descendants)
    std::vector<Entity> hierarchyEntities;
    CollectHierarchy(entityGUID, hierarchyEntities);

    // Remove all entities in hierarchy from their current scene tracking
    for (Entity e : hierarchyEntities)
    {
        RemoveEntityFromSceneTracking(e);
    }

    if (!targetSceneName.empty())
    {
        // Add to target scene - only add the root, children are tracked via HierarchyComponent
        auto& sceneEntities = Framework::sceneManager.GetSceneEntities()[targetSceneName];

        // Make it a root entity in the target scene
        hierarchy.parentGUID.clear();
        hierarchy.sortIndex = static_cast<int>(sceneEntities.size());

        sceneEntities.push_back(entity);

        // Update sortIndices for the target scene
        UpdateSiblingSortIndices("", targetSceneName);
    }
    else
    {
        // Moving to non-scene status
        hierarchy.parentGUID.clear();

        // Set sortIndex to append at end of non-scene entities
        auto nonSceneRoots = GetNonSceneRootEntities();
        hierarchy.sortIndex = static_cast<int>(nonSceneRoots.size());

        UpdateNonSceneSortIndices();
    }

    std::string entityName = ecsInterface.GetEntityName(entity);
    if (entityName.empty()) entityName = "Entity " + std::to_string(entity);

    std::cout << "[Hierarchy] Moved '" << entityName << "' and "
        << (hierarchyEntities.size() - 1) << " children "
        << (targetSceneName.empty() ? "to non-scene" : "to scene: " + targetSceneName)
        << std::endl;
}

// Check if dropping onto a scene header is allowed
static bool CanMoveToScene(const std::string& entityGUID, const std::string& targetSceneName)
{
    Entity entity = ecsInterface.GetEntityByGUID(entityGUID);
    if (!ecsInterface.IsEntityValid(entity)) return false;

    // Check if already in target scene
    std::string currentScene = FindSceneNameForEntity(entity);
    return currentScene != targetSceneName;
}



// Helper function to check if 'ancestor' is an ancestor of 'entity'
static bool IsAncestorOf(const std::string& ancestorGUID, const std::string& entityGUID)
{
    if (ancestorGUID == entityGUID) return true;

    Entity entity = ecsInterface.GetEntityByGUID(entityGUID);
    if (!ecsInterface.IsEntityValid(entity)) return false;

    auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
    std::string currentParent = hierarchy.parentGUID;

    while (!currentParent.empty())
    {
        if (currentParent == ancestorGUID) return true;

        Entity parentEntity = ecsInterface.GetEntityByGUID(currentParent);
        if (!ecsInterface.IsEntityValid(parentEntity)) break;

        currentParent = ecsInterface.GetComponent<HierarchyComponent>(parentEntity).parentGUID;
    }

    return false;
}

void DrawEntityHierarchy(const std::string& guid, Entity& selectedEntity, bool& isPropertiesWindowOpen)
{
    Entity entity = ecsInterface.GetEntityByGUID(guid);
    if (!ecsInterface.IsEntityValid(entity)) return;

    auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
    std::string entityName = ecsInterface.GetEntityName(entity);
    if (entityName.empty()) entityName = "Entity " + std::to_string(entity);
    std::string label = entityName + "##" + guid;

    bool hasChildren = !hierarchy.childrenGUIDs.empty();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (selectedEntity == entity)
        flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    // Click to select
    if (ImGui::IsItemClicked())
    {
        selectedEntity = entity;
        isPropertiesWindowOpen = true;
    }

    // Begin drag
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload("ENTITY_GUID", guid.c_str(), guid.size() + 1);
        ImGui::Text("Moving: %s", entityName.c_str());
        ImGui::EndDragDropSource();
    }

    // Handle drop target
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_GUID"))
        {
            const char* data = (const char*)payload->Data;
            std::string childGUID(data);

            // Prevent dragging entity onto itself or its descendants (cycle prevention)
            if (childGUID != guid && !IsAncestorOf(childGUID, guid))
            {
                ImVec2 min = ImGui::GetItemRectMin();
                ImVec2 max = ImGui::GetItemRectMax();
                float itemHeight = max.y - min.y;
                ImVec2 mouse = ImGui::GetIO().MousePos;

                // Calculate drop zone (normalized 0-1)
                float relativeY = (mouse.y - min.y) / itemHeight;

                // Adjust these thresholds to control drop zone sizes
                const float INSERT_BEFORE_THRESHOLD = 0.33f;  // Top 33%
                const float INSERT_AFTER_THRESHOLD = 0.67f;   // Bottom 33%

                if (relativeY < INSERT_BEFORE_THRESHOLD)
                {
                    // TOP ZONE: Insert before target
                    std::string parentOfTarget = hierarchy.parentGUID;

                    if (!parentOfTarget.empty())
                    {
                        // Target has a parent - insert as sibling before target
                        Entity parentEntity = ecsInterface.GetEntityByGUID(parentOfTarget);
                        if (ecsInterface.IsEntityValid(parentEntity))
                        {
                            auto& siblings = ecsInterface.GetComponent<HierarchyComponent>(parentEntity).childrenGUIDs;
                            auto it = std::find(siblings.begin(), siblings.end(), guid);
                            if (it != siblings.end())
                            {
                                int targetIdx = static_cast<int>(std::distance(siblings.begin(), it));

                                // Check if dragged child is before target in same parent
                                Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
                                auto& childHierarchy = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
                                int adjustedIdx = targetIdx;

                                if (childHierarchy.parentGUID == parentOfTarget)
                                {
                                    auto childIt = std::find(siblings.begin(), siblings.end(), childGUID);
                                    if (childIt != siblings.end())
                                    {
                                        int childIdx = static_cast<int>(std::distance(siblings.begin(), childIt));
                                        if (childIdx < targetIdx)
                                        {
                                            adjustedIdx = targetIdx - 1;
                                        }
                                    }
                                }

                                ReparentEntityWithIndex(childGUID, parentOfTarget, adjustedIdx);
                            }
                        }
                    }
                    else
                    {
                        // Target is at scene root or non-scene root - insert before target
                        std::string sceneName = FindSceneNameForEntity(entity);

                        if (!sceneName.empty())
                        {
                            // Scene entity
                            auto& sceneVec = Framework::sceneManager.GetSceneEntities().at(sceneName);
                            auto targetIt = std::find(sceneVec.begin(), sceneVec.end(), entity);
                            if (targetIt != sceneVec.end())
                            {
                                int targetIdx = static_cast<int>(std::distance(sceneVec.begin(), targetIt));

                                Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
                                std::string childScene = FindSceneNameForEntity(childEntity);
                                int adjustedIdx = targetIdx;

                                if (childScene == sceneName)
                                {
                                    auto childIt = std::find(sceneVec.begin(), sceneVec.end(), childEntity);
                                    if (childIt != sceneVec.end())
                                    {
                                        int childIdx = static_cast<int>(std::distance(sceneVec.begin(), childIt));
                                        if (childIdx < targetIdx)
                                        {
                                            adjustedIdx = targetIdx - 1;
                                        }
                                    }
                                }

                                ReparentEntityWithIndex(childGUID, "", adjustedIdx, sceneName);
                            }
                        }
                        else
                        {
                            // Non-scene entity - use sortIndex-based reordering
                            int targetIdx = hierarchy.sortIndex;

                            Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
                            std::string childScene = FindSceneNameForEntity(childEntity);

                            // Only allow reordering within non-scene entities
                            if (childScene.empty())
                            {
                                auto& childHierarchy = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
                                int adjustedIdx = targetIdx;

                                if (childHierarchy.sortIndex < targetIdx)
                                {
                                    adjustedIdx = targetIdx - 1;
                                }

                                ReparentEntityWithIndex(childGUID, "", adjustedIdx, "");
                            }
                        }
                    }
                }
                else if (relativeY > INSERT_AFTER_THRESHOLD)
                {
                    // BOTTOM ZONE: Insert after target
                    std::string parentOfTarget = hierarchy.parentGUID;

                    if (!parentOfTarget.empty())
                    {
                        // Target has a parent - insert as sibling after target
                        Entity parentEntity = ecsInterface.GetEntityByGUID(parentOfTarget);
                        if (ecsInterface.IsEntityValid(parentEntity))
                        {
                            auto& siblings = ecsInterface.GetComponent<HierarchyComponent>(parentEntity).childrenGUIDs;
                            auto it = std::find(siblings.begin(), siblings.end(), guid);
                            if (it != siblings.end())
                            {
                                int targetIdx = static_cast<int>(std::distance(siblings.begin(), it));

                                // Check if dragged child is before target in same parent
                                Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
                                auto& childHierarchy = ecsInterface.GetComponent<HierarchyComponent>(childEntity);
                                int adjustedIdx = targetIdx + 1;

                                if (childHierarchy.parentGUID == parentOfTarget)
                                {
                                    auto childIt = std::find(siblings.begin(), siblings.end(), childGUID);
                                    if (childIt != siblings.end())
                                    {
                                        int childIdx = static_cast<int>(std::distance(siblings.begin(), childIt));
                                        if (childIdx < targetIdx)
                                        {
                                            adjustedIdx = targetIdx; // No +1 needed since we removed one before target
                                        }
                                    }
                                }

                                ReparentEntityWithIndex(childGUID, parentOfTarget, adjustedIdx);
                            }
                        }
                    }
                    else
                    {
                        // Target is at scene root - insert after target at root level
                        std::string sceneName = FindSceneNameForEntity(entity);
                        if (!sceneName.empty())
                        {
                            auto& sceneVec = Framework::sceneManager.GetSceneEntities().at(sceneName);
                            auto targetIt = std::find(sceneVec.begin(), sceneVec.end(), entity);
                            if (targetIt != sceneVec.end())
                            {
                                int targetIdx = static_cast<int>(std::distance(sceneVec.begin(), targetIt));

                                // Check if dragged entity is before target in same scene
                                Entity childEntity = ecsInterface.GetEntityByGUID(childGUID);
                                std::string childScene = FindSceneNameForEntity(childEntity);
                                int adjustedIdx = targetIdx + 1;

                                if (childScene == sceneName)
                                {
                                    auto childIt = std::find(sceneVec.begin(), sceneVec.end(), childEntity);
                                    if (childIt != sceneVec.end())
                                    {
                                        int childIdx = static_cast<int>(std::distance(sceneVec.begin(), childIt));
                                        if (childIdx < targetIdx)
                                        {
                                            adjustedIdx = targetIdx; // No +1 needed since we removed one before target
                                        }
                                    }
                                }

                                ReparentEntityWithIndex(childGUID, "", adjustedIdx, sceneName);
                            }
                        }
                    }
                }
                else
                {
                    // MIDDLE ZONE: Make child of target
                    ReparentEntityWithIndex(childGUID, guid, -1);
                }
            }
        }

        ImGui::EndDragDropTarget();
    }

    // Draw visual feedback for drop zones while dragging
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
        {
            if (payload->IsDataType("ENTITY_GUID"))
            {
                const char* data = (const char*)payload->Data;
                std::string draggedGUID(data);

                if (draggedGUID != guid && !IsAncestorOf(draggedGUID, guid))
                {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    float itemHeight = max.y - min.y;
                    ImVec2 mouse = ImGui::GetIO().MousePos;
                    float relativeY = (mouse.y - min.y) / itemHeight;

                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImU32 lineColor = IM_COL32(255, 255, 0, 255); // Yellow indicator

                    const float INSERT_BEFORE_THRESHOLD = 0.33f;
                    const float INSERT_AFTER_THRESHOLD = 0.67f;

                    if (relativeY < INSERT_BEFORE_THRESHOLD)
                    {
                        // Draw line above
                        drawList->AddLine(ImVec2(min.x, min.y), ImVec2(max.x, min.y), lineColor, 2.0f);
                    }
                    else if (relativeY > INSERT_AFTER_THRESHOLD)
                    {
                        // Draw line below
                        drawList->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), lineColor, 2.0f);
                    }
                    else
                    {
                        // Highlight entire item
                        drawList->AddRect(min, max, IM_COL32(255, 255, 0, 100), 0.0f, 0, 2.0f);
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Recursively draw children, sorted by sortIndex
    if (hasChildren && open)
    {
        std::vector<std::string> sortedChildren = hierarchy.childrenGUIDs;
        std::sort(sortedChildren.begin(), sortedChildren.end(), [](const std::string& a, const std::string& b) {
            Entity ea = ecsInterface.GetEntityByGUID(a);
            Entity eb = ecsInterface.GetEntityByGUID(b);
            if (!ecsInterface.IsEntityValid(ea) || !ecsInterface.IsEntityValid(eb)) return false;
            return ecsInterface.GetComponent<HierarchyComponent>(ea).sortIndex <
                ecsInterface.GetComponent<HierarchyComponent>(eb).sortIndex;
            });

        for (const std::string& childGUID : sortedChildren)
        {
            DrawEntityHierarchy(childGUID, selectedEntity, isPropertiesWindowOpen);
        }

        if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen))
            ImGui::TreePop();
    }
}
// --- Hierarchy Helper End ----- //