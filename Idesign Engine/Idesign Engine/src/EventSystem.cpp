#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include <EventReceiverComponent.h> // Ensure this path is correct

extern Framework::Coordinator ecsInterface;

namespace Framework {

    void EventSystem::Initialize() {
        // Leave this empty now. 
        // We cannot trust that entities exist during Initialize.
    }

    void EventSystem::Update(float timeslice) {
        // 1. Iterate over all entities that this system cares about
        for (auto const& entity : mEntities) {

            // Safety check
            if (!ecsInterface.HasComponent<EventReceiverComponent>(entity)) {
                continue;
            }

            auto& receiver = ecsInterface.GetComponent<EventReceiverComponent>(entity);

            // 2. CHECK THE FLAG
            // Only wire up if we haven't done so already
            if (!receiver.isSubscribed) {

                // 3. Subscribe
                EventManager::Instance().Subscribe(receiver.messageToListenFor,
                    [entity, this](void* data) { // Capture 'this' to access ecsInterface if needed, or capture 'scriptingSystem' if you get it earlier

                        // Re-get component inside lambda to be safe, or capture necessary strings by value
                        // Note: Capturing 'entity' by value is safe. 

                        // Use 'ecsInterface' (global) to get the component to ensure we have fresh data
                        if (!ecsInterface.IsEntityValid(entity)) return;
                        auto& r = ecsInterface.GetComponent<EventReceiverComponent>(entity);

                        std::cout << "[EventSystem] RECEIVED Event: " << r.messageToListenFor
                            << " on Entity: " << entity << std::endl;

                        auto scriptingSystem = ecsInterface.GetSystem<ScriptingSystem>();
                        if (scriptingSystem) {
                            Entity* otherEntity = static_cast<Entity*>(data);

                            // Use 'r.luaFunctionToCall' from the fresh component ref
                            scriptingSystem->CallEntityFunction(
                                entity,
                                r.luaFunctionToCall,
                                (otherEntity ? *otherEntity : -1)
                            );
                        }
                    }
                );

                // 4. MARK AS DONE
                // Crucial: Prevent re-subscribing next frame
                receiver.isSubscribed = true;

                std::cout << "[EventSystem] Wired up entity " << entity << " to listener " << receiver.messageToListenFor << "\n";
            }
        }
    }
}