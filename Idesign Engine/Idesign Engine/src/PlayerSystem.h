#pragma once

#include "pch.h"
#include "System.h"
#include "InputHandler.h"
#include "ComponentList.h"
#include <string>

namespace Framework {
    class PlayerSystem : public ISystem {
    public:
        PlayerSystem();

        // Initialize the system
        void Initialize() override;

        // Update the system each frame
        void Update(float timeslice) override;

        // Returns the name of the system
        std::string GetName() override;

        // Destructor
        ~PlayerSystem();

    private:
        InputHandler* inputHandler; // Reference to the input handler
        std::string playerText;     // Stores the current typed text for the player entity

        // Helper function to handle text input
        void HandleTextInput(char inputChar);
    };
}
