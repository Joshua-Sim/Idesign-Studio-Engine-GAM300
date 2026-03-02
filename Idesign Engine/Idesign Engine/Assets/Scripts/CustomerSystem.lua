local CustomerSystem = {}

-- =========================================================
-- CONFIGURATION
-- =========================================================
CustomerSystem.customerPrefabGUID = "Customer_Kinematic_Prefab"
CustomerSystem.spawnPointName     = "SpawnPoint"
CustomerSystem.spawn_delay        = 5.0      -- Stagger delay between each customer spawn

-- [WAIT POINTS] -- Add 3 named entities in your scene matching these names
CustomerSystem.waitPointNames = {
    "CustomerWaitingPoint_1",
    "CustomerWaitingPoint_2",
    "CustomerWaitingPoint_3",
}

CustomerSystem.customer_models = {
    { 
        prefab = "Customer_Kinematic_Prefab",
        walk   = "Armature|WalkAnimation",
        idle   = "Armature|IdleAnimation"
    },
    { 
        prefab = "Customer_Kinematic_2_Prefab",
        walk   = "Armature|WalkAnimation",
        idle   = "Armature|IdleAnimation"
    }
}

-- [PROGRESS BAR CONFIG]
CustomerSystem.prefab_frame = "ProgressBarFrame_Prefab"
CustomerSystem.prefab_fill  = "ProgressBarFill_Prefab"
CustomerSystem.tex_green    = "Assets/Textures/green.png"
CustomerSystem.tex_red      = "Assets/Textures/red.png"

-- =========================================================
-- CONSTANTS
-- =========================================================
local MAX_COUNTER_SLOTS  = 2        -- Max customers at counter simultaneously
local MAX_CUSTOMERS      = 9        -- Total customers per night (adjust as needed)
local PHASE_NIGHT        = 2

-- Patience
local patience_duration  = 120.0
local arrival_threshold  = 1.5

-- Progress bar visuals
local color_swap_threshold = 0.3
local bar_length_max       = 0.5
local bar_thickness        = 0.2
local y_offset             = 3.8
local x_offset_right       = -0.5
local tune_x               = 1.6
local tune_y               = -0.04
local billboard_depth_adjust = 0.055

-- Ingredient check
local ingredient_check_delay = 2.0
local end_phase_delay        = 2.0

-- Log prefab
local prefab_log = "CustomerLog_Prefab"

-- =========================================================
-- INTERNAL STATE
-- =========================================================

-- [SCORE]
local score_text_entity = nil

-- [PHASE]
local previousPhase    = -1
local customers_served = 0
local active_logs      = {}

-- [SPAWN]
local spawnTimer       = 0
local navMeshBuilt     = false
local player_id        = nil
local next_model_index = 1

-- [INGREDIENT / END PHASE]
local ingredient_check_timer  = 0.0
local end_phase_timer         = 0.0
local waiting_to_end          = false
local has_checked_ingredients = false

-- [WAIT POINTS] Resolved entities at init
local waitPointEntities = { nil, nil, nil }

-- =========================================================
-- MULTI-CUSTOMER SLOT TABLE
-- Each slot holds everything needed for that customer + UI
-- =========================================================
--[[
    activeCustomers[slot] = {
        entity      = <entityID>,
        order       = "strength_potion",   -- potion name string (for UI)
        icon_type   = 1 or 2,              -- 1 = fish, 2 = orc (for UI avatar)
        idle_anim   = "Armature|IdleAnimation",
        walk_anim   = "Armature|WalkAnimation",
        patience    = 120.0,               -- current patience (for UI bar)
        is_at_counter = false,
        bar_state   = "green",
        bar_frame   = <entityID or nil>,
        bar_fill    = <entityID or nil>,
        last_pos_x  = 0,
        last_pos_z  = 0,
        is_moving   = true,
    }
--]]
local activeCustomers = { nil, nil, nil }

-- =========================================================
-- HELPER: Count how many slots are occupied
-- =========================================================
local function CountActiveSlots()
    local count = 0
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] ~= nil then count = count + 1 end
    end
    return count
end

-- =========================================================
-- HELPER: Find a free slot index (1/2/3), or nil if full
-- =========================================================
local function GetFreeSlot()
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] == nil then return i end
    end
    return nil
