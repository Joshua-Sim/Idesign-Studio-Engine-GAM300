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
        // In GameSystem.cpp
        static void AddCurrency(int delta) {
            // 1. Get the raw multiplier (stored as an int where 100 = 1.0x)
            int multiplierInt = GetData("currencyMultiplier");

            // Safety check: if it hasn't been initialized yet, default to 100 (1.0x)
            if (multiplierInt <= 0) multiplierInt = 100;

            float multiplier = multiplierInt / 100.0f;

            // 2. Apply multiplier ONLY if the delta is positive (don't multiply penalties/costs)
            int modifiedDelta = delta;
            if (delta > 0) {
                modifiedDelta = static_cast<int>(delta * multiplier);
            }

            // 3. Add to the total
            AddData("currency", modifiedDelta);

            std::cout << "[GameSystem] Added " << modifiedDelta << " gold (Raw: " << delta << " x " << multiplier << ")" << std::endl;
        }
        static int GetCurrency() { return GetData("currency"); }

        // Customer queue functions (now use gameData internally)
        static void SetCustomerQueue(int value) { SetData("customerQueue", value); }
        static void AddCustomerQueue(int delta) { AddData("customerQueue", delta); }
        static int GetCustomerQueue() { return GetData("customerQueue"); }
    };
}