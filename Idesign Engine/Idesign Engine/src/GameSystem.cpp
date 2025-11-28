#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include "GameSystem.h"
#include <EventReceiverComponent.h> // Ensure this path is correct
#include <BoxColliderComponent.h>
#include "Inputhandler.h"
#include <RecipeBookManager.h>
#include "PhaseManager.h" // Include the header


extern Framework::Coordinator ecsInterface;
Framework::RecipeBookManager* recipeBookManager = nullptr;

namespace Framework {
    int GameSystem::score = 0;
    PhaseManager* phaseManager = nullptr;
    void GameSystem::Initialize() {
        if (recipeBookManager == nullptr) {
            recipeBookManager = new RecipeBookManager(&ecsInterface);
        }
        if (phaseManager == nullptr) {
            phaseManager = new PhaseManager(&ecsInterface);
            phaseManager->Initialize();
        }

    }
    GameSystem::~GameSystem()
    {
        Shutdown();
    }
    void GameSystem::Update(float timeslice) {
        static bool wasBookOpen = false;

        if (GameSystem::bookOpen != wasBookOpen) {

            if (GameSystem::bookOpen) {
                std::cout << "\nBook Opened!" << std::endl;
                if (recipeBookManager) recipeBookManager->BuildBookUI();
            }
            else {
                std::cout << "\nBook Closed!" << std::endl;
                if (recipeBookManager) recipeBookManager->ClearBook();
            }
            wasBookOpen = GameSystem::bookOpen;
        }
        

    }

    void GameSystem::SetScore(int value)
    {
        score = value;
    }

    void GameSystem::AddScore(int delta)
    {
        score += delta;
    }

    int GameSystem::GetScore()
    {
        return score;
    }

    void GameSystem::Shutdown()
    {
        // 1. Clean up Recipe Book
        if (recipeBookManager) {
            recipeBookManager->Shutdown(); // Clears the vectors/strings
            delete recipeBookManager;      // Deletes the object
            recipeBookManager = nullptr;
        }

        // 2. Clean up Phase Manager
        if (phaseManager) {
            // phaseManager->Shutdown(); // If it has one
            delete phaseManager;
            phaseManager = nullptr;
        }

        std::cout << "[GameSystem] Shutdown complete. Managers freed." << std::endl;
    }
}