end

-- =========================================================
-- HELPER: Find which slot an entity belongs to
-- =========================================================
local function FindSlotByEntity(entity)
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] and activeCustomers[i].entity == entity then
            return i
        end
    end
    return nil
end

-- =========================================================
-- GLOBAL HELPER FUNCTIONS
-- =========================================================
function UpdateCurrencyUI()
    if score_text_entity then
        local current_currency = GetCurrency()
        local currency_text = "        :" .. tostring(current_currency)
        SetTextComponent(score_text_entity, "text", currency_text)
        print("[UpdateCurrencyUI] Display updated to: " .. current_currency)
    else
        print("[UpdateCurrencyUI] Warning: score_text_entity is nil!")
    end
end

-- =========================================================
-- UI DATA ACCESSOR
-- Call this from your UI system to get all active customer data
-- Returns a table of up to 3 entries, nil slots are empty
-- =========================================================
function GetCustomerUIData()
    local result = {}
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] then
            local c = activeCustomers[i]
            result[i] = {
                slot        = i,
                order       = c.order,                              -- potion name string
                icon_type   = c.icon_type,                          -- 1 or 2
                patience    = c.patience,                           -- current value
                patience_pct = c.patience / patience_duration,      -- 0.0 to 1.0 (for bar fill)
                is_arriving = not c.is_at_counter,                  -- still walking in
            }
        else
            result[i] = nil  -- empty slot
        end
    end
    return result
end

-- =========================================================
-- ON INIT
-- =========================================================
function CustomerSystem:on_init()
    print("[CustomerSystem] >> Initialized.")

    player_id = FindEntityByName("Player")

    -- Resolve all 3 wait point entities
    for i = 1, MAX_COUNTER_SLOTS do
        waitPointEntities[i] = FindEntityByName(self.waitPointNames[i])
        if waitPointEntities[i] then
            print("[CustomerSystem] Found wait point: " .. self.waitPointNames[i])
        else
            print("[CustomerSystem] WARNING: Missing wait point: " .. self.waitPointNames[i])
        end
    end

    -- Score text
    local existing_score = FindEntityByName("ScoreText")
    if existing_score then
        score_text_entity = existing_score
        print("[CustomerSystem] ScoreText already exists, reusing it.")
    else
        score_text_entity = SpawnPrefab("ScoreText_Prefab")
        if score_text_entity then
            SetTransformComponent(score_text_entity, "position", {x = 0.010, y = 0.060, z = 0})
            SetTextComponent(score_text_entity, "color", {r = 0, g = 0, b = 0})
            SetTextComponent(score_text_entity, "scale", 1.5)
            CustomerSystem:UpdateCurrencyDisplay()
        else
            print("[CustomerSystem] ERROR: Failed to spawn ScoreText prefab!")
        end
    end

    -- NavMesh
    if not navMeshBuilt then
        print("[CustomerSystem] Building NavMesh...")
        local success = BuildNavMesh()
        if success then
            print("[CustomerSystem] NavMesh built successfully!")
            navMeshBuilt = true
        else
            print("[CustomerSystem] ERROR: Failed to build NavMesh!")
        end
    end

    self:ResetNightPhase()
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function CustomerSystem:on_update(dt)

    -- Score text visibility
    if score_text_entity then
        if IsEngineState("Play") or IsEngineState("Upgrade") or IsEngineState("Editor") then
            SetTextComponent(score_text_entity, "opacity", 1.0)
        else
            SetTextComponent(score_text_entity, "opacity", 0.0)
        end
    end

    if not navMeshBuilt then return end

    local currentPhase = GetPhase()

    -- Phase transition into night
    if currentPhase == PHASE_NIGHT and previousPhase ~= PHASE_NIGHT then
        print("[CustomerSystem] --- Transition INTO Night Phase ---")
        self:ResetNightPhase()
    end
    previousPhase = currentPhase

    if currentPhase ~= PHASE_NIGHT then return end

    -- === DELAYED END PHASE ===
    if waiting_to_end then
        end_phase_timer = end_phase_timer + dt
        if end_phase_timer >= end_phase_delay then
            print("[CustomerSystem] Timer complete. Ending night phase now.")
            self:EndNightPhase()
        end
        return
    end

    local active_count = CountActiveSlots()

    -- === INGREDIENT CHECK (once any customer is active) ===
    if active_count > 0 and not has_checked_ingredients then
        ingredient_check_timer = ingredient_check_timer + dt
        if ingredient_check_timer >= ingredient_check_delay then
            if not HasCraftablePotions() then
                print("[CustomerSystem] No craftable potions detected!")
                self:SpawnLog("No potions available! Starting upgrade phase...")
                Sound.Play("event:/wrongSound")
                waiting_to_end = true
                end_phase_timer = 0.0
                has_checked_ingredients = true
                return
            else
                has_checked_ingredients = true
                print("[CustomerSystem] Ingredient check passed. Continuing night phase.")
            end
        end
    end

    -- === NORMAL END CHECK ===
    if customers_served >= MAX_CUSTOMERS and active_count == 0 then
        print("[CustomerSystem] Max customers reached. Ending Night Phase.")
        self:EndNightPhase()
        return
    end

    -- === SPAWN LOGIC: spawn if slots free and haven't hit max yet ===
    local total_spawned_or_active = customers_served + active_count
    if total_spawned_or_active < MAX_CUSTOMERS and active_count < MAX_COUNTER_SLOTS then
        spawnTimer = spawnTimer + dt
        if spawnTimer >= self.spawn_delay then
            if not HasCraftablePotions() then
                print("[CustomerSystem] Cannot spawn - no craftable potions!")
                self:SpawnLog("No potions available! Starting upgrade phase...")
                Sound.Play("event:/wrongSound")
                waiting_to_end = true
                end_phase_timer = 0.0
                return
            end

            local freeSlot = GetFreeSlot()
            if freeSlot then
                print("[CustomerSystem] Free slot found: " .. freeSlot .. ". Spawning customer...")
                self:spawn_customer(freeSlot)
                spawnTimer = 0

                -- Reset ingredient check when a new customer spawns
                ingredient_check_timer = 0.0
                has_checked_ingredients = false
            end
        end
    else
        spawnTimer = 0
    end

    -- === UPDATE ALL ACTIVE CUSTOMER SLOTS ===
    for slot = 1, MAX_COUNTER_SLOTS do
        local c = activeCustomers[slot]
        if c then
            self:UpdateSlot(slot, c, dt)
        end
    end
