-- CurrencyIncreaseUpgrade.lua
local M = {}

function M:on_init()
    print("[CurrencyUpgrade] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[CurrencyUpgrade] Currency upgrade button clicked!")
        
        -- Get info for display purposes
        local cost = GetCurrencyUpgradeCost()
        local currentCurrency = GetCurrency()
        
        print("[CurrencyUpgrade] Cost: " .. cost .. ", Current Currency: " .. currentCurrency)
        
        -- Just call the function - C++ handles everything!
        local success = UpgradeCurrencyMultiplier(0.5)
        
        if success then
            UpdateCurrencyUI()
            local newMultiplier = GetCurrencyMultiplier()
            local nextCost = GetCurrencyUpgradeCost()
            print("[CurrencyUpgrade] Upgraded! New multiplier: " .. newMultiplier .. "x")
            print("[CurrencyUpgrade] Next upgrade cost: " .. nextCost)
            print("[CurrencyUpgrade] Remaining currency: " .. GetCurrency())
        else
            print("[CurrencyUpgrade] Not enough currency!")
            print("[CurrencyUpgrade] Need " .. cost .. ", have " .. currentCurrency)
        end
    end)
end

return M