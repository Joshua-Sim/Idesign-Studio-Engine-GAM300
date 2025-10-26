///////////////////////////////////////////////////////////////////////////////
///
///	@file  : PrefabProperties.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _PREFAB_PROPERTIES_H_
#define _PREFAB_PROPERTIES_H_

#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct Prefab
    {
        std::string name;
        int entityCount;
    };

    inline const std::vector g_PrefabPropsArray
    {
        PROP_STRING(name, Prefab),
        PROP_INT(entityCount, Prefab)
    };

    inline const prop_list g_PrefabProps{ "Prefab", g_PrefabPropsArray };
    inline global_registration g_PrefabPropsReg(&g_PrefabProps);
}
#endif // !_PREFAB_PROPERTIES_H_
