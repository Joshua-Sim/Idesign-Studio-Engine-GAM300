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

        void Initialize() override;
        void Update(float timeslice) override;
        std::string GetName() override { return "GameSystem"; }
        void Shutdown();

        // NEW: Flexible data storage
        static std::unordered_map<std::string, int> gameData;

        // Data management functions
        static void SetData(const std::string& key, int value);
        static void AddData(const std::string& key, int delta);
        static int GetData(const std::string& key);
        static bool HasData(const std::string& key);

        // Legacy score functions (now use gameData internally)
        static void SetScore(int value) { SetData("score", value); }
        static void AddScore(int delta) { AddData("score", delta); }
        static int GetScore() { return GetData("score"); }

        // Customer queue functions (now use gameData internally)
        static void SetCustomerQueue(int value) { SetData("customerQueue", value); }
        static void AddCustomerQueue(int delta) { AddData("customerQueue", delta); }
        static int GetCustomerQueue() { return GetData("customerQueue"); }
    };
}