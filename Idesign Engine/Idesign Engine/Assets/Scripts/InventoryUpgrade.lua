-- InventoryUpgrade.lua
local M = {}

function M:on_init()
    print("[InventoryUpgrade] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[InventoryUpgrade] Upgrade button clicked!")
        
        -- Get current currency and cost
        local cost = GetInventoryUpgradeCost()
        local currentCurrency = GetCurrency()
        
        print("[InventoryUpgrade] Cost: " .. cost .. ", Current Currency: " .. currentCurrency)
        
        -- Check if player can afford it
        if currentCurrency >= cost then
            -- Try to upgrade (C++ will handle currency deduction)
            local success = UpgradeInventorySlots(1)
            
            if success then
                UpdateCurrencyUI()
                local newSlots = GetInventoryMaxSlots()
                local newCost = GetInventoryUpgradeCost()
                print("[InventoryUpgrade] Upgraded! New slots: " .. newSlots)
                print("[InventoryUpgrade] Next upgrade cost: " .. newCost)
                print("[InventoryUpgrade] Remaining currency: " .. GetCurrency())
            else
                print("[InventoryUpgrade] Already at max slots!")
            end
        else
            print("[InventoryUpgrade] Not enough currency!")
            print("[InventoryUpgrade] Need " .. cost .. ", have " .. currentCurrency)
            -- Optional: spawn a message for the player
            -- SpawnPrefab("NotEnoughCurrencyMessage")
        end
    end)
end

return M