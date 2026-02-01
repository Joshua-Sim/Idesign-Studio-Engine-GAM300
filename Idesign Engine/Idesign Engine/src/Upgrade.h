
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

        // Currency multiplier upgrades
        bool UpgradeCurrencyMultiplier(float additiveBonus); // e.g., 0.5 for +50%
        float GetCurrencyMultiplier() const;
        int GetCurrencyUpgradeCost() const;

        // Future upgrades
        // bool UpgradeMaxHealth(int amount);
        // bool UpgradeDamage(float amount);

    private:
        UpgradeManager() = default;
        
        // Inventory System Cost
        int m_inventoryUpgradeCost = 100;
        
        // Currency multiplier system
        float m_currencyMultiplier = 1.0f;  // Start at 1.0x
        int m_currencyUpgradeCost = 150;    // Initial cost
    };
}
#endif // !_UPGRADE_H_