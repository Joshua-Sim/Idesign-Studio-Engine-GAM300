#pragma once
#ifndef _BOX_COLLIDER_COMPONENT_H_
#define _BOX_COLLIDER_COMPONENT_H_

#include "Coordinator.h"
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <glm/glm/glm.hpp>
#include <vector>

namespace Framework {

    // 1. Single Collider Data
    struct BoxColliderEntry {
        glm::vec3 size{ 1.0f, 1.0f, 1.0f };
        glm::vec3 center{ 0.0f, 0.0f, 0.0f }; // Offset from Entity Transform
        bool canPassThrough = false;

        // Runtime State (Per-collider tracking)
        bool wasInsideLastFrame = false;
    };

    // Register Entry Properties
    inline const std::vector g_BoxColliderEntryPropsArray =
    {
        PROP_VEC3(size, BoxColliderEntry),
        PROP_VEC3(center, BoxColliderEntry),
        PROP_BOOL(canPassThrough, BoxColliderEntry),
        // Hide runtime state from Editor/Serialization
        PROP_BOOL_HIDDEN(wasInsideLastFrame, BoxColliderEntry)
    };

    inline const prop_list g_BoxColliderEntryProps{ "BoxColliderEntry", g_BoxColliderEntryPropsArray };
    inline global_registration g_BoxColliderEntryPropsReg(&g_BoxColliderEntryProps);


    // 2. The Component (The Container)
    struct BoxColliderComponent {
        std::vector<BoxColliderEntry> colliders;

        // Hidden helper for reflection
        int colliderCount = 0;
    };

    struct CollisionEventData {
        Entity senderID;
        Entity targetID;
        int actionType; // 1 = Enter, 0 = Exit
    };

    // Register Component Properties
    inline const std::vector g_BoxColliderPropsArray =
    {
        PROP_INT_HIDDEN(colliderCount, BoxColliderComponent)
    };

    inline const prop_list g_BoxColliderProps{ "BoxColliderComponent", g_BoxColliderPropsArray };
    inline global_registration g_BoxColliderPropsReg(&g_BoxColliderProps);
}

#endif // _BOX_COLLIDER_COMPONENT_H_