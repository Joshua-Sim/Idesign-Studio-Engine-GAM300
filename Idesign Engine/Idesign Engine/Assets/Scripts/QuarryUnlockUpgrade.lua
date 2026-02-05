-- QuarryUnlockUpgrade.lua
local M = {}

function M:on_init()
    print("[QuarryUnlock] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[QuarryUnlock] Quarry unlock button clicked!")
        
        -- Check if already unlocked
        if IsQuarryUnlocked() then
            print("[QuarryUnlock] Quarry already unlocked!")
            return
        end
        
        -- Get cost
        local cost = GetQuarryUnlockCost()
        local currentCurrency = GetCurrency()
        
        print("[QuarryUnlock] Cost: " .. cost .. ", Current Currency: " .. currentCurrency)
        
        -- Try to unlock
        local success = UnlockQuarry()
        
        if success then
            UpdateCurrencyUI()  -- Update display
            Sound.Play("event:/spendMoney")
            print("[QuarryUnlock] Quarry unlocked!")
            print("[QuarryUnlock] Remaining currency: " .. GetCurrency())
            
            -- Optional: Teleport player to quarry immediately
            -- TeleportToQuarry()
        else
            print("[QuarryUnlock] Failed to unlock quarry!")
            print("[QuarryUnlock] Need " .. cost .. ", have " .. currentCurrency)
        end
    end)
end

return M