
#include "pch.h"
#include "Upgrade.h"

namespace Framework
{
    UpgradeManager& UpgradeManager::Instance() 
    {
        static UpgradeManager instance;
        return instance;
    }

    bool UpgradeManager::UpgradeInventorySlots(int amount) 
    {
        auto& inv = Inventory::Instance();

        if (!inv.CanUpgradeSlots()) 
        {
            return false;
        }

        inv.UpgradeSlots(amount);

        // Increase cost for next upgrade
        m_inventoryUpgradeCost += 50;

        return true;
    }

    int UpgradeManager::GetInventoryUpgradeCost() const 
    {
        return m_inventoryUpgradeCost;
    }

    bool UpgradeManager::UpgradeCurrencyMultiplier(float additiveBonus)
    {
        m_currencyMultiplier += additiveBonus;

        // Store as integer in GameSystem (100 = 1.0x)
        int multiplierAsInt = static_cast<int>(m_currencyMultiplier * 100);
        GameSystem::SetData("currencyMultiplier", multiplierAsInt);

        std::cout << "[UpgradeManager] Currency multiplier upgraded to "
            << m_currencyMultiplier << "x" << std::endl;

        // Increase cost for next upgrade
        m_currencyUpgradeCost += 100;

        return true;
    }

    float UpgradeManager::GetCurrencyMultiplier() const
    {
        return m_currencyMultiplier;
    }

    int UpgradeManager::GetCurrencyUpgradeCost() const
    {
        return m_currencyUpgradeCost;
    }
}