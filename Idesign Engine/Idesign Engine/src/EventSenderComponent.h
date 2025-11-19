#pragma once
#include <string>
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <string>  
#include <vector> 
#include <cmath>   

namespace Framework {
    struct EventSenderComponent {
        std::string triggerName;

        // The message to broadcast when that trigger happens
        // e.g., "ShowPrompt"
        std::string messageName;
    };

    inline const std::vector g_EventSenderPropsArray =
    {
        PROP_STRING(triggerName, EventSenderComponent),
        PROP_STRING(messageName, EventSenderComponent)
    };

    inline const prop_list g_EventSenderProps{"EventSenderComponent", g_EventSenderPropsArray };

    inline global_registration g_EventSenderPropsReg(&g_EventSenderProps);
}