local M = {}

function M:on_init()
    print("[InventoryUpgrade] Initializing for entity:", self.entity)
    
    RegisterButtonCallback(self.entity, function(ent)
        print("[InventoryUpgrade] Upgrade button clicked!")
        
        -- Just try to upgrade
        local success = UpgradeInventorySlots(1)
        
        if success then
            local newSlots = GetInventoryMaxSlots()
            print("[InventoryUpgrade] Upgraded! New slots:", newSlots)
        else
            print("[InventoryUpgrade] Already at max slots!")
        end
    end)
end

return M