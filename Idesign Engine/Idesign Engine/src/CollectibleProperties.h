#pragma once
#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework {

    inline const char* kItemNames[] = { "unchopped_herb", "chopped_herb" };
    inline constexpr int kItemCount = sizeof(kItemNames) / sizeof(kItemNames[0]);

    inline std::string MakeItemIconPath(int itemId) {
        if (itemId < 0 || itemId >= kItemCount) return {};
        return std::string("Assets/Textures/") + kItemNames[itemId] + ".png";
    }

    struct CollectibleProperties
    {
        int   itemId = 0;
        int   amount = 1;
        float pickupRadius = 20.0f;
        bool  isIngredient = false;
        bool  isChoppable = false;
    };

    // Reflect exactly like PhysicsProperties
    inline const std::vector g_CollectiblePropsArray = {
        PROP_INT(amount, CollectibleProperties),
        PROP_FLOAT(pickupRadius, CollectibleProperties),
        PROP_BOOL(isIngredient, CollectibleProperties),
        PROP_BOOL(isChoppable, CollectibleProperties),
    };

    inline const prop_list g_CollectibleProps{ "CollectibleProperties", g_CollectiblePropsArray };
    inline global_registration g_CollectiblePropsReg(&g_CollectibleProps);

} // namespace Framework