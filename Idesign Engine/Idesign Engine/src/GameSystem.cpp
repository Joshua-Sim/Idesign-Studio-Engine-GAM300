#include "pch.h" 
#include "EventSystem.h"
#include "EventManager.h"
#include "ScriptingSystem.h"
#include "GameSystem.h"
#include <EventReceiverComponent.h>
#include <BoxColliderComponent.h>
#include "Inputhandler.h"
#include <RecipeBookManager.h>
#include "PhaseManager.h"
#include <fstream>                      
#include <rapidjson/document.h>         
#include <rapidjson/istreamwrapper.h>   
#include "ItemComponent.h"

extern Framework::Coordinator ecsInterface;
Framework::RecipeBookManager* recipeBookManager = nullptr;
Framework::InputHandler* inputHandler = nullptr;


namespace Framework {
    // Static member initialization
    std::unordered_map<std::string, int> GameSystem::gameData;

    PhaseManager* phaseManager = nullptr;



    void GameSystem::Initialize() {

        std::filesystem::path projectRoot = Framework::ScriptingSystem::GetProjectRoot();
        std::string itemsPath = (projectRoot / "Assets/JSON/items.json").string();
        bool success = Framework::InitializeItemDatabase(itemsPath);

        if (!success) {
            std::cerr << "CRITICAL WARNING: Items failed to load!" << std::endl;
            // Optional: return -1; if the game cannot function without items
        }

        if (recipeBookManager == nullptr) {
            recipeBookManager = new RecipeBookManager(&ecsInterface);

            if (InputHandler::GetInstance()) {
                InputHandler::GetInstance()->SetRecipeManager(recipeBookManager);
                std::cout << " [GameSystem] Linked RecipeBookManager to InputHandler." << std::endl;
            }
        }

        if (phaseManager == nullptr) {
            phaseManager = new PhaseManager(&ecsInterface);
            phaseManager->Initialize();
        }

        // Initialize default values
        SetData("score", 0);
        SetData("customerQueue", 0);
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
                recipeBookManager->bookSpawned = false;
                if (recipeBookManager) recipeBookManager->ClearBook();
            }
            wasBookOpen = GameSystem::bookOpen;
        }
    }

    // =========================================================
    // DATA MANAGEMENT FUNCTIONS
    // =========================================================

    void GameSystem::SetData(const std::string& key, int value)
    {
        gameData[key] = value;
        std::cout << "[GameSystem] Set " << key << " = " << value << std::endl;
    }

    void GameSystem::AddData(const std::string& key, int delta)
    {
        // Check if key exists, if not create with 0
        if (gameData.find(key) == gameData.end()) {
            gameData[key] = 0;
            std::cout << "[GameSystem] Created new data entry: " << key << " = 0" << std::endl;
        }

        gameData[key] += delta;
        std::cout << "[GameSystem] Added " << delta << " to " << key << ", new value = " << gameData[key] << std::endl;
    }

    int GameSystem::GetData(const std::string& key)
    {
        // Check if key exists, if not return 0
        if (gameData.find(key) == gameData.end()) {
            std::cout << "[GameSystem] Data key '" << key << "' not found, returning 0" << std::endl;
            return 0;
        }
        return gameData[key];
    }

    bool GameSystem::HasData(const std::string& key)
    {
        return gameData.find(key) != gameData.end();
    }

    void GameSystem::Shutdown()
    {
        // 1. Clean up Recipe Book
        if (recipeBookManager) {
            recipeBookManager->Shutdown();
            delete recipeBookManager;
            recipeBookManager = nullptr;
        }

        // 2. Clean up Phase Manager
        if (phaseManager) {
            delete phaseManager;
            phaseManager = nullptr;
        }

        // 3. Clear game data
        gameData.clear();

        std::cout << "[GameSystem] Shutdown complete. Managers freed." << std::endl;
    }
}