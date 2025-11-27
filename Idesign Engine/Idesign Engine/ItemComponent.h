#pragma once
#ifndef _ITEM_COMPONENT_H_
#define _ITEM_COMPONENT_H_

#include "Coordinator.h"
#include <string>
#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework
{
    // 1. Define Items
    inline const char* kItemNames[] = { "unchopped_herb", "chopped_herb", "pear", "strength_potion", "vitality_potion"};
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

    // 2. Define Recipe Structures Here (Global Access)
    struct PotionRecipe {
        std::vector<int> ingredientIDs; // Store IDs, not strings, for faster comparison
        int resultItemID;
    };

    // 3. The Global Recipe List
    // We use a function to ensure it initializes correctly when called
    inline const std::vector<PotionRecipe>& GetGlobalRecipes() {
        static std::vector<PotionRecipe> recipes;
        if (recipes.empty()) {
            // --- RECIPE 1: Strength Potion ---
            // Requires: unchopped_herb (0) + chopped_herb (1)
            recipes.push_back({ {0, 2}, 3 });

            // --- RECIPE 2: Example ---
            recipes.push_back({ {1, 2}, 4 }); 
        }
        return recipes;
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