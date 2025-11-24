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

    class ButtonSystem : public ISystem {
    public:
        ButtonSystem();
        ButtonSystem(InputHandler* inputHandler);

        void Initialize() override;    // Initialize the system
        void Update(float deltaTime) override; // Update the button state (check for clicks and hover)
        std::string GetName() override { return "ButtonSystem"; }
        //void AddButton(ButtonComponent button); // Add a button
        // Hook the global InputHandler singleton into this system
        void SetInputHandler(InputHandler* handler);
       // std::vector<ButtonComponent> GetButtons() const;
        void HandleClickUV(float u, float v);

    private:
        //std::vector<ButtonComponent> buttons; // List of buttons
        InputHandler* m_inputHandler = nullptr;           // Handle input for click events
        static constexpr float DESIGN_WIDTH = 1920.0f;
        static constexpr float DESIGN_HEIGHT = 1080.0f;
    };

}
