#pragma once
#pragma once
#include "Coordinator.h"
#include "System.h" // Assuming you have a base System class
#include "MessageSystem.h"
#include "EventSenderComponent.h"
#include "GameMessages.h"
// You will also need your Input and Physics/Raycasting headers here

namespace Framework {

    class InteractionSystem : public System {
    public:
        void Init(Coordinator* coord) {
            mCoordinator = coord;
        }

        void Update() {
            // --- This logic is an example ---

            // 1. Check for input (Assuming you have an Input manager)
            if (Input::IsKeyJustPressed("E")) {

                // 2. Get the player
                auto& players = mCoordinator->GetEntitiesByTag("Player");
                if (players.empty()) return;
                Entity player = *players.begin();

                // 3. Find what the player is looking at (Assuming you have a raycast)
                Entity target = Physics::RaycastFrom(player);
                if (!mCoordinator->IsEntityValid(target)) return;

                // 4. Check if the target is interactable
                if (mCoordinator->HasComponent<EventSenderComponent>(target)) {

                    auto& sender = mCoordinator->GetComponent<EventSenderComponent>(target);

                    // 5. Check if this is the trigger it's waiting for
                    if (sender.onTrigger == "OnInteract") {

                        // 6. Map the string to the C++ message and send it
                        if (sender.messageName == "PlayerInteractedWithItem") {

                            MessageBus::Get().QueueMessage(
                                PlayerInteractedWithItem{ player, target } // {interactor, interactee}
                            );
                        }
                    }
                }
            }
        }
    private:
        Coordinator* mCoordinator;
    };
}