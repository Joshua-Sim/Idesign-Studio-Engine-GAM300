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

namespace Framework
{
    struct MeshProperties
    {
        std::string guid;
        std::string path = "Assets/Objects/plane2.obj"; // asset path to load and fill in mesh component vars, alloted default
        unsigned int vao = 0, vbo = 0, ebo = 0;
        unsigned int indexCount = 0;
    };

    inline constexpr std::array g_MeshPropsArray =
    {
        PROP_STRING(guid, MeshProperties),
        PROP_STRING(path, MeshProperties),
        PROP_UINT(vao, MeshProperties),
        PROP_UINT(vbo, MeshProperties),
        PROP_UINT(ebo, MeshProperties),
        PROP_UINT(indexCount, MeshProperties)
    };

    inline constexpr prop_list g_MeshProps{ "MeshProperties", g_MeshPropsArray };
    inline global_registration g_MeshPropsReg(&g_MeshProps);
}
#endif // !_MESH_PROPERTIES_H_