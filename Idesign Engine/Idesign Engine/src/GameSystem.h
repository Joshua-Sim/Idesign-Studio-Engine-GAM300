#pragma once
#include "Coordinator.h"
#include <string>
#include <unordered_map>

namespace Framework {
    // Inherit from ISystem (the interface you shared)
    class GameSystem : public ISystem {
    public:
        ~GameSystem();

        static bool bookOpen;
        static bool bookOpened;
        static bool m_debugRevealAll; 

        void Initialize() override;
        void Update(float timeslice) override;
        std::string GetName() override { return "GameSystem"; }
        void Shutdown();

        static void ToggleDebugReveal();

        // NEW: Flexible data storage
        static std::unordered_map<std::string, int> gameData;

        // Data management functions
        static void SetData(const std::string& key, int value);
        static void AddData(const std::string& key, int delta);
        static int GetData(const std::string& key);
        static bool HasData(const std::string& key);

        // Currency functions (uses gameData internally)
        static void SetCurrency(int value) { SetData("currency", value); }
        static void AddCurrency(int delta) { AddData("currency", delta); }
        static int GetCurrency() { return GetData("currency"); }

        // Legacy score functions (DEPRECATED - redirects to currency)
        static void SetScore(int value) { SetCurrency(value); }
        static void AddScore(int delta) { AddCurrency(delta); }
        static int GetScore() { return GetCurrency(); }

        // Customer queue functions (now use gameData internally)
        static void SetCustomerQueue(int value) { SetData("customerQueue", value); }
        static void AddCustomerQueue(int delta) { AddData("customerQueue", delta); }
        static int GetCustomerQueue() { return GetData("customerQueue"); }
    };
}