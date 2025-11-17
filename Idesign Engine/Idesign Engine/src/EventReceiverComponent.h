#pragma once

#include <string>
#include "Coordinator.h"
#include "MessageSystem.h"
#include "GameMessages.h"
#include "ScriptingSystem.h" // Include your Lua component header
#include "PropertySystem.h"
#include "PropertyMacro.h"

// Include your TypeRegistry/Property system headers here
// e.g., #include "TypeRegistry.h"
// e.g., #include "PropertyList.h"

namespace Framework {

    /**
     * @brief A data-driven C++ to Lua bridge.
     * Listens for a C++ message and calls a Lua function in response.
     */
    struct EventReceiverComponent {
        // --- Data (Set by your editor/deserializer) ---
        std::string messageToListenFor; // e.g., "PlayerInteractedWithItem"
        std::string onReceiveFunction;  // e.g., "AddItemToInventory"

        // --- Internal State (Set at initialization) ---
        Coordinator* mCoordinator = nullptr;
        Entity mEntity = 0;

        /**
         * @brief Call this after the component's data has been loaded.
         * This function subscribes to the C++ MessageBus.
         */
        void Initialize() {
            if (messageToListenFor == "PlayerInteractedWithItem") {
                MessageBus::Get().SubscribeDeferred<PlayerInteractedWithItem>(
                    [this](const PlayerInteractedWithItem& msg) {

                        // 1. Get the ScriptingSystem from the coordinator
                        // We must get the system, as it's the only
                        // part of your engine that can talk to Lua.
                        auto scriptingSystem = mCoordinator->GetSystem<ScriptingSystem>();

                        if (scriptingSystem) {
                            // 2. Call the new public function
                            scriptingSystem->CallEntityFunction(
                                mEntity,           // The entity to call the function on (the Player)
                                onReceiveFunction, // The string name ("AddItemToInventory")
                                msg.interactor,    // The first argument
                                msg.interactee     // The second argument
                            );
                        }
                    }
                );
            }
            // else if (messageToListenFor == "AnotherMessage") { ... }
        }
    };

    // --- REGISTRATION FOR IMGUI ---
    inline const std::vector g_EventReceiverPropsArray =
    {
        // Assuming you have a PROP_STRING macro
        PROP_STRING(messageToListenFor, EventReceiverComponent),
        PROP_STRING(onReceiveFunction, EventReceiverComponent)
    };

    inline const prop_list g_EventReceiverProps{ "EventReceiverComponent", g_EventReceiverPropsArray };

    inline global_registration g_EventReceiverPropsReg(&g_EventReceiverProps);
    // --- END REGISTRATION ---

} // namespace Framework