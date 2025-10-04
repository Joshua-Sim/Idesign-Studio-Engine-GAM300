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
        //debug rendering 
        DebugShapeType debugShape = DebugShapeType::None;
        glm::vec3 debugcolor{ 1.f,1.f,0.0f };
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
    };

    inline constexpr prop_list g_MeshProps{ "MeshProperties", g_MeshPropsArray };
    inline global_registration g_MeshPropsReg(&g_MeshProps);
}
#endif // !_MESH_PROPERTIES_H_