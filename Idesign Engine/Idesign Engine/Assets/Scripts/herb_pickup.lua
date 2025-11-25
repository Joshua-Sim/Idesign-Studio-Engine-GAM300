local M = {}

function M.OnPickup(entityID)
    local myName = GetItemName(entityID) 
    local slot = AddToInventory(myName)

    if slot >= 0 then
        print("Lua: Picked up " .. myName)
        
        if IsItemDestroyOnPick(entityID) then
            DestroyEntity(entityID)
        end
    else
        print("Inventory Full!")
    end

    return true
end

return M