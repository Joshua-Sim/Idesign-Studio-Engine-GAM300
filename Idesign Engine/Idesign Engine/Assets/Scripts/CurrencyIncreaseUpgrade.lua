-- CurrencyIncreaseUpgrade.lua
local M = {}

function M:on_init()
    print("[CurrencyUpgrade] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[CurrencyUpgrade] Currency upgrade button clicked!")
        
        -- Get current cost
        local cost = GetCurrencyUpgradeCost()
        local currentCurrency = GetCurrency()
        
        print("[CurrencyUpgrade] Cost: " .. cost .. ", Current Currency: " .. currentCurrency)
        
        -- Check if player can afford it
        if currentCurrency >= cost then
            -- Deduct the cost
            AddCurrency(-cost)
            
            -- Apply the upgrade (+50% = 0.5)
            local success = UpgradeCurrencyMultiplier(0.5)
            
            if success then
                local newMultiplier = GetCurrencyMultiplier()
                local nextCost = GetCurrencyUpgradeCost()
                print("[CurrencyUpgrade] Upgraded! New multiplier: " .. newMultiplier .. "x")
                print("[CurrencyUpgrade] Next upgrade cost: " .. nextCost)
            else
                print("[CurrencyUpgrade] Upgrade failed!")
                -- Refund the cost if upgrade failed
                AddCurrency(cost)
            end
        else
            print("[CurrencyUpgrade] Not enough currency! Need " .. cost .. ", have " .. currentCurrency)
        end
    end)
end

return M