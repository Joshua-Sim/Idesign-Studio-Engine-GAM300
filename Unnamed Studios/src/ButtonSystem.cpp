///////////////////////////////////////////////////////////////////////////////
///
/// @file ButtonSystem.cpp
/// 
/// @brief Implementation of the ButtonSystem class, which manages interactive 
///        buttons within the game engine. This class provides functionality for the buttons
///  
/// @authors Joshua Sim Yue Chen
/// @copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////


#include "pch.h"
#include <iostream>

#include "ButtonSystem.h"

namespace Framework {

    ButtonSystem::ButtonSystem(InputHandler* inputHandler) : inputHandler(inputHandler) {}

    void ButtonSystem::Initialize() {
        // Initialize buttons if needed
    }

    void ButtonSystem::Update(float deltaTime) {
        (void)deltaTime;
        // Check for button interaction, hover, and clicks
        for (auto& button : buttons) {
            double mouseX = inputHandler->GetMouseX();
            double mouseY = inputHandler->GetMouseY();

            // Check if the mouse is over the button
            if (mouseX > button.position.x && mouseX < button.position.x + button.size.x &&
                mouseY > button.position.y && mouseY < button.position.y + button.size.y) {
                button.isHovered = true;

                // Check if the button is clicked
                if (inputHandler->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                    button.TriggerClick();
                }
            }
            else {
                button.isHovered = false;
            }
        }
    }

    void ButtonSystem::AddButton(ButtonComponent button) {
        buttons.push_back(button);
    }

    std::vector<ButtonComponent> ButtonSystem::GetButtons() const {
        return buttons;
    }

}