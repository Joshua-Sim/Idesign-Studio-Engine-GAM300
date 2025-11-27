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
#include "TextureProperties.h"
#include "EngineState.h"

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

    void ButtonSystem::Update(float deltaTime)
    {
        (void)deltaTime;

        for (auto entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity)     ||
                !ecsInterface.HasComponent<TransformComponent>(entity)  ||
                !ecsInterface.HasComponent<TextureProperties>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entity);

            // interpret Transform as “center + full size” in design space
            //button.position = glm::vec2(tr.position.x - 0.5f * tr.scale.x,tr.position.y - 0.5f * tr.scale.y);
            button.position = glm::vec2(tr.position.x, tr.position.y);
            button.size = glm::vec2(tr.scale.x, tr.scale.y);

            // Game State Play/Pause
            if (ecsInterface.HasComponent<TextureProperties>(entity)) 
            {
                auto& texture = ecsInterface.GetComponent<TextureProperties>(entity);
                texture.isUI = !engineState.IsPlay(); // true when paused, false when playing
            }
        }
    }

    // ---------------------------------------------------------------------
    // Click handling in design (1920x1080) coordinates
    // ---------------------------------------------------------------------
    void ButtonSystem::HandleClickUV(float u, float v)
    {
        float designX = u * DESIGN_WIDTH;
        float designY = v * DESIGN_HEIGHT;

        for (auto entity : mEntities)
        {
            if (!ecsInterface.HasComponent<ButtonComponent>(entity)     ||
                !ecsInterface.HasComponent<TransformComponent>(entity)  ||
                !ecsInterface.HasComponent<TextureProperties>(entity))
                continue;

            auto& button = ecsInterface.GetComponent<ButtonComponent>(entity);
            auto& tr = ecsInterface.GetComponent<TransformComponent>(entity);
            auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);

            // only UI quads are treated as buttons
            if (!tex.isUI) continue;

            // derive bounds from Transform (center + full size)
            float left = tr.position.x - 0.5f * tr.scale.x;
            float right = tr.position.x + 0.5f * tr.scale.x;
            float bottom = tr.position.y - 0.5f * tr.scale.y;
            float top = tr.position.y + 0.5f * tr.scale.y;

            bool inside = (designX > left && designX < right && designY > bottom && designY < top);
            button.isHovered = inside;

            if (inside) button.TriggerClick(entity);
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