end

-- =========================================================
-- UPDATE A SINGLE SLOT (movement check + patience)
-- =========================================================
function CustomerSystem:UpdateSlot(slot, c, dt)
    local waitPt = waitPointEntities[slot]

    -- VELOCITY / ANIMATION CHECK (while walking)
    if not c.is_at_counter then
        local cx, cy, cz = GetPosition(c.entity)

        local dx = cx - c.last_pos_x
        local dz = cz - c.last_pos_z
        local dist_moved = math.sqrt(dx*dx + dz*dz)
        local speed = dist_moved / dt

        if speed < 0.1 then
            if c.is_moving then
                SetAnimationSettings(c.entity, 1.0, true, true)
                PlayEntityAnimation(c.entity, c.idle_anim)
                c.is_moving = false
            end
        else
            if not c.is_moving then
                SetAnimationSettings(c.entity, 1.0, true, true)
                PlayEntityAnimation(c.entity, c.walk_anim)
                c.is_moving = true
            end
        end

        c.last_pos_x = cx
        c.last_pos_z = cz

        -- ARRIVAL CHECK
        if waitPt then
            local wx, wy, wz = GetPosition(waitPt)
            local dist = math.sqrt((cx - wx)^2 + (cy - wy)^2 + (cz - wz)^2)

            if dist < arrival_threshold then
                print("[CustomerSystem] Slot " .. slot .. " >> ARRIVED!")
                c.is_at_counter = true
                c.is_moving = false

                SetAnimationSettings(c.entity, 1.0, true, true)
                PlayEntityAnimation(c.entity, c.idle_anim)

                c.patience = patience_duration
                self:SpawnBarForSlot(slot, c)
                FireEvent("OnArrival", c.entity, c.entity, 0)
            end
        end

    else
        -- PATIENCE COUNTDOWN
        c.patience = c.patience - dt

        if c.patience <= 0 then
            print("[CustomerSystem] Slot " .. slot .. " timed out!")
            CustomerSystem:SpawnLog("Time's up!")
            FireEvent("CustomerTimeout", c.entity, c.entity, 0)
            FireEvent("CustomerFailed",  c.entity, c.entity, 0)
            return
        end

        -- Update bar visuals
        local pct = c.patience / patience_duration

        if c.bar_fill then
            -- Color swap
            if pct <= color_swap_threshold then
                if c.bar_state == "green" then
                    SetTexture(c.bar_fill, self.tex_red)
                    c.bar_state = "red"
                end
            else
                if c.bar_state == "red" then
                    SetTexture(c.bar_fill, self.tex_green)
                    c.bar_state = "green"
                end
            end

            local current_length = pct * bar_length_max
            SetScale(c.bar_fill, current_length, bar_thickness * 0.9, bar_thickness * 0.9)
        end

        -- Billboard update
        if player_id then
            local px, py, pz = GetPosition(player_id)
            local cx, cy, cz = GetPosition(c.entity)
            self:UpdateBillboardForSlot(c, px, pz, cx, cy, cz)
        end
    end
