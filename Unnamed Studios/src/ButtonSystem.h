///////////////////////////////////////////////////////////////////////////////
///
/// @file ButtonSystem.h
/// 
/// @brief Implementation of the ButtonSystem class, which manages interactive 
///        buttons within the game engine. This class provides functionality for the buttons
///  
/// @authors Joshua Sim Yue Chen
/// @copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "pch.h"
#include <vector>
#include "Vector2D.h"
#include "System.h"
#include "InputHandler.h"
#include "fstream"

// RapidJson
#pragma once
#include <vector>
#include "ComponentList.h"
#include "InputHandler.h"

namespace Framework {

    class ButtonSystem {
    public:
        ButtonSystem(InputHandler* inputHandler);

        void Initialize();    // Initialize the system
        void Update(float deltaTime); // Update the button state (check for clicks and hover)
        void AddButton(ButtonComponent button); // Add a button

        std::vector<ButtonComponent> GetButtons() const;

    private:
        std::vector<ButtonComponent> buttons; // List of buttons
        InputHandler* inputHandler;           // Handle input for click events
    };

}
