
#include "pch.h"
#include "Upgrade.h"

namespace Framework
{
    UpgradeManager& UpgradeManager::Instance() 
    {
        static UpgradeManager instance;
        return instance;
    }

    // =========================================================
    // INVENTORY UPGRADES
    // =========================================================
    bool UpgradeManager::CanAffordInventoryUpgrade() const
    {
        int currentCurrency = GameSystem::GetCurrency();
        return currentCurrency >= m_inventoryUpgradeCost;
    }

    bool UpgradeManager::UpgradeInventorySlots(int amount)
    {
        auto& inv = Inventory::Instance();

        // Check if inventory can be upgraded
        /*if (!inv.CanUpgradeSlots())
        {
            std::cout << "[UpgradeManager] Cannot upgrade inventory slots (already at max)" << std::endl;
            return false;
        }*/

        // Check if player can afford it
        if (!CanAffordInventoryUpgrade())
        {
            std::cout << "[UpgradeManager] Not enough currency! Need " << m_inventoryUpgradeCost
                << ", have " << GameSystem::GetCurrency() << std::endl;
            return false;
        }

        // Deduct the cost
        GameSystem::AddCurrency(-m_inventoryUpgradeCost);
        std::cout << "[UpgradeManager] Deducted " << m_inventoryUpgradeCost << " currency" << std::endl;

        // Apply the upgrade
        //inv.UpgradeSlots(amount);

        // Increase cost for next upgrade
        //m_inventoryUpgradeCost += 50;

        std::cout << "[UpgradeManager] Inventory upgraded! New slots: " << inv.GetHotbarSlotCount()
            << ", Next cost: " << m_inventoryUpgradeCost << std::endl;

        return true;
    }

    int UpgradeManager::GetInventoryUpgradeCost() const 
    {
        return m_inventoryUpgradeCost;
    }

    // =========================================================
    // CURRENCY MULTIPLIER UPGRADES
    // =========================================================
    bool UpgradeManager::CanAffordCurrencyUpgrade() const
    {
        int currentCurrency = GameSystem::GetCurrency();
        return currentCurrency >= m_currencyUpgradeCost;
    }

    bool UpgradeManager::UpgradeCurrencyMultiplier(float additiveBonus)
    {
        // Check if player can afford it
        if (!CanAffordCurrencyUpgrade())
        {
            std::cout << "[UpgradeManager] Not enough currency! Need " << m_currencyUpgradeCost
                << ", have " << GameSystem::GetCurrency() << std::endl;
            return false;
        }

        // Deduct the cost
        GameSystem::AddCurrency(-m_currencyUpgradeCost);
        std::cout << "[UpgradeManager] Deducted " << m_currencyUpgradeCost << " currency" << std::endl;

        // Apply the upgrade
        m_currencyMultiplier += additiveBonus;

        // Store as integer in GameSystem (100 = 1.0x)
        int multiplierAsInt = static_cast<int>(m_currencyMultiplier * 100);
        GameSystem::SetData("currencyMultiplier", multiplierAsInt);

        std::cout << "[UpgradeManager] Currency multiplier upgraded to "
            << m_currencyMultiplier << "x" << std::endl;

        // Increase cost for next upgrade
        //m_currencyUpgradeCost += 100;

        std::cout << "[UpgradeManager] Next currency upgrade cost: " << m_currencyUpgradeCost << std::endl;

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

    bool UpgradeManager::UnlockArea(GameArea area)
    {
        // Check if already unlocked
        if (IsAreaUnlocked(area))
        {
            std::cout << "[UpgradeManager] Area already unlocked!" << std::endl;
            return false;
        }

        // Check if player can afford it
        int cost = GetAreaUnlockCost(area);
        int currentCurrency = GameSystem::GetCurrency();

        if (currentCurrency < cost)
        {
            std::cout << "[UpgradeManager] Not enough currency! Need " << cost
                << ", have " << currentCurrency << std::endl;
            return false;
        }

        // Deduct the cost
        GameSystem::AddCurrency(-cost);
        std::cout << "[UpgradeManager] Deducted " << cost << " currency" << std::endl;

        // Unlock the area
        m_unlockedAreas[area] = true;

        // Store in GameSystem for persistence
        std::string areaKey = "area_" + std::to_string(static_cast<int>(area));
        GameSystem::SetData(areaKey, 1);  // 1 = unlocked

        std::cout << "[UpgradeManager] Area unlocked! Remaining currency: "
            << GameSystem::GetCurrency() << std::endl;

        return true;
    }

    bool UpgradeManager::IsAreaUnlocked(GameArea area) const
    {
        auto it = m_unlockedAreas.find(area);
        if (it != m_unlockedAreas.end())
        {
            return it->second;
        }
        return false;
    }

    int UpgradeManager::GetAreaUnlockCost(GameArea area) const
    {
        auto it = m_areaUnlockCosts.find(area);
        if (it != m_areaUnlockCosts.end())
        {
            return it->second;
        }
        return 0;
    }

    bool UpgradeManager::CanAffordBackpackUpgrade() const
    {
        int currentCurrency = GameSystem::GetCurrency();
        return currentCurrency >= m_backpackUpgradeCost;
    }

    bool UpgradeManager::UpgradeBackpack()
    {
        auto& inv = Inventory::Instance();
        int currentLevel = inv.GetBackpackLevel();

        // Check if already at max
        if (currentLevel >= 3)
        {
            std::cout << "[UpgradeManager] Backpack already at max level!" << std::endl;
            return false;
        }

        // Check if player can afford it
        if (!CanAffordBackpackUpgrade())
        {
            std::cout << "[UpgradeManager] Not enough currency! Need " << m_backpackUpgradeCost
                << ", have " << GameSystem::GetCurrency() << std::endl;
            return false;
        }

        // Deduct the cost
        GameSystem::AddCurrency(-m_backpackUpgradeCost);
        std::cout << "[UpgradeManager] Deducted " << m_backpackUpgradeCost << " currency" << std::endl;

        // Apply the upgrade
        inv.SetBackpackLevel(currentLevel + 1);

        // Increase cost for next upgrade
        //m_backpackUpgradeCost += 150;

        std::cout << "[UpgradeManager] Backpack upgraded to level " << (currentLevel + 1)
            << "! Next cost: " << m_backpackUpgradeCost << std::endl;

        return true;
    }

    int UpgradeManager::GetBackpackUpgradeCost() const
    {
        return m_backpackUpgradeCost;
    }

    /*
    // C++ - UpgradeManager
    bool UpgradeManager::UpgradeXXX(params)
    {
        // 1. Check pre-conditions (max level, etc)
        if (!CanUpgradeXXX()) return false;
    
        // 2. Check affordability
        if (!CanAffordXXXUpgrade()) return false;
    
        // 3. Deduct currency
        GameSystem::AddCurrency(-m_xxxUpgradeCost);
    
        // 4. Apply the upgrade
        ApplyXXXUpgrade(params);
    
        // 5. Increase cost for next time
        m_xxxUpgradeCost += increment;
    
        return true;
    }

    -- Lua - Simple button callback
    RegisterButtonCallback(self.entity, function(ent)
        local success = UpgradeXXX(params)

        if success then
            print("Upgraded!")
        else
            print("Can't upgrade!")
        end
    end)
    */
}