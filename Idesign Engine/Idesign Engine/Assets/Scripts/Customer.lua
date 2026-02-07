-- Customer.lua
local Customer = {}

Customer.interaction_range = 2.5

-- Available items to request
local available_items = {"strength_potion", "vitality_potion", "Warmth_Potion"}

-- Textbox configuration
local prefab_textbox = "CustomerTextbox_Prefab"
local textbox_y_offset = 4.5
local billboard_depth_adjust = 0.055
local x_offset_right = -0.5

-- Log prefab
local prefab_log = "CustomerLog_Prefab"

-- Interaction cooldown
local interaction_cooldown = 0.7
local last_interaction_time = 0.0

-- Wrong item tracking
local MAX_WRONG_ATTEMPTS = 2
local wrong_item_count = 0

-- State
local player_entity = nil
local has_received_item = false
local my_requested_item = ""
local entity_textbox = nil
local elapsed_time = 0.0
local has_arrived = false

function Customer.on_init(self)
    print("[Customer] ===== INIT DEBUG =====")
    print("[Customer] Initialized for entity: " .. tostring(self.entity))
    
    -- Generate own order
    my_requested_item = Customer.GenerateRequest()
    print("[Customer] ✓ Generated order: " .. my_requested_item)
    
    player_entity = FindEntityByName("Player")
    elapsed_time = 0.0
    last_interaction_time = 0.0
    has_arrived = false
    wrong_item_count = 0
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
    
    has_arrived = true
    
    local orderText = "Hi, I want a " .. my_requested_item .. "."
    Customer.SpawnTextbox(senderID, orderText)
    
    Customer.SpawnLog("Customer arrived!")
end

function Customer.OnInteract(senderID, targetID, actionType)
    -- Check if customer has arrived
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
        
        ConsumeSelectedItem()
        
        Customer.DestroyTextbox()
        
        -- Fire completion event to CustomerSystem
        FireEvent("CustomerComplete", customer_entity, senderID, 1)
        Sound.Play("event:/CorrectSound")
        print("[Customer] Thank you! Leaving now...")
        DestroyEntity(customer_entity)
    else
        -- WRONG ITEM LOGIC
        wrong_item_count = wrong_item_count + 1
        print("[Customer] Wrong item! Count: " .. wrong_item_count .. "/" .. MAX_WRONG_ATTEMPTS)
        Sound.Play("event:/wrongSound")
        if wrong_item_count >= MAX_WRONG_ATTEMPTS then
            -- Too many wrong items - customer leaves angry
            print("[Customer] That's it! I've had enough! Leaving...")
            Customer.DestroyTextbox()
            Customer.SpawnLog("Customer left angry!")
            
            -- Fire failure event to CustomerSystem
            FireEvent("CustomerFailed", customer_entity, senderID, 0)
        else
            -- Still give them another chance
            print("[Customer] Sorry, I ordered " .. my_requested_item .. ", not " .. itemName .. "!")
            Customer.SpawnLog("Wrong item! " .. (MAX_WRONG_ATTEMPTS - wrong_item_count) .. " chance(s) left")
        end
    end
end

-- =========================================================
-- ORDER GENERATION
-- =========================================================

function Customer.GenerateRequest()
    local fixedQueue = GetData("fixedQueue")
    
    if fixedQueue > 0 then
        -- Demo mode
        local items = {"warmth_potion", "vitality_potion", "strength_potion"}
        local item = items[fixedQueue] or "strength_potion"
        SetData("fixedQueue", fixedQueue - 1)
        return item
    else
        -- Dynamic mode: only craftable potions
        local requestedItem = GetRandomCraftablePotion()
        
        if not requestedItem then
            print("[Customer] WARNING: No craftable potions!")
            requestedItem = "strength_potion" -- fallback
        end
        
        return requestedItem
    end
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
    local logEntity = SpawnPrefab(prefab_log)
    
    if logEntity then
        SetTextComponent(logEntity, "text", message)
        print("[Customer] Spawned log: " .. message)
    else
        print("[Customer] ERROR: Failed to spawn log!")
    end
end

function Customer.UpdateTextboxBillboard(customer_entity, px, pz, cx, cy, cz)
    if not entity_textbox then return end
    
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz) 
    local angle_deg = math.deg(angle_rad)
    
    local textbox_x = cx + x_offset_right
    local textbox_y = cy + textbox_y_offset
    local textbox_z = cz
    
    local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
    local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)
    
    textbox_x = textbox_x + depth_offset_x
    textbox_z = textbox_z + depth_offset_z
    
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

function Customer.OnTimeout(senderID, targetID, actionType)
    print("[Customer] Timeout! Cleaning up before leaving...")
    
    -- Clean up textbox
    Customer.DestroyTextbox()
    
    print("[Customer] Cleanup complete, ready to be destroyed.")
end

return Customer