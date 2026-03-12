-- Customer.lua
local Customer = {}

Customer.interaction_range = 2.5

-- Textbox configuration
local prefab_textbox   = "CustomerTextbox_Prefab"
local textbox_y_offset = 4.5

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
end

-- =========================================================
-- ON ARRIVAL
-- =========================================================
function Customer.OnArrival(senderID, targetID, actionType)
    print("[Customer] Entity " .. tostring(targetID) .. " arrived at counter!")

    SetState(targetID, "HasArrived", 1)

    -- 1. Get the internal string ID from the system (e.g., "warmth_potion")
    local orderIDString = GetCustomerOrder(targetID) or "strength_potion"
    
    -- 2. Call your newly registered C++ function
    local displayName = GetItemDisplayName(orderIDString)

    -- 3. Display the fancy name in the speech bubble
    local orderText = "Hi, I want a " .. tostring(displayName) .. "."
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

        -- 1. Get the dynamic reward from the item data
        -- We use the heldItemId we already fetched earlier in the function
        local reward = GetItemValue(heldItemId) 
        
        -- Fallback safety check
        if not reward or reward <= 0 then
            reward = 100 -- Minimum fallback price
        end

        ConsumeSelectedItem()
        Customer.DestroyTextbox(customer_entity)

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
        local niceName = GetItemDisplayName(my_order)
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
        -- Say "I ordered Potion of Warmth" instead of "I ordered warmth_potion"
            print("[Customer] Sorry, I ordered " .. tostring(niceName) .. ", not " .. tostring(itemName) .. "!")
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
        local items = {"warmth_potion", "vitality_potion", "strength_potion", "mending_potion", "defense_potion", "preservation_powder", "healing_powder"}
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
-- Textbox entity ID stored per-entity via SetData.
-- isBillboard=true in the prefab so the engine handles facing;
-- we only need to set the world position once on spawn.
-- =========================================================
function Customer.SpawnTextbox(entity, text)
    local textboxKey = "Textbox_" .. tostring(entity)
    -- Guard: already has a live textbox, don't spawn a duplicate
    local existing = GetData(textboxKey)
    if existing and existing > 0 then
        print("[Customer] Textbox already exists for entity " .. tostring(entity) .. ", skipping spawn.")
        return
    end

    local textbox = SpawnPrefab(prefab_textbox, entity)
    if not textbox then
        print("[Customer] ERROR: Failed to spawn textbox for entity " .. tostring(entity))
        return
    end

    SetTextComponent(textbox, "text", text)
    SetData(textboxKey, textbox)

    -- Position above the customer with a per-slot Y stagger to avoid overlap
    local cx, cy, cz = GetPosition(entity)
    local slot = GetData("Slot_" .. tostring(entity))
    local slot_y_stagger = (slot == 2) and 0.7 or 0.0
    SetPosition(textbox, cx, cy + textbox_y_offset + slot_y_stagger, cz)

    print("[Customer] Spawned textbox for entity " .. tostring(entity) .. ": " .. text)
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