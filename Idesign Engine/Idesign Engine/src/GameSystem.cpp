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
#include "Upgrade.h"

extern Framework::Coordinator ecsInterface;
Framework::RecipeBookManager* recipeBookManager = nullptr;
Framework::InputHandler* inputHandler = nullptr;


namespace Framework
{
    // Static member initialization
    std::unordered_map<std::string, int> GameSystem::gameData;
    PhaseManager* phaseManager = nullptr;

    // =========================================================
    // NIGHT SCALING CONFIG
    // Tweak these to adjust difficulty curve.
    // requiredCustomers = BASE + (night - 1) * PER_NIGHT, capped at MAX
    // =========================================================
    static constexpr int REQUIRED_CUSTOMERS_BASE = 5;   // Night 1: 5 customers required
    static constexpr int REQUIRED_CUSTOMERS_PER_NIGHT = 2;  // +2 per night
    static constexpr int REQUIRED_CUSTOMERS_MAX = 15;  // Hard cap

    void GameSystem::Initialize()
    {
        std::filesystem::path projectRoot = ScriptingSystem::GetProjectRoot();
        std::string itemsPath = (projectRoot / "Assets/JSON/items.json").string();
        bool success = InitializeItemDatabase(itemsPath);

        if (!success) {
            std::cerr << "CRITICAL WARNING: Items failed to load!" << std::endl;
        }

        if (recipeBookManager == nullptr) {
            recipeBookManager = new RecipeBookManager(&ecsInterface);

            if (InputHandler::GetInstance()) {
                InputHandler::GetInstance()->SetRecipeManager(recipeBookManager);
                std::cout << "[GameSystem] Linked RecipeBookManager to InputHandler." << std::endl;
            }
        }

        if (phaseManager == nullptr) {
            phaseManager = new PhaseManager(&ecsInterface);
            phaseManager->Initialize();
        }

        // =========================================================
        // Initialize default values
        // =========================================================
        SetData("currency", 5000);
        SetData("currencyMultiplier", 100);   // 100 = 1.0x
        SetData("customerQueue", 0);

        // Night tracking - starts at 0, incremented to 1 on first OnNightStart
        SetData("currentNight", 0);

        // Required customers - set properly when night starts
        SetData("requiredCustomers", REQUIRED_CUSTOMERS_BASE);

        // =========================================================
        // Subscribe to OnNightStart to increment night counter
        // and recompute per-night values
        // =========================================================
        EventManager::Instance().Subscribe("OnNightStart", [](void* /*data*/) -> bool {
            // Increment night number
            int night = GameSystem::GetData("currentNight") + 1;
            GameSystem::SetData("currentNight", night);

            // Scale required customers: BASE + (night-1) * PER_NIGHT, capped
            int required = REQUIRED_CUSTOMERS_BASE + (night - 1) * REQUIRED_CUSTOMERS_PER_NIGHT;
            if (required > REQUIRED_CUSTOMERS_MAX) required = REQUIRED_CUSTOMERS_MAX;
            GameSystem::SetData("requiredCustomers", required);

            // Scale currency multiplier: grows 10% per night (100 = 1.0x, 110 = 1.1x, ...)
            int multiplier = 100 + (night - 1) * 10;
            GameSystem::SetData("currencyMultiplier", multiplier);

            std::cout << "[GameSystem] Night " << night
                << " started. Required customers: " << required
                << ". Currency multiplier: " << multiplier << "%" << std::endl;

            return true;
            });

        // IMPORTANT: Initialize UpgradeManager to load saved unlock states
        UpgradeManager::Instance().Initialize();
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
                g_AudioSystem->PlayAudio("event:/PaperUp");
            }
            else {
                std::cout << "\nBook Closed!" << std::endl;
                recipeBookManager->bookSpawned = false;
                if (recipeBookManager) recipeBookManager->ClearBook();
                g_AudioSystem->PlayAudio("event:/PaperDown");
            }
            wasBookOpen = GameSystem::bookOpen;
        }

        if (GameSystem::bookOpen && recipeBookManager) {
            recipeBookManager->UpdateVisuals(timeslice);
        }
    }

    // =========================================================
    // DATA MANAGEMENT FUNCTIONS
    // =========================================================

    void GameSystem::SetData(const std::string& key, int value)
    {
        gameData[key] = value;
        if (key.find("ElapsedTime") == std::string::npos)
        {
            std::cout << "[GameSystem] Set " << key << " = " << value << std::endl;
        }
    }

    void GameSystem::AddData(const std::string& key, int delta)
    {
        if (gameData.find(key) == gameData.end()) {
            gameData[key] = 0;
            std::cout << "[GameSystem] Created new data entry: " << key << " = 0" << std::endl;
        }

        gameData[key] += delta;
        std::cout << "[GameSystem] Added " << delta << " to " << key << ", new value = " << gameData[key] << std::endl;
    }

    int GameSystem::GetData(const std::string& key)
    {
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
        if (recipeBookManager) {
            recipeBookManager->Shutdown();
            delete recipeBookManager;
            recipeBookManager = nullptr;
        }

        if (phaseManager) {
            delete phaseManager;
            phaseManager = nullptr;
        }

        gameData.clear();

        std::cout << "[GameSystem] Shutdown complete. Managers freed." << std::endl;
    }
}