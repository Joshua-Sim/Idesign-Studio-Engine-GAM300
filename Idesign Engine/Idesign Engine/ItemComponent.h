#pragma once
#ifndef _ITEM_COMPONENT_H_
#define _ITEM_COMPONENT_H_

#include "Coordinator.h"
#include <string>
#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    struct ItemComponent
    {
        std::string name;

        // Logic flags
        bool isIngredient = false;
        bool isChoppable = false;
    };

    inline const std::vector g_ItemPropsArray =
    {
        PROP_STRING(name,         ItemComponent),
        PROP_BOOL(isIngredient,   ItemComponent),
        PROP_BOOL(isChoppable,    ItemComponent)
    };

    inline const prop_list g_ItemProps{ "ItemComponent", g_ItemPropsArray };
    inline global_registration g_ItemPropsReg(&g_ItemProps);

} // namespace Framework

#endif // _ITEM_COMPONENT_H_