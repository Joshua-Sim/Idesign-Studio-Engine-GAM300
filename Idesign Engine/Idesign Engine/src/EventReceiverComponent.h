#pragma once

#include <string>
#include <vector>
#include "PropertySystem.h"
#include "PropertyMacro.h"

namespace Framework {

    /**
     * @brief Just Data. The EventSystem reads this to know what to wire up.
     */
    struct EventReceiverComponent {
        // --- Data (Set by your editor) ---

        // The "Channel" to listen to (e.g., "ShowPrompt", "OnCollision")
        std::string messageToListenFor;

        // The Lua function to run when that channel fires (e.g., "Lua_ShowPrompt")
        std::string luaFunctionToCall;

        bool isSubscribed = false;
    };

    // --- REGISTRATION FOR IMGUI ---
    inline const std::vector g_EventReceiverPropsArray =
    {
        PROP_STRING(messageToListenFor, EventReceiverComponent),
        // Note: I renamed 'onReceiveFunction' to 'luaFunctionToCall' to match the System code I gave you.
        PROP_STRING(luaFunctionToCall, EventReceiverComponent)
    };

    inline const prop_list g_EventReceiverProps{ "EventReceiverComponent", g_EventReceiverPropsArray };

    inline global_registration g_EventReceiverPropsReg(&g_EventReceiverProps);
    // --- END REGISTRATION ---

} // namespace Framework