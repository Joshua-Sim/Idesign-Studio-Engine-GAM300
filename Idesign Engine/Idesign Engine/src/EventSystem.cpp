#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include <EventReceiverComponent.h> // Ensure this path is correct
#include <BoxColliderComponent.h>
#include <AudioSystem.h>

extern Framework::Coordinator ecsInterface;

namespace Framework {

    void EventSystem::Initialize() {
        // Leave this empty now. 
        // We cannot trust that entities exist during Initialize.
    }

    void EventSystem::Update(float timeslice) {
        // 1. Confirm the System is active (Prints once every 60 frames)
        static int frameCount = 0;
        frameCount++;

        int entityCount = 0;

        for (auto const& entity : mEntities) {
            if (!ecsInterface.HasComponent<EventReceiverComponent>(entity)) continue;
            entityCount++;

            auto& receiver = ecsInterface.GetComponent<EventReceiverComponent>(entity);

            // Inside EventSystem::Update loop...

            if (!receiver.isSubscribed) {

                // Capture the string BY VALUE
                std::string expectedChannel = receiver.messageToListenFor;

                // [DEBUG] 1. Confirm we found a new listener in the scene
                std::cout << "[EventSystem] (+) New Listener detected: Entity " << entity
                    << " is now listening for channel '" << expectedChannel << "'" << std::endl;

                EventManager::Instance().Subscribe(receiver.messageToListenFor,
                    [entity, expectedChannel](void* data) -> bool {

                        // 1. CHECK: Is the Entity dead?
                        if (!ecsInterface.IsEntityValid(entity)) {
                            std::cout << "[EventSystem] (-) Unsubscribing Entity " << entity << " (Entity Invalid/Dead)" << std::endl;
                            return false;
                        }

                        // 2. CHECK: Was the component removed?
                        if (!ecsInterface.HasComponent<EventReceiverComponent>(entity)) {
                            std::cout << "[EventSystem] (-) Unsubscribing Entity " << entity << " (Component Removed)" << std::endl;
                            return false;
                        }

                        // 3. CHECK: Did the user change the text in ImGui?
                        auto& currentReceiver = ecsInterface.GetComponent<EventReceiverComponent>(entity);
                        if (currentReceiver.messageToListenFor != expectedChannel) {
                            std::cout << "[EventSystem] (-) Unsubscribing Entity " << entity
                                << " (Channel changed from '" << expectedChannel
                                << "' to '" << currentReceiver.messageToListenFor << "')" << std::endl;
                            return false;
                        }

                        // --- LOGIC IS SAFE TO RUN ---
                        auto scriptingSystem = ecsInterface.GetSystem<ScriptingSystem>();
                        if (scriptingSystem) {

                            // [DEBUG] 2. Confirm the event hit the specific entity
                            std::cout << "[EventSystem] [>>>] Event '" << expectedChannel << "' received by Entity " << entity
                                << ". Executing Lua: " << currentReceiver.luaFunctionToCall << std::endl;

                            // [CRITICAL FIX] Cast to the struct, not Entity* directly!
                            // Ensure you have #include "PhysicsSystem.h" or wherever CollisionEventData is defined
                            auto* payload = static_cast<CollisionEventData*>(data);

                            Entity target = -1;
                            if (payload) {
                                target = payload->targetID;
                            }
                            int action = payload->actionType; // [NEW] Get 1 or 0
                            scriptingSystem->CallEntityFunction(
                                entity,
                                currentReceiver.luaFunctionToCall,
                                target,
                                action // <--- PASS THIS! (It was -1 before)
                            );
                        }

                        // NEW: Trigger audio if this entity has an AudioComponent
                        if (ecsInterface.HasComponent<AudioComponent>(entity)) 
                        {
                            auto audioSystem = ecsInterface.GetSystem<AudioSystem>();
                            if (audioSystem) {
                                audioSystem->PlayEntitySound(entity);
                                std::cout << "[EventSystem] [AUDIO] Playing sound for entity " << entity << " on channel '" << expectedChannel << "'" << std::endl;
                            }
                        }
                        return true;
                    }
                );

                receiver.isSubscribed = true;
            }
            // Optional: If you really want to see existing listeners every frame (WARNING: SPAMMY)
            // else {
            //    // std::cout << "Entity " << entity << " is already subscribed." << std::endl;
            // }
        }
    }
}