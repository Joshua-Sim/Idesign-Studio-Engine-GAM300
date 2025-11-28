-- Customer.lua
local Customer = {}

Customer.interaction_range = 2.5

-- Available items to request
local available_items = {"strength_potion", "vitality_potion"}

-- Textbox configuration
local prefab_textbox = "CustomerTextbox_Prefab"
local textbox_y_offset = 4.5
local billboard_depth_adjust = 0.055
local x_offset_right = -0.5

-- Log prefab
local prefab_log = "CustomerLog_Prefab"

-- Interaction cooldown
local interaction_cooldown = 0.7  -- Seconds between interactions
local last_interaction_time = 0.0

-- State
local player_entity = nil
local has_received_item = false
local my_requested_item = ""
local entity_textbox = nil
local elapsed_time = 0.0
local has_arrived = false  -- NEW: Track if customer reached counter

function Customer.on_init(self)
    print("[Customer] ===== INIT DEBUG =====")
    print("[Customer] Initialized for entity: " .. tostring(self.entity))
    
    -- Generate own order
    my_requested_item = Customer.GenerateRequest()
    print("[Customer] ✓ Generated order: " .. my_requested_item)
    
    player_entity = FindEntityByName("Player")
    elapsed_time = 0.0
    last_interaction_time = 0.0
    has_arrived = false  -- Customer hasn't arrived yet
end

function Customer.on_update(self, dt)
    elapsed_time = elapsed_time + dt
    
    if not player_entity then 
        player_entity = FindEntityByName("Player") 
    end
    
    -- Update textbox billboard if it exists
    if entity_textbox and player_entity then
        local px, py, pz = GetPosition(player_entity)
        local cx, cy, cz = GetPosition(self.entity)
        Customer.UpdateTextboxBillboard(self.entity, px, pz, cx, cy, cz)
    end
end

function Customer.OnArrival(senderID, targetID, actionType)
    print("[Customer] Arrived at counter! Spawning textbox...")
    
    -- Enable interactions
    has_arrived = true
    
    -- Spawn textbox with order
    local orderText = "Hi, I want a " .. my_requested_item .. "."
    Customer.SpawnTextbox(senderID, orderText)
    
    -- Spawn arrival log
    Customer.SpawnLog("Customer arrived!")
end

function Customer.OnInteract(senderID, targetID, actionType)
    -- Check if customer has arrived at counter
    if not has_arrived then
        print("[Customer] I'm not ready yet! Still walking to the counter...")
        return
    end
    
    -- Check cooldown
    local time_since_last = elapsed_time - last_interaction_time
    if time_since_last < interaction_cooldown then
        print("[Customer] Interaction on cooldown (" .. string.format("%.1f", interaction_cooldown - time_since_last) .. "s remaining)")
        return
    end
    
    -- Update last interaction time
    last_interaction_time = elapsed_time
    
    print("[Customer] Customer interacted by player: " .. tostring(senderID))
    
    local customer_entity = targetID
    if not player_entity then 
        player_entity = FindEntityByName("Player") 
    end

    if has_received_item then
        print("[Customer] Customer already has their order!")
        return
    end
    
    -- Check what player is holding
    local heldItemId = GetSelectedItem()
    
    if heldItemId < 0 then
        print("[Customer] You're not holding anything!")
        Customer.SpawnLog("You're not holding anything!")
        return
    end
    
    local itemName = GetItemNameById(heldItemId)
    print("[Customer] Player gave me: " .. itemName)
    print("[Customer] I want: " .. my_requested_item)
    
    -- Check if it matches the order
    if itemName == my_requested_item then
        print("[Customer] Perfect! That's exactly what I wanted!")
        has_received_item = true
        
        -- Take the item
        ConsumeSelectedItem()
        
        -- Add score reward
        AddScore(10)
        print("[Customer] +10 score!")
        
        -- Clean up textbox
        Customer.DestroyTextbox()
        
        -- Spawn success log
        Customer.SpawnLog("Order complete! +10 points")
        
        -- Fire completion event
        FireEvent("CustomerComplete", customer_entity, senderID, 1)
        
        -- Leave
        print("[Customer] Thank you! Leaving now...")
        DestroyEntity(customer_entity)
    else
        print("[Customer] Sorry, I ordered " .. my_requested_item .. ", not " .. itemName .. "!")
        
        -- Spawn wrong item log
        Customer.SpawnLog("Wrong item! Customer wanted " .. my_requested_item)
    end
end

-- =========================================================
-- ORDER GENERATION
-- =========================================================

function Customer.GenerateRequest()
    -- Random pick from available items
    local randomIndex = math.random(1, #available_items)
    local requestedItem = available_items[randomIndex]
    
    print("[Customer] Generated request: " .. requestedItem)
    return requestedItem
end

-- =========================================================
-- TEXTBOX FUNCTIONS
-- =========================================================

function Customer.SpawnTextbox(parentID, text)
    entity_textbox = SpawnPrefab(prefab_textbox, parentID)
    
    if entity_textbox then
        SetTextComponent(entity_textbox, "text", text)
        print("[Customer] Spawned textbox: " .. entity_textbox .. " with text: " .. text)
    else
        print("[Customer] ERROR: Failed to spawn textbox!")
    end
end

function Customer.SpawnLog(message)
    -- Spawn log prefab (already positioned at top-left in prefab)
    local logEntity = SpawnPrefab(prefab_log)
    
    if logEntity then
        -- Update the text
        SetTextComponent(logEntity, "text", message)
        print("[Customer] Spawned log: " .. message)
    else
        print("[Customer] ERROR: Failed to spawn log!")
    end
end

function Customer.UpdateTextboxBillboard(customer_entity, px, pz, cx, cy, cz)
    if not entity_textbox then return end
    
    -- Calculate angle to player
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz) 
    local angle_deg = math.deg(angle_rad)
    
    -- Position textbox above customer
    local textbox_x = cx + x_offset_right
    local textbox_y = cy + textbox_y_offset
    local textbox_z = cz
    
    -- Apply depth offset to prevent Z-fighting
    local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
    local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)
    
    textbox_x = textbox_x + depth_offset_x
    textbox_z = textbox_z + depth_offset_z
    
    -- Update position and rotation
    SetPosition(entity_textbox, textbox_x, textbox_y, textbox_z)
    SetRotation(entity_textbox, 0.0, angle_deg, 0.0)
end

function Customer.DestroyTextbox()
    if entity_textbox then 
        DestroyEntity(entity_textbox)
        entity_textbox = nil
        print("[Customer] Destroyed textbox")
    end
end

-- Timeout handler
function Customer.OnCustomerTimeout(senderID, targetID, actionType)
    print("[Customer] Timeout! I've waited long enough!")
    
    -- Clean up textbox
    Customer.DestroyTextbox()
    
    -- Spawn timeout log
    Customer.SpawnLog("Customer left! -10 points")
    
    -- Leave
    print("[Customer] Leaving disappointed...")
    DestroyEntity(targetID)
end

return Customer