end

-- =========================================================
-- SPAWN CUSTOMER INTO A SPECIFIC SLOT
-- =========================================================
function CustomerSystem:spawn_customer(slot)
    local spawnPoint = FindEntityByName(self.spawnPointName)
    if not spawnPoint then
        print("[CustomerSystem] ERROR: SpawnPoint not found!")
        return
    end

    self:play_doorbell()

    local modelData = self.customer_models[next_model_index]
    local customer  = SpawnPrefab(modelData.prefab, spawnPoint)

    if not customer then
        print("[CustomerSystem] ERROR: Failed to spawn customer prefab!")
        return
    end
   
    -- Give this customer a unique name based on slot
    local uniqueName = "Customer_Slot_" .. slot
    SetEntityName(customer, uniqueName)

    -- Store model type on entity (Customer.lua uses this for sounds)
    SetData("Type_" .. customer, next_model_index)

    -- Store which slot this customer belongs to (used by OnComplete/OnFailed)
    SetData("Slot_" .. customer, slot)

    -- Build the slot data table
    activeCustomers[slot] = {
        entity        = customer,
        entity_name   = uniqueName,
        order         = "",           -- filled in by OnArrival after Customer.lua sets it via SetData
        icon_type     = next_model_index,
        idle_anim     = modelData.idle,
        walk_anim     = modelData.walk,
        patience      = patience_duration,
        is_at_counter = false,
        bar_state     = "green",
        bar_frame     = nil,
        bar_fill      = nil,
        last_pos_x    = 0,
        last_pos_z    = 0,
        is_moving     = true,
    }

    -- Walk animation
    SetAnimationSettings(customer, 1.0, true, true)
    PlayEntityAnimation(customer, modelData.walk)

    local sx, sy, sz = GetPosition(customer)
    activeCustomers[slot].last_pos_x = sx
    activeCustomers[slot].last_pos_z = sz

    -- Pathfind to this slot's wait point
    -- FIX: use actual entity name instead of prefab name
    print("[CustomerSystem] Slot " .. slot .. " >> Pathfinding to " .. self.waitPointNames[slot])
    PathfindTo(uniqueName, self.waitPointNames[slot])

    -- Alternate model
    next_model_index = (next_model_index % #self.customer_models) + 1
    print("[CustomerSystem] Spawned customer in slot " .. slot)
end

-- =========================================================
-- ON ARRIVAL (fired by Customer.lua)
-- Pull the order from SetData now that Customer.lua has set it
-- =========================================================
function CustomerSystem.OnArrival(senderID, targetID, actionType)
    local slot = GetData("Slot_" .. targetID)
    if not slot or slot < 1 or slot > MAX_COUNTER_SLOTS then
        print("[CustomerSystem] OnArrival: Could not find slot for entity " .. tostring(targetID))
        return
    end

    local c = activeCustomers[slot]
    if not c then return end

    -- Pull order that Customer.lua stored via SetData in its on_init
    local order = GetData("CustomerOrder_" .. tostring(targetID))
    c.order = order or "unknown"

    print("[CustomerSystem] Slot " .. slot .. " arrived. Order: " .. c.order)
end

-- =========================================================
-- ON CUSTOMER COMPLETE
-- =========================================================
function CustomerSystem.OnCustomerComplete(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER COMPLETE ==========")

    local slot = GetData("Slot_" .. senderID)
    if not slot then
        print("[CustomerSystem] OnCustomerComplete: Slot not found for entity " .. tostring(senderID))
        return
    end

    -- Currency
    local base_amount   = 100
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then multiplier_int = 100 end
    local multiplier    = multiplier_int / 100.0
    local actual_earned = math.floor(base_amount * multiplier)

    CustomerSystem:AddCurrency(base_amount)

    if multiplier > 1.0 then
        CustomerSystem:SpawnLog("Customer served! +" .. actual_earned .. " currency (" .. multiplier .. "x)")
    else
        CustomerSystem:SpawnLog("Customer served! +" .. actual_earned .. " currency")
    end

    customers_served = customers_served + 1

    -- Clean up slot
    CustomerSystem:ClearSlot(slot)
end

-- =========================================================
-- ON CUSTOMER FAILED
-- =========================================================
function CustomerSystem.OnCustomerFailed(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER FAILED ==========")

    local slot = GetData("Slot_" .. senderID)
    if not slot then
        print("[CustomerSystem] OnCustomerFailed: Slot not found for entity " .. tostring(senderID))
        return
    end

    -- Penalty
    local base_penalty   = 100
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then multiplier_int = 100 end
    local multiplier        = multiplier_int / 100.0
    local penalty_multiplier = 1.0 + ((multiplier - 1.0) * 0.5)
    local actual_penalty    = math.floor(base_penalty * penalty_multiplier)

    local current_currency = GetCurrency()
    local new_currency     = math.max(0, current_currency - actual_penalty)
    SetCurrency(new_currency)
    CustomerSystem:UpdateCurrencyDisplay()

    if penalty_multiplier > 1.0 then
        CustomerSystem:SpawnLog("Customer left angry! -" .. actual_penalty .. " currency (" .. penalty_multiplier .. "x)")
    else
        CustomerSystem:SpawnLog("Customer left angry! -" .. actual_penalty .. " currency")
    end

    print("[CustomerSystem] Penalty: -" .. actual_penalty)

    customers_served = customers_served + 1

    -- Clean up slot
    CustomerSystem:ClearSlot(slot)
end

-- =========================================================
-- CLEAR A SLOT (destroy bar, free table entry, clean SetData)
-- =========================================================
function CustomerSystem:ClearSlot(slot)
    local c = activeCustomers[slot]
    if not c then return end

    -- Destroy progress bar
    self:DestroyBarForSlot(slot, c)

    -- Clean up SetData keys
    SetData("CustomerOrder_" .. tostring(c.entity), "")
    SetData("Slot_"          .. tostring(c.entity), -1)

    -- Destroy entity if still alive (e.g. timeout path)
    -- Customer.lua's OnInteract already destroys on success/fail,
    -- but timeout goes through OnCustomerFailed so guard here
    -- (DestroyEntity is safe to call even if already destroyed in most engines)

    activeCustomers[slot] = nil

    print("[CustomerSystem] Slot " .. slot .. " cleared.")
end

-- =========================================================
-- PROGRESS BAR — PER SLOT
-- =========================================================
function CustomerSystem:SpawnBarForSlot(slot, c)
    c.bar_frame = SpawnPrefab(self.prefab_frame, c.entity)
    if c.bar_frame then
        SetScale(c.bar_frame, bar_length_max, bar_thickness, bar_thickness * 5)
    end
    c.bar_fill = SpawnPrefab(self.prefab_fill, c.entity)
    if c.bar_fill then
        SetScale(c.bar_fill, bar_length_max, bar_thickness, bar_thickness)
    end
    SetTexture(c.bar_fill, self.tex_green)
    c.bar_state = "green"
end

function CustomerSystem:DestroyBarForSlot(slot, c)
    if c.bar_frame then DestroyEntity(c.bar_frame); c.bar_frame = nil end
    if c.bar_fill  then DestroyEntity(c.bar_fill);  c.bar_fill  = nil end
    c.bar_state = "green"
end

-- =========================================================
-- BILLBOARD — PER SLOT
-- =========================================================
function CustomerSystem:UpdateBillboardForSlot(c, px, pz, cx, cy, cz)
    local dx        = px - cx
    local dz        = pz - cz
    local angle_rad = math.atan2(dx, dz)
    local angle_deg = math.deg(angle_rad)

    local frame_x = cx + x_offset_right
    local frame_y = cy + y_offset
    local frame_z = cz

    if c.bar_frame then
        SetPosition(c.bar_frame, frame_x, frame_y, frame_z)
        SetRotation(c.bar_frame, 0.0, angle_deg, 0.0)
    end

    if c.bar_fill then
        SetRotation(c.bar_fill, 0.0, angle_deg + 180.0, 0.0)

        local right_angle   = angle_rad + (math.pi / 2.0)
        local side_dist     = bar_length_max * tune_x
        local side_offset_x = side_dist * math.sin(right_angle)
        local side_offset_z = side_dist * math.cos(right_angle)
        local depth_x       = billboard_depth_adjust * math.sin(angle_rad)
        local depth_z       = billboard_depth_adjust * math.cos(angle_rad)

        SetPosition(c.bar_fill,
            frame_x + side_offset_x + depth_x,
            frame_y + tune_y,
            frame_z + side_offset_z + depth_z)
    end
end

-- =========================================================
-- RESET NIGHT PHASE
-- =========================================================
function CustomerSystem:ResetNightPhase()
    print("[CustomerSystem] Resetting night phase state...")
    self:ClearAllLogs()

    customers_served = 0
    SetData("fixedQueue", 0)

    -- Destroy all active customers
    for slot = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[slot] then
            self:DestroyBarForSlot(slot, activeCustomers[slot])
            DestroyEntity(activeCustomers[slot].entity)
            activeCustomers[slot] = nil
        end
    end

    spawnTimer            = 0
    next_model_index      = 1
    ingredient_check_timer = 0.0
    end_phase_timer       = 0.0
    waiting_to_end        = false
    has_checked_ingredients = false

    CustomerSystem:UpdateCurrencyDisplay()
end

-- =========================================================
-- END NIGHT PHASE
-- =========================================================
function CustomerSystem:EndNightPhase()
    print("[CustomerSystem] Ending Night Phase...")
    self:ClearAllLogs()

    -- Destroy all bars
    for slot = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[slot] then
            self:DestroyBarForSlot(slot, activeCustomers[slot])
        end
    end

    GameStateEndNightPhase()
end

-- =========================================================
-- CURRENCY
-- =========================================================
function CustomerSystem:AddCurrency(amount)
    local current_currency = GetCurrency()
    local multiplier_int   = GetData("currencyMultiplier")
    if multiplier_int <= 0 then
        multiplier_int = 100
        SetData("currencyMultiplier", 100)
    end

    local multiplier      = multiplier_int / 100.0
    local modified_amount = math.floor(amount * multiplier)
    local new_currency    = math.max(0, current_currency + modified_amount)

    SetCurrency(new_currency)
    print("[CustomerSystem] Currency +" .. modified_amount .. " (x" .. multiplier .. "), total: " .. new_currency)
    CustomerSystem:UpdateCurrencyDisplay()
end

function CustomerSystem:UpdateCurrencyDisplay()
    UpdateCurrencyUI()
end

function CustomerSystem:GetScore()
    return GetScore()
end

-- =========================================================
-- LOGS
-- =========================================================
function CustomerSystem:SpawnLog(message)
    local logEntity = SpawnPrefab(prefab_log)
    if logEntity then
        SetTextComponent(logEntity, "text", message)
        table.insert(active_logs, logEntity)
    end
end

function CustomerSystem:ClearAllLogs()
    for _, logEntity in ipairs(active_logs) do
        if logEntity then DestroyEntity(logEntity) end
    end
    active_logs = {}
end

-- =========================================================
-- DOORBELL
-- =========================================================
function CustomerSystem:play_doorbell()
    local frogEntity = FindEntityByName("Frog")
    if frogEntity then PlayEntitySound(frogEntity) end
end

return CustomerSystem