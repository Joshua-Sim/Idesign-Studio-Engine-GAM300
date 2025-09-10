///////////////////////////////////////////////////////////////////////////////
///
///	\file PlayerSystem.cpp
/// 
/// * The FontSystem loads fonts, generates textures for each glyph, and renders text in the Unnamed Engine.
/// * It leverages the FreeType library for font loading and the OpenGL pipeline for efficient rendering.
/// * The system allows dynamic text rendering and can be easily extended for more advanced features.
/// 
///	Authors: Joshua Sim Yue Chen
///	Copyright 2024, Digipen Institute of Technology
///////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include <ft2build.h>
#include "PlayerSystem.h"
#include "Coordinator.h"
#include "GLEW.h"
#include <iostream> // For debugging or logging if needed
#include "EngineState.h"

extern Framework::Coordinator ecsInterface;
extern Framework::EngineState engineState;

namespace Framework {

    PlayerSystem::PlayerSystem() {
    
    }



    void PlayerSystem::Initialize() {
        ecsInterface.RegisterComponent<PlayerComponent>();
        //  ---- ECS INITIALIZATION --- 
        Signature signature;
        signature.set(ecsInterface.GetComponentType<PlayerComponent>()); //Set mEntities to be populated with RenderComponent holding Entities
        ecsInterface.SetSystemSignature<PlayerSystem>(signature);

        //  ---- ECS INITIALIZATION END --- 

        // ---- Create Input Handler Instance (Relocate to appropriate system *
        //  only renders*)
        inputHandler = InputHandler::GetInstance();
    }

    void PlayerSystem::Update(float dt) {
        (void)dt;
        if (!engineState.IsPlay()) {
            return; 
            // Dont execute this system
        }
        // Retrieve the single player entity (assumes only one player entity exists)
        for (auto const& entity : mEntities) {
            TextComponent& textComponent = ecsInterface.GetComponent<TextComponent>(entity);

            // Process input for alphabet keys (GLFW_KEY_A to GLFW_KEY_Z)
            for (int key = GLFW_KEY_A; key <= GLFW_KEY_Z; ++key) {
                if (inputHandler->IsKeyPressed(key)) {
                    // Convert GLFW key to corresponding character
                    char inputChar = 'A' + (static_cast<char>(key) - GLFW_KEY_A);

                    // Add character if under 10 characters limit
                    if (playerText.length() < 15) {
                        playerText += inputChar;
                    }
                }
            }

            if (inputHandler->IsKeyHeld(GLFW_KEY_BACKSPACE)) {
                if (!playerText.empty()) {
                    playerText.pop_back(); // Remove the last character
                }
            }


            // Clear text on Enter key
            if (inputHandler->IsKeyPressed(GLFW_KEY_ENTER)) {
                playerText.clear();
                engineState.SetPaused(true);
            }

            // Update the TextComponent with the player text
            textComponent.text = playerText;
        }
    }



    void PlayerSystem::HandleTextInput(char inputChar) {
        // Add character to the text, handle backspace if necessary
        if (inputChar >= 'A' && inputChar <= 'Z') {
            playerText += inputChar;
        }
        else if (inputChar == '\b' && !playerText.empty()) {
            playerText.pop_back(); // Handle backspace
        }
    }

    std::string PlayerSystem::GetName() {
        return "PlayerSystem";
    }

    PlayerSystem::~PlayerSystem() {
        // Cleanup logic if required
        std::cout << "PlayerSystem destroyed.\n";
    }

}