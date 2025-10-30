///////////////////////////////////////////////////////////////////////////////
///
///	@file  : MeshProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _MESH_PROPERTIES_H_
#define _MESH_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <glm.hpp>
namespace Framework
{
    enum class DebugShapeType
    {
        None,
        Box,
        Sphere,
        Triangle
    };
    struct MeshProperties
    {
        std::string guid;
        std::string name;
        std::string path; // asset path to load and fill in mesh component vars, alloted default
        unsigned int vao = 0, vbo = 0, ebo = 0;
        unsigned int indexCount = 0;
        bool isDirty = false;

        // Handle for the VBO containing VertexSkinning data
        unsigned int vbo_skinning = 0;

        //debug rendering 
        DebugShapeType debugShape = DebugShapeType::None;
        glm::vec3 debugcolor{ 1.f,1.f,0.0f };

        // --- ADDED MATERIAL PROPERTIES ---
        glm::vec3 ambient{ 0.1f, 0.1f, 0.1f };
        glm::vec3 albedo{ 1.0f, 1.0f, 1.0f };    // This will be the tint for your texture
        glm::vec3 specular{ 1.0f, 1.0f, 1.0f };   // Bright white specular for metals/plastics
        float shininess = 32.0f;                // Common default shininess
        float opacity=1.0f;
    };

    inline constexpr std::array g_MeshPropsArray =
    {
        PROP_STRING(guid, MeshProperties),
        PROP_STRING(name, MeshProperties),
        PROP_STRING(path, MeshProperties),
        PROP_UINT(vao, MeshProperties),
        PROP_UINT(vbo, MeshProperties),
        PROP_UINT(ebo, MeshProperties),
        PROP_UINT(indexCount, MeshProperties),
        PROP_VEC3(ambient,MeshProperties),
        PROP_VEC3(albedo,MeshProperties),
        PROP_VEC3(specular,MeshProperties),
        PROP_FLOAT(shininess,MeshProperties),
        PROP_FLOAT(opacity,MeshProperties)


    };

    inline constexpr prop_list g_MeshProps{ "MeshProperties", g_MeshPropsArray };
    inline global_registration g_MeshPropsReg(&g_MeshProps);
}
#endif // !_MESH_PROPERTIES_H_