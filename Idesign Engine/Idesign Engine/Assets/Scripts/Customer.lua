-- Customer.lua
local Customer = {}

Customer.interaction_range = 2.5

-- Textbox configuration
local prefab_textbox         = "CustomerTextbox_Prefab"
local textbox_y_offset       = 4.5
local billboard_depth_adjust = 0.055
local x_offset_right         = -0.5

-- Log prefab
local prefab_log = "CustomerLog_Prefab"

-- Interaction cooldown
local interaction_cooldown = 0.7

-- Wrong item tracking
local MAX_WRONG_ATTEMPTS = 2

-- =========================================================
-- HELPERS: per-entity state via SetData/GetData
-- All state is keyed by entity ID so each customer instance
-- is fully independent - no shared module-level variables
-- =========================================================
local function GetState(entity, key)
    return GetData(key .. "_" .. tostring(entity))
end

local function SetState(entity, key, value)
    SetData(key .. "_" .. tostring(entity), value)
end

local function ClearState(entity)
    local id = tostring(entity)
    SetData("HasReceived_"      .. id, 0)
    SetData("HasArrived_"       .. id, 0)
    SetData("WrongCount_"       .. id, 0)
    SetData("LastInteractTime_" .. id, 0)
    SetData("ElapsedTime_"      .. id, 0)
    -- CustomerOrder string is managed by CustomerSystem (not SetData)
    SetData("Textbox_"          .. id, 0)
end

-- =========================================================
-- ON INIT
-- =========================================================
function Customer.on_init(self)
    print("[Customer] ===== INIT DEBUG =====")
    print("[Customer] Initialized for entity: " .. tostring(self.entity))

    -- Clear any leftover state
    ClearState(self.entity)

    -- Order is assigned by CustomerSystem at spawn time.
    -- Read it via the global getter (SetData is int-only, cant store strings).
    local order = GetCustomerOrder(self.entity)
    if not order or order == "" then
        print("[Customer] WARNING: No order from CustomerSystem! Using fallback.")
        order = "strength_potion"
    end

    print("[Customer] Order confirmed: " .. tostring(order))
    print("[Customer] State initialized for entity: " .. tostring(self.entity))
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function Customer.on_update(self, dt)
    -- Tick elapsed time in milliseconds (SetData only accepts integers)
    local elapsed_ms = GetState(self.entity, "ElapsedTime") + math.floor(dt * 1000)
    SetState(self.entity, "ElapsedTime", elapsed_ms)

    -- Update textbox billboard if arrived
    if GetState(self.entity, "HasArrived") == 1 then
        local player = FindEntityByName("Player")
        if player then
            local px, py, pz = GetPosition(player)
            local cx, cy, cz = GetPosition(self.entity)
            Customer.UpdateTextboxBillboard(self.entity, px, pz, cx, cy, cz)
        end
    end
end

-- =========================================================
-- ON ARRIVAL
-- =========================================================
function Customer.OnArrival(senderID, targetID, actionType)
    print("[Customer] Entity " .. tostring(targetID) .. " arrived at counter!")

    SetState(targetID, "HasArrived", 1)

    local order = GetCustomerOrder(targetID) or "something"
    local orderText = "Hi, I want a " .. tostring(order) .. "."
    Customer.SpawnTextbox(targetID, orderText)

    Customer.SpawnLog("Customer arrived!")
end

