-- PointsUpgrade.lua
local M = {}

function M:on_init()
    print("[PointsUpgrade] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[PointsUpgrade] Points upgrade button clicked!")
        
        -- Get current cost
        local cost = GetPointsUpgradeCost()
        local currentScore = GetScore()
        
        print("[PointsUpgrade] Cost: " .. cost .. ", Current Score: " .. currentScore)
        
        -- Check if player can afford it
        if currentScore >= cost then
            -- Deduct the cost
            AddScore(-cost)
            
            -- Apply the upgrade (+50% = 0.5)
            local success = UpgradePointsMultiplier(0.5)
            
            if success then
                local newMultiplier = GetPointsMultiplier()
                local nextCost = GetPointsUpgradeCost()
                print("[PointsUpgrade] Upgraded! New multiplier: " .. newMultiplier .. "x")
                print("[PointsUpgrade] Next upgrade cost: " .. nextCost)
            else
                print("[PointsUpgrade] Upgrade failed!")
                -- Refund the cost if upgrade failed
                AddScore(cost)
            end
        else
            print("[PointsUpgrade] Not enough points! Need " .. cost .. ", have " .. currentScore)
            -- Optional: Show a message to the player
            -- SpawnPrefab("NotEnoughPointsMessage")
        end
    end)
end

return M