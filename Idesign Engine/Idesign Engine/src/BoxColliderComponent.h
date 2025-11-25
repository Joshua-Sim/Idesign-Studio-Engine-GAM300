#pragma once
#ifndef _BOX_COLLIDER_COMPONENT_H_
#define _BOX_COLLIDER_COMPONENT_H_

#include "Coordinator.h"
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <glm/glm/glm.hpp>
#include <vector>
#include <string> // Added

namespace Framework {

    struct CollisionEventData {
        Entity senderID;
        Entity targetID;
        int actionType; // 1 = Enter, 0 = Exit
    };

    // 1. Single Collider Data
    struct BoxColliderEntry {
        glm::vec3 size{ 1.0f, 1.0f, 1.0f };
        glm::vec3 center{ 0.0f, 0.0f, 0.0f };
        bool canPassThrough = false;

        // --- NEW INTERACTION FIELDS ---
        bool isInteractable = false;       // If true, raycast can trigger event
        std::string interactMessage;       // e.g. "OnPickUp" or "OpenChest"

        // Runtime State
        bool wasInsideLastFrame = false;
    };

    // Register Entry Properties
    inline const std::vector g_BoxColliderEntryPropsArray =
    {
        PROP_VEC3(size, BoxColliderEntry),
        PROP_VEC3(center, BoxColliderEntry),
        PROP_BOOL(canPassThrough, BoxColliderEntry),
        PROP_BOOL(isInteractable, BoxColliderEntry),     // NEW
        PROP_STRING(interactMessage, BoxColliderEntry),  // NEW
        PROP_BOOL_HIDDEN(wasInsideLastFrame, BoxColliderEntry)
    };

    inline const prop_list g_BoxColliderEntryProps{ "BoxColliderEntry", g_BoxColliderEntryPropsArray };
    inline global_registration g_BoxColliderEntryPropsReg(&g_BoxColliderEntryProps);


    // 2. The Component
    struct BoxColliderComponent {
        std::vector<BoxColliderEntry> colliders;
        int colliderCount = 0;
    };

    inline const std::vector g_BoxColliderPropsArray =
    {
        PROP_INT_HIDDEN(colliderCount, BoxColliderComponent)
    };

    inline const prop_list g_BoxColliderProps{ "BoxColliderComponent", g_BoxColliderPropsArray };
    inline global_registration g_BoxColliderPropsReg(&g_BoxColliderProps);
}

#endif // _BOX_COLLIDER_COMPONENT_H_