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

    inline constexpr std::array g_PrefabPropsArray
    {
        prop{ "name", prop::STRING,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<Prefab*>(pObject)->name = *static_cast<const std::string*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<std::string*>(pData) = static_cast<const Prefab*>(pObject)->name; } },

        prop{ "entityCount", prop::INT,
            [](void* pObject, const void* pData) { assert(pObject && pData); static_cast<Prefab*>(pObject)->entityCount = *static_cast<const int*>(pData); },
            [](const void* pObject, void* pData) { assert(pObject && pData); *static_cast<int*>(pData) = static_cast<const Prefab*>(pObject)->entityCount; } },
    };

    inline constexpr prop_list g_PrefabProps{ "Prefab", g_PrefabPropsArray };
    inline global_registration g_PrefabPropsReg(&g_PrefabProps);
}
#endif // !_PREFAB_PROPERTIES_H_
