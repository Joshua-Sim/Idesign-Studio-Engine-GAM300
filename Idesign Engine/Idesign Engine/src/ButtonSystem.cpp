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
#include "Coordinator.h"
#include "ButtonSystem.h"
extern Framework::Coordinator ecsInterface;
namespace Framework {

    ButtonSystem::ButtonSystem() : m_inputHandler(nullptr) {}
    ButtonSystem::ButtonSystem(InputHandler* inputHandler) : m_inputHandler(inputHandler) {}

    void ButtonSystem::SetInputHandler(InputHandler* handler)
    {
        m_inputHandler = handler;
    }

    void ButtonSystem::Initialize() {
        // Initialize buttons if needed
    }


    void ButtonSystem::Update(float deltaTime) {
        (void)deltaTime;

    }
    void ButtonSystem::HandleClickUV(float u, float v)
    {
        // u, v are in [0,1], with v = 0 at top of the viewport (ImGui convention)
        // Our 2D UI projection is bottom-left origin, so flip v.
        float designX = u * DESIGN_WIDTH;
        float designY = v * DESIGN_HEIGHT;

        for (auto& entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);

            // AABB in design-space
            float left = button.position.x;
            float right = button.position.x + button.size.x;
            float bottom = button.position.y;
            float top = button.position.y + button.size.y;

            bool inside = (designX > left && designX < right &&
                designY > bottom && designY < top);

            button.isHovered = inside;

            if (inside)
            {
                // This function is only called on a mouse click,
                // so we can immediately trigger the callback.
                button.TriggerClick();
            }
        }
    }
}

//    void ButtonSystem::AddButton(ButtonComponent button) {
//        buttons.push_back(button);
//    }
//
//    std::vector<ButtonComponent> ButtonSystem::GetButtons() const {
//        return buttons;
//    }
//
//}