#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include <EventReceiverComponent.h>

extern Framework::Coordinator ecsInterface;

namespace Framework {

    // 1. FIX: Renamed to Initialize
    void EventSystem::Initialize() {
        // 1. Get all entities that the Coordinator knows have an EventReceiver
        for (auto const& entity : mEntities) {
            // ADD THIS SAFETY CHECK
            if (!ecsInterface.HasComponent<EventReceiverComponent>(entity)) {
                continue;
            }

            auto& receiver = ecsInterface.GetComponent<EventReceiverComponent>(entity);

            // 2. The Wiring
            // We tell the Manager: "When [messageToListenFor] happens, run this lambda."
            EventManager::Instance().Subscribe(receiver.messageToListenFor,



                // THE LAMBDA (The actual logic that runs later)
                [entity, receiver](void* data) { // Capture entity by value, receiver by copy (safety)


                    std::cout << "[EventSystem] RECEIVED Event: " << receiver.messageToListenFor
                        << " on Entity: " << entity << std::endl;

                    auto scriptingSystem = ecsInterface.GetSystem<ScriptingSystem>();

                    if (scriptingSystem) {
                        // B. Decode the payload
                        // In your diagram, the Sender passes the "other" entity (e.g., who hit the box)
                        Entity* otherEntity = static_cast<Entity*>(data);

                        // C. Call Lua
                        // "Entity [ID] runs function [Lua_ShowPrompt]"
                        // We pass -1 (or 0) as the 4th argument because the function signature requires it
                        scriptingSystem->CallEntityFunction(
                            entity,
                            receiver.luaFunctionToCall,
                            (otherEntity ? *otherEntity : -1)
                        );
                    }
                }
            );
        }
    }


    // 2. FIX: Empty body for Update
    void EventSystem::Update(float timeslice) {
        return;
    }
}
