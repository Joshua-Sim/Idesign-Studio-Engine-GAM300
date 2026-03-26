#pragma once
#include "Coordinator.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Framework 
{
    // Inherit from ISystem (the interface you shared)
    class GameSystem : public ISystem 
    {
    public:

        // =========================================================
        // DAY STATS — one entry per completed day
        // =========================================================
        struct DayStats
        {
            int dayIndex = 0;   // 1-based
            int goldFromCustomers = 0;   // post-multiplier gold from fulfilled orders
            int timeBonusGold = 0;   // remaining_seconds * TIME_BONUS_RATE
            int totalGold = 0;   // goldFromCustomers + timeBonusGold
            int customersServed = 0;   // orders successfully fulfilled
            int customersSpawned = 0;   // total orders queued this night
            int timeRemainingX100 = 0;   // seconds * 100 (int-safe storage)
            int stars = 0;   // 0 / 1 / 2 / 3
        };

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
        static void AddCurrency(int delta) 
        {
            // Change this temporarily:
            int mInt = GetData("currencyMultiplier");

            // 2. Safety: If it's 0 or unitialized, default to 100%
            if (mInt < 100) mInt = 100;

            // 3. Convert to FLOAT (Crucial: use .0f)
            float multiplier = mInt / 100.0f;
            std::cout << std::endl;
            std::cout << "[GameSystem] Adding currency with multiplier: " << mInt << "%" << std::endl;

            // 4. Multiply for income, keep raw for expenses
            int finalAmount = delta;
            if (delta > 0) 
            {
                finalAmount = static_cast<int>(delta * multiplier);
                
                // Accumulate for today's stats (tracks post-multiplier gold)
                AddData("dayGoldFromCustomers", finalAmount);
            }

			std::cout << "[GameSystem] Final currency change after multiplier: " << finalAmount << std::endl;
            std::cout << std::endl;

            // 5. Apply the data
            AddData("currency", finalAmount);

            // Debug this line to see the math in the console!
            std::cout << "[C++ Math] " << delta << " * " << multiplier << " = " << finalAmount << std::endl;
        }

        static int GetCurrency() { return GetData("currency"); }

        // Customer queue functions (now use gameData internally)
        static void SetCustomerQueue(int value) { SetData("customerQueue", value); }
        static void AddCustomerQueue(int delta) { AddData("customerQueue", delta); }
        static int GetCustomerQueue() { return GetData("customerQueue"); }

        // =========================================================
        // DAY STATS API
        // =========================================================

        // Called by PhaseManager when Day -> Night fires.
        // timeRemaining: seconds left on the day timer (0 if natural expiry).
        static void SaveDayStats(float timeRemaining);

        // Resets per-day accumulators. Subscribed to OnDayStart event.
        static void ResetDayAccumulators();

        // Returns a copy of the last completed day's stats.
        static DayStats GetLastDayStats();

        // Returns all day history (read-only).
        static const std::vector<DayStats>& GetDayHistory() { return s_dayHistory; }

        // Gold earned per second of remaining day time. Tune freely.
        static constexpr float TIME_BONUS_RATE = 2.0f;

    private:
        static std::vector<DayStats> s_dayHistory;
        static int ComputeStars(int served, int spawned, int totalGold);
    };
}