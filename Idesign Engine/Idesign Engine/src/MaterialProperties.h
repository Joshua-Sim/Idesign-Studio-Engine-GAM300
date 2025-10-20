#pragma once
#ifndef _MATERIAL_PROPERTIES_H_
#define _MATERIAL_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <string>
#include <glm.hpp> 
#include <array>   
namespace Framework
{
    //different types of material availble
    struct MaterialComponent
    {
        // GUID of the MaterialProperties asset
        std::string materialGuid;
    };

    inline constexpr std::array g_MaterialComponentPropsArray =
    {
        PROP_STRING(materialGuid, MaterialComponent),
    };

    inline constexpr prop_list g_MaterialComponentProps{ "MaterialComponent", g_MaterialComponentPropsArray };
    inline global_registration g_MaterialComponentReg(&g_MaterialComponentProps);


    // Store a single GUID of the material
    struct MaterialProperties
    {
        std::string guid;
        std::string name = "DefaultMaterial";
        bool isDirty = false;

        // --- Albedo (Color) ---
        glm::vec3 albedoColor = { 1.0f, 1.0f, 1.0f }; // Constant tint
        std::string albedoMapGuid;                    // Texture map

        // --- Metallic (Shine) ---
        float metallic = 0.0f;          // Constant value (0.0 = non-metal, 1.0 = metal)
        std::string metallicMapGuid;  // Texture map

        // --- Roughness (Shine) ---
        float roughness = 1.0f;         // Constant value (0.0 = smooth, 1.0 = rough)
        std::string roughnessMapGuid; // Texture map
    };

    // --- Registration block for the Asset ---
    inline constexpr std::array g_MaterialPropertiesPropsArray =
    {
        PROP_STRING(guid, MaterialProperties),
        PROP_STRING(name, MaterialProperties),

        PROP_VEC3(albedoColor, MaterialProperties),
        PROP_STRING(albedoMapGuid, MaterialProperties),

        PROP_FLOAT(metallic, MaterialProperties),
        PROP_STRING(metallicMapGuid, MaterialProperties),

        PROP_FLOAT(roughness, MaterialProperties),
        PROP_STRING(roughnessMapGuid, MaterialProperties),
    };

    inline constexpr prop_list g_MaterialPropertiesProps{ "MaterialProperties", g_MaterialPropertiesPropsArray };
    inline global_registration g_MaterialPropertiesReg(&g_MaterialPropertiesProps);
}
#endif // !_MATERIAL_PROPERTIES_H_