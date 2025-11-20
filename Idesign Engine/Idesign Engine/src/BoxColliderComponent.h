#pragma once

#include "Coordinator.h"
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <glm/glm/glm.hpp>
#include <vector>


namespace Framework {

    // 1. The component struct
    struct BoxColliderComponent {
        glm::vec3 size{ 1.0f, 1.0f, 1.0f };
        glm::vec3 center{ 0.0f, 0.0f, 0.0f };
        bool wasInsideLastFrame = false;
        bool canPassThrough = false; // New field
    };


    // 2. --- REGISTRATION FOR IMGUI ---
    // (This assumes you have a PROP_VEC3 macro in PropertyMacro.h)

    inline const std::vector g_BoxColliderPropsArray =
    {
        PROP_VEC3(size, BoxColliderComponent),
        PROP_VEC3(center, BoxColliderComponent),
        PROP_BOOL(canPassThrough, BoxColliderComponent) // <--- NEW
    };

    inline const prop_list g_BoxColliderProps{ "BoxColliderComponent", g_BoxColliderPropsArray };

    inline global_registration g_BoxColliderPropsReg(&g_BoxColliderProps);
}