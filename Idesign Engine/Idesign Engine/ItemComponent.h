#pragma once
#ifndef _ITEM_COMPONENT_H_
#define _ITEM_COMPONENT_H_

#include "Coordinator.h"
#include <string>
#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    inline const char* kItemNames[] = { "unchopped_herb", "chopped_herb", "pear"};
    inline constexpr int kItemCount = sizeof(kItemNames) / sizeof(kItemNames[0]);

    inline std::string MakeItemIconPath(int itemId) {
        if (itemId < 0 || itemId >= kItemCount) return {};
        return std::string("Assets/Textures/") + kItemNames[itemId] + ".png";
    }

    inline int ItemNameToId(const std::string& name) {
        for (int i = 0; i < kItemCount; ++i) {
            if (name == kItemNames[i]) return i;
        }
        return -1;
    }

    struct ItemComponent
    {
        std::string name;
        //float pickupRadius = 20.0f;
        // Logic flags
        bool isIngredient = false;
        bool isChoppable = false;
        bool toDestroyWhenPicked = false;
    };

    inline const std::vector g_ItemPropsArray =
    {
        PROP_STRING(name,         ItemComponent),
        //PROP_FLOAT(pickupRadius, ItemComponent),
        PROP_BOOL(isIngredient,   ItemComponent),
        PROP_BOOL(isChoppable,    ItemComponent),
        PROP_BOOL(toDestroyWhenPicked,    ItemComponent)
    };

    inline const prop_list g_ItemProps{ "ItemComponent", g_ItemPropsArray };
    inline global_registration g_ItemPropsReg(&g_ItemProps);

} // namespace Framework

#endif // _ITEM_COMPONENT_H_