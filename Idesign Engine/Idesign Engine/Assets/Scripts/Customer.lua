-- Customer.lua // Handles on interact
local Customer = {}

-- Configuration
Customer.interaction_range = 2.5

-- Internal state
local player_entity = nil
local has_received_item = false

function Customer:on_init()
    print("[Customer] Initialized for entity: " .. self.entity)
    player_entity = FindEntityByName("Player")
end

function Customer:on_update(dt)
    -- Customer update logic (NavMesh handles movement)
    if not player_entity then 
        player_entity = FindEntityByName("Player") 
    end
end

-- --- THIS IS CALLED BY C++ RAYCAST (Press 'E') ---
function Customer:OnInteract()
    print("[Customer] Customer interacted!")
    
    -- Check if player is in range (safety check)
    if not self:IsPlayerInRange() then
        print("[Customer] Player too far away.")
        return
    end
    
    -- Check if customer already received an item
    if has_received_item then
        print("[Customer] Customer already has their order!")
        return
    end
    
    -- TODO: Check if player is holding the correct item
    -- local playerItem = GetPlayerSelectedItem()
    -- if playerItem == "" or playerItem == nil then
    --     print("[Customer] You're not holding anything!")
    --     return
    -- end
    
    -- For now, accept any interaction
    print("[Customer] Item received! Thank you!")
    has_received_item = true
    
    -- TODO: Add reward logic here (money, score, etc.)
    
    -- Notify the system that this customer is done
    -- We'll call a global function or event system
    self:CompleteOrder()
end
-- --------------------------------------------------

function Customer:IsPlayerInRange()
    if not player_entity then return false end
    
    local cx, cy, cz = GetPosition(self.entity)
    local px, py, pz = GetPosition(player_entity)
    
    local dx = cx - px
    local dy = cy - py
    local dz = cz - pz
    
    local distance = math.sqrt(dx*dx + dy*dy + dz*dz)
    return distance <= self.interaction_range
end

function Customer:CompleteOrder()
    -- Despawn after a short delay (you could add animation/feedback here)
    print("[Customer] Order complete. Leaving shop...")
    
    -- TODO: Play leave animation or walk to exit
    
    -- Destroy this customer entity
    DestroyEntity(self.entity)
    
    -- Notify the system to spawn next customer
    if CustomerSystemNotifyComplete then
        CustomerSystemNotifyComplete()
    end
end

return Customer