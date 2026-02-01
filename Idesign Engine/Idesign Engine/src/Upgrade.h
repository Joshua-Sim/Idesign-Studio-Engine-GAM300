
#pragma once
#ifndef _UPGRADE_H_
#define _UPGRADE_H_

#include "Inventory.h"
#include "GameSystem.h"

namespace Framework
{
    enum GameArea
    {
        Home = 0,       // Default starting area
        Forest = 1,     // Unlockable area for mining
        Quarry = 2      // Unlockable area for forging
    };

    class UpgradeManager
    {
    public:
        static UpgradeManager& Instance();

        // Inventory upgrades
        bool UpgradeInventorySlots(int amount);
        int GetInventoryUpgradeCost() const;
        bool CanAffordInventoryUpgrade() const;

        // Currency multiplier upgrades
        bool UpgradeCurrencyMultiplier(float additiveBonus); // e.g., 0.5 for +50%
        float GetCurrencyMultiplier() const;
        int GetCurrencyUpgradeCost() const;
        bool CanAffordCurrencyUpgrade() const;

        // Area unlock upgrades
        bool UnlockArea(GameArea area);
        bool IsAreaUnlocked(GameArea area) const;
        int GetAreaUnlockCost(GameArea area) const;

    private:
        UpgradeManager() = default;
        
        // Inventory System Cost
        int m_inventoryUpgradeCost = 50;
        
        // Currency multiplier system
        float m_currencyMultiplier = 1.0f;  // Start at 1.0x
        int m_currencyUpgradeCost = 100;    // Initial cost

        // Area unlock system
        std::unordered_map<GameArea, bool> m_unlockedAreas = 
        {
            {GameArea::Home, true},     // Home is always unlocked
            {GameArea::Forest, false},  // Forest starts locked
            {GameArea::Quarry, false}   // Quarry starts locked
        };

        std::unordered_map<GameArea, int> m_areaUnlockCosts = 
        {
            {GameArea::Forest, 500},  // Forest costs 500 currency
            {GameArea::Quarry, 1000}  // Quarry costs 1000 currency
        };
    };
}
#endif // !_UPGRADE_H_