-- =========================================================
-- ON INTERACT
-- =========================================================
function Customer.OnInteract(senderID, targetID, actionType)
    local customer_entity = targetID

    -- Check arrived
    if GetState(customer_entity, "HasArrived") ~= 1 then
        print("[Customer] Not ready yet! Still walking to the counter...")
        return
    end

    -- Check cooldown (stored in ms, compare in seconds)
    local elapsed_ms      = GetState(customer_entity, "ElapsedTime")
    local lastInteract_ms = GetState(customer_entity, "LastInteractTime")
    local time_since_last = (elapsed_ms - lastInteract_ms) / 1000.0

    if time_since_last < interaction_cooldown then
        print("[Customer] Interaction on cooldown (" .. string.format("%.1f", interaction_cooldown - time_since_last) .. "s remaining)")
        return
    end

    -- Store current elapsed ms as last interact time
    SetState(customer_entity, "LastInteractTime", elapsed_ms)

    print("[Customer] Customer " .. tostring(customer_entity) .. " interacted by player: " .. tostring(senderID))

    -- Already served
    if GetState(customer_entity, "HasReceived") == 1 then
        print("[Customer] Customer already has their order!")
        return
    end

    -- Check held item
    local heldItemId = GetSelectedItem()
    if heldItemId < 0 then
        print("[Customer] You're not holding anything!")
        Customer.SpawnLog("You're not holding anything!")
        return
    end

    local itemName = GetItemNameById(heldItemId)
    local my_order = GetCustomerOrder(customer_entity)
    local myType   = GetData("Type_" .. tostring(customer_entity))

    print("[Customer] Player gave: " .. tostring(itemName))
    print("[Customer] I want:      " .. tostring(my_order))

    -- =====================
    -- CORRECT ITEM
    -- =====================
    if itemName == my_order then
        print("[Customer] Perfect! That's exactly what I wanted!")
        SetState(customer_entity, "HasReceived", 1)

        ConsumeSelectedItem()
        Customer.DestroyTextbox(customer_entity)

        -- Apply reward using native AddCurrency binding
        local reward = 100
        AddCurrency(reward)
        UpdateCurrencyUI()
        Customer.SpawnLog("Customer served! +" .. reward .. " gold")
        print("[Customer] Reward applied: +" .. reward .. " currency")

        -- Fire completion event
        FireEvent("CustomerComplete", customer_entity, senderID, 1)

        -- Sounds
        if myType == 1 then
            Sound.Play("event:/FishCorrect")
            Sound.Play("event:/EarnGold")
        else
            Sound.Play("event:/OrcCorrect2")
            Sound.Play("event:/EarnGold")
        end

        print("[Customer] Thank you! Leaving now...")
        DestroyEntity(customer_entity)

    -- =====================
    -- WRONG ITEM
    -- =====================
    else
        local wrong_count = GetState(customer_entity, "WrongCount") + 1
        SetState(customer_entity, "WrongCount", wrong_count)

        print("[Customer] Wrong item! Count: " .. wrong_count .. "/" .. MAX_WRONG_ATTEMPTS)

        -- Sounds
        if myType == 1 then
            Sound.Play("event:/FishWrong")
        else
            Sound.Play("event:/OrcWrong2")
        end

        if wrong_count >= MAX_WRONG_ATTEMPTS then
            print("[Customer] That's it! I've had enough! Leaving...")
            Customer.DestroyTextbox(customer_entity)
            -- Apply penalty using native AddCurrency binding
            local penalty = 100
            AddCurrency(-penalty)
            UpdateCurrencyUI()
            Customer.SpawnLog("Customer left angry! -" .. penalty .. " gold")
            print("[Customer] Penalty applied: -" .. penalty .. " currency")
            FireEvent("CustomerFailed", customer_entity, senderID, 0)
            DestroyEntity(customer_entity)
        else
            local chances_left = MAX_WRONG_ATTEMPTS - wrong_count
            print("[Customer] Sorry, I ordered " .. tostring(my_order) .. ", not " .. tostring(itemName) .. "!")
            Customer.SpawnLog("Wrong item! " .. chances_left .. " chance(s) left")
        end
    end
end

-- =========================================================
-- ON TIMEOUT
-- =========================================================
function Customer.OnTimeout(senderID, targetID, actionType)
    print("[Customer] Entity " .. tostring(targetID) .. " timed out! Cleaning up...")
    Customer.DestroyTextbox(targetID)
    print("[Customer] Cleanup complete.")
end

-- =========================================================
-- ORDER GENERATION
-- =========================================================
function Customer.GenerateRequest()
    local fixedQueue = GetData("fixedQueue")

    if fixedQueue > 0 then
        -- Demo mode
        local items = {"warmth_potion", "vitality_potion", "strength_potion"}
        local item  = items[fixedQueue] or "strength_potion"
        SetData("fixedQueue", fixedQueue - 1)
        return item
    else
        -- Dynamic mode: only craftable potions
        local requestedItem = GetRandomCraftablePotion()
        if not requestedItem then
            print("[Customer] WARNING: No craftable potions! Using fallback.")
            requestedItem = "strength_potion"
        end
        return requestedItem
    end
end

-- =========================================================
-- TEXTBOX FUNCTIONS
-- Textbox entity ID stored per-entity via SetData
-- =========================================================
function Customer.SpawnTextbox(entity, text)
    local textbox = SpawnPrefab(prefab_textbox, entity)
    if textbox then
        SetTextComponent(textbox, "text", text)
        SetData("Textbox_" .. tostring(entity), textbox)
        print("[Customer] Spawned textbox for entity " .. tostring(entity) .. ": " .. text)
    else
        print("[Customer] ERROR: Failed to spawn textbox for entity " .. tostring(entity))
    end
end

function Customer.DestroyTextbox(entity)
    local textboxKey = "Textbox_" .. tostring(entity)
    local textbox    = GetData(textboxKey)
    if textbox and textbox > 0 then
        DestroyEntity(textbox)
        SetData(textboxKey, 0)
        print("[Customer] Destroyed textbox for entity " .. tostring(entity))
    end
end

function Customer.UpdateTextboxBillboard(entity, px, pz, cx, cy, cz)
    local textbox = GetData("Textbox_" .. tostring(entity))
    if not textbox or textbox <= 0 then return end

    local dx        = px - cx
    local dz        = pz - cz
    local angle_rad = math.atan2(dx, dz)
    local angle_deg = math.deg(angle_rad)

    local textbox_x = cx + x_offset_right + billboard_depth_adjust * math.sin(angle_rad)
    local textbox_y = cy + textbox_y_offset
    local textbox_z = cz                  + billboard_depth_adjust * math.cos(angle_rad)

    SetPosition(textbox, textbox_x, textbox_y, textbox_z)
    SetRotation(textbox, 0.0, angle_deg, 0.0)
end

-- =========================================================
-- LOG
-- =========================================================
function Customer.SpawnLog(message)
    local logEntity = SpawnPrefab(prefab_log)
    if logEntity then
        SetTextComponent(logEntity, "text", message)
        print("[Customer] Log: " .. message)
    else
        print("[Customer] ERROR: Failed to spawn log!")
    end
end

return Customer