#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include "GameSystem.h"
#include <EventReceiverComponent.h> // Ensure this path is correct
#include <BoxColliderComponent.h>
#include "Inputhandler.h"
#include <RecipeBookManager.h>

extern Framework::Coordinator ecsInterface;
Framework::RecipeBookManager* recipeBookManager = nullptr;

namespace Framework {

    void GameSystem::Initialize() {
        if (recipeBookManager == nullptr) {
            recipeBookManager = new RecipeBookManager(&ecsInterface);
        }
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
}