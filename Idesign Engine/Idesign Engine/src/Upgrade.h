
#pragma once
#ifndef _UPGRADE_H_
#define _UPGRADE_H_

#include "Inventory.h"
#include "GameSystem.h"

namespace Framework
{
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

    private:
        UpgradeManager() = default;
        
        // Inventory System Cost
        int m_inventoryUpgradeCost = 50;
        
        // Currency multiplier system
        float m_currencyMultiplier = 1.0f;  // Start at 1.0x
        int m_currencyUpgradeCost = 100;    // Initial cost
    };
}
#endif // !_UPGRADE_H_