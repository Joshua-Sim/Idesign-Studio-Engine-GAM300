#pragma once
#include <string>
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <string>  
#include <vector> 
#include <cmath>   

namespace Framework {
    enum EEventTriggerType : int {
        OnInteract,
        OnCollisionEnter,
        OnDamage,
        OnLeftClick  
    };

    // 2. The names array stays the same
    inline const char* kEventTriggerTypeNames[] = {
        "OnInteract",
        "OnCollisionEnter",
        "OnDamage",
        "OnLeftClick"
    };

    // 3. THIS IS THE FIX:
    struct EventSenderComponent {
        int onTrigger = EEventTriggerType::OnInteract;

        std::string messageName;
    };

    // --- REGISTRATION FOR IMGUI ---
    // This PROP_INT is now 100% correct
    inline const std::vector g_EventSenderPropsArray =
    {
        PROP_INT(onTrigger, EventSenderComponent),
        PROP_STRING(messageName, EventSenderComponent)
    };

    inline const prop_list g_EventSenderProps{"EventSenderComponent", g_EventSenderPropsArray };

    inline global_registration g_EventSenderPropsReg(&g_EventSenderProps);
}