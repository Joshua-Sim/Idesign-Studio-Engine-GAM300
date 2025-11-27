-- Customer.lua // Handles on interact
local Customer = {}

-- Configuration
Customer.interaction_range = 2.5

-- Internal state
local player_entity = nil
local has_received_item = false

function Customer.on_init(self)
    print("[Customer] Initialized for entity: " .. self.entity)
    player_entity = FindEntityByName("Player")
end

function Customer.on_update(self, dt)
    -- Customer update logic (NavMesh handles movement)
    if not player_entity then 
        player_entity = FindEntityByName("Player") 
    end
end

-- Called by C++ EventSystem when player presses 'E'
function Customer.OnInteract(senderID, targetID, actionType)
    print("[Customer] Customer interacted by player: " .. tostring(senderID))
    
    -- Get entities
    local customer_entity = targetID
    if not player_entity then 
        player_entity = FindEntityByName("Player") 
    end
    
    -- Accept item
    print("[Customer] Item received! Thank you!")
    has_received_item = true
    
    -- -- FIRE EVENT TO NOTIFY CUSTOMER SYSTEM
    FireEvent("CustomerComplete", customer_entity, 0, 0)

    -- Complete order
    print("[Customer] Order complete. Leaving shop...")
  --  DestroyEntity(customer_entity)
end

return Customer