local CustomerSystem = {}

-- =========================================================
-- CONFIGURATION
-- =========================================================
CustomerSystem.customerPrefabGUID = "Customer_Kinematic_Prefab"
CustomerSystem.spawnPointName     = "SpawnPoint"

-- Base wait time between customer spawns (seconds). Halved each use -> spawn_delay / 2.
-- Scales down per night via GetData("currentNight").
CustomerSystem.base_wait_time     = 10.0    -- Night 1 base (divided by 2 = 5s first spawn delay).
CustomerSystem.min_wait_time      = 2.0     -- Floor so it never gets impossibly fast.

-- [WAIT POINTS] -- 2 named entities in your scene matching these names
CustomerSystem.waitPointNames = {
    "CustomerWaitingPoint_1",
    "CustomerWaitingPoint_2",
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
local MAX_COUNTER_SLOTS  = 2        -- 2 customers at counter simultaneously
local PHASE_NIGHT        = 2

-- MAX_CUSTOMERS is now read dynamically from GameSystem each night
-- (set by GameSystem.cpp's OnNightStart handler via SetData("requiredCustomers", ...))
-- Fallback of 5 is used if the key hasn't been set yet (e.g. very first frame)
local function GetRequiredCustomers()
    local n = GetData("requiredCustomers")
    if not n or n <= 0 then return 5 end
    return n
end

-- Patience
local patience_duration    = 120.0
local arrival_threshold    = 1.5

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

-- Free-play
local FREE_PLAY_DURATION = 300.0   -- 5 minutes

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
local effective_spawn_delay = 10.0   -- computed at night start

-- [INGREDIENT / END PHASE]
local ingredient_check_timer  = 0.0
local end_phase_timer         = 0.0
local waiting_to_end          = false
local has_checked_ingredients = false

-- [FREE PLAY]
local is_free_play       = false
local free_play_timer    = 0.0

-- [WAIT POINTS] Resolved entities at init
local waitPointEntities = { nil, nil }

-- =========================================================
-- MULTI-CUSTOMER SLOT TABLE
-- Each slot holds everything needed for that customer + UI
-- =========================================================
--[[
    activeCustomers[slot] = {
        entity        = <entityID>,
        order         = "strength_potion",
        icon_type     = 1 or 2,
        idle_anim     = "Armature|IdleAnimation",
        walk_anim     = "Armature|WalkAnimation",
        patience      = 120.0,
        is_at_counter = false,
        bar_state     = "green",
        bar_frame     = <entityID or nil>,
        bar_fill      = <entityID or nil>,
        last_pos_x    = 0,
        last_pos_z    = 0,
        is_moving     = true,
    }
--]]
local activeCustomers = { nil, nil }

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
-- HELPER: Find a free slot index (1 or 2), or nil if full
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
-- HELPER: Compute spawn delay for this night
-- Spawn delay = max(base_wait / night_scale, min_wait) / 2
-- =========================================================
local function ComputeSpawnDelay()
    local night = GetData("currentNight")
    if not night or night <= 0 then night = 1 end

    -- Each night the wait shrinks: base / sqrt(night) gives a smooth ramp
    local scaled = CustomerSystem.base_wait_time / math.sqrt(night)
    local clamped = math.max(scaled, CustomerSystem.min_wait_time)

    -- Divided by 2 as per design (2 customers share one wait interval)
    local result = clamped / 2.0
    print("[CustomerSystem] Night " .. night .. " spawn delay: " .. string.format("%.1f", result) .. "s")
    return result
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
-- Called by HUDSystem to get active customer data (2 slots)
-- =========================================================
function GetCustomerUIData()
    local result = {}
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] then
            local c = activeCustomers[i]
            result[i] = {
                slot         = i,
                order        = c.order,
                icon_type    = c.icon_type,
                patience     = c.patience,
                patience_pct = c.patience / patience_duration,
                is_arriving  = not c.is_at_counter,
            }
        else
            result[i] = nil
        end
    end
    return result
end

-- =========================================================
-- FREE PLAY UI DATA
-- =========================================================
function GetFreePlayTimeLeft()
    if not is_free_play then return nil end
    return math.max(0, FREE_PLAY_DURATION - free_play_timer)
end

-- =========================================================
-- ON INIT
-- =========================================================
function CustomerSystem:on_init()
    print("[CustomerSystem] >> Initialized.")

    player_id = FindEntityByName("Player")

    -- Resolve 2 wait point entities
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

    -- === FREE PLAY MODE ===
    if is_free_play then
        free_play_timer = free_play_timer + dt

        -- Spawn customers freely while time remains and slots are open
        local active_count = CountActiveSlots()
        if active_count < MAX_COUNTER_SLOTS then
            spawnTimer = spawnTimer + dt
            if spawnTimer >= effective_spawn_delay then
                if HasCraftablePotions() then
                    local freeSlot = GetFreeSlot()
                    if freeSlot then
                        self:spawn_customer(freeSlot, true) -- true = free play, any order
                        spawnTimer = 0
                    end
                else
                    spawnTimer = 0
                end
            end
        end

        -- Update active slots
        for slot = 1, MAX_COUNTER_SLOTS do
            local c = activeCustomers[slot]
            if c then self:UpdateSlot(slot, c, dt) end
        end

        -- End free play when time runs out
        if free_play_timer >= FREE_PLAY_DURATION then
            print("[CustomerSystem] Free play time limit reached. Ending night.")
            waiting_to_end = true
            end_phase_timer = 0.0
        end

        return
    end

    -- === NORMAL NIGHT LOGIC ===
    local active_count = CountActiveSlots()

    local required = GetRequiredCustomers()

    -- === NO INGREDIENTS + NO ACTIVE CUSTOMERS -> END NIGHT ===
    -- Check regardless of active_count so we catch the case where
    -- the last customer just left and inventory is empty.
    if not HasCraftablePotions() and active_count == 0 then
        if not has_checked_ingredients then
            ingredient_check_timer = ingredient_check_timer + dt
            if ingredient_check_timer >= ingredient_check_delay then
                print("[CustomerSystem] No craftable potions and no active customers. Ending night.")
                self:SpawnLog("No potions available! Going to upgrade phase...")
                Sound.Play("event:/wrongSound")
                waiting_to_end = true
                end_phase_timer = 0.0
                has_checked_ingredients = true
            end
        end
        return
    end

    -- Reset ingredient check timer whenever potions are available
    if HasCraftablePotions() and not has_checked_ingredients then
        has_checked_ingredients = false
        ingredient_check_timer = 0.0
    end

    -- === CHECK: Required customers done -> enter free play ===
    if customers_served >= required and active_count == 0 then
        print("[CustomerSystem] Required customers served. Entering FREE PLAY mode!")
        local night = GetData("currentNight")
        self:SpawnLog("Night " .. night .. " done! Free Play - earn as much as you can!")
        is_free_play = true
        free_play_timer = 0.0
        spawnTimer = effective_spawn_delay
        return
    end

    -- === SPAWN LOGIC ===
    -- Both slots spawn freely based on the timer.
    -- Slot 1 generates its order from craftable potions.
    -- Slot 2 generates its order from what the player actually has in inventory,
    -- so its request accounts for slot 1's unfulfilled order.
    local total_spawned_or_active = customers_served + active_count

    if total_spawned_or_active < required and active_count < MAX_COUNTER_SLOTS then
        spawnTimer = spawnTimer + dt
        if spawnTimer >= effective_spawn_delay then
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
                print("[CustomerSystem] Spawning customer into slot " .. freeSlot)
                self:spawn_customer(freeSlot, false)
                spawnTimer = 0

                -- Reset ingredient check for new customer
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
        local ok, cx, cy, cz = pcall(GetPosition, c.entity)
        if not ok then
            print("[CustomerSystem] Entity " .. tostring(c.entity) .. " no longer valid during walk, clearing slot.")
            CustomerSystem:ClearSlot(slot)
            return
        end

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
            local ok, cx, cy, cz = pcall(GetPosition, c.entity)
            if not ok then
                -- Entity was destroyed mid-frame (e.g. served/left), clear the slot
                print("[CustomerSystem] Entity " .. tostring(c.entity) .. " no longer valid, clearing slot.")
                CustomerSystem:ClearSlot(slot)
                return
            end
            self:UpdateBillboardForSlot(c, px, pz, cx, cy, cz)
        end
    end
end

-- =========================================================
-- SPAWN CUSTOMER INTO A SPECIFIC SLOT
-- is_free_play: if true, generate order regardless of slot rules
-- =========================================================
function CustomerSystem:spawn_customer(slot, is_free_play_spawn)
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

    -- Unique name per slot
    local uniqueName = "Customer_Slot_" .. slot
    SetEntityName(customer, uniqueName)

    -- Store model type (used by Customer.lua for sounds)
    SetData("Type_" .. customer, next_model_index)

    -- Store slot assignment
    SetData("Slot_" .. customer, slot)

    -- Mark slot 2 so Customer.lua knows to check inventory gate
    -- If slot == 2 and not free play, set a flag so GenerateRequest
    -- will use inventory-aware logic
    if slot == 2 and not is_free_play_spawn then
        SetData("CheckInventory_" .. customer, 1)
    else
        SetData("CheckInventory_" .. customer, 0)
    end

    -- Build the slot data table
    activeCustomers[slot] = {
        entity        = customer,
        entity_name   = uniqueName,
        order         = "",
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
    print("[CustomerSystem] Slot " .. slot .. " >> Pathfinding to " .. self.waitPointNames[slot])
    PathfindTo(uniqueName, self.waitPointNames[slot])

    -- Alternate model
    next_model_index = (next_model_index % #self.customer_models) + 1
    print("[CustomerSystem] Spawned customer in slot " .. slot)
end

-- =========================================================
-- ON ARRIVAL (fired by Customer.lua)
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

    -- Currency reward
    local base_amount    = 100
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

    -- Clean up slot - this also resets spawnTimer so next customer comes promptly
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
    local multiplier         = multiplier_int / 100.0
    local penalty_multiplier = 1.0 + ((multiplier - 1.0) * 0.5)
    local actual_penalty     = math.floor(base_penalty * penalty_multiplier)

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
-- CLEAR A SLOT
-- =========================================================
function CustomerSystem:ClearSlot(slot)
    local c = activeCustomers[slot]
    if not c then return end

    -- Destroy progress bar
    self:DestroyBarForSlot(slot, c)

    -- Clean up SetData keys
    -- CustomerOrder is stored in Customer.lua's Lua table, not SetData
    SetData("Slot_"            .. tostring(c.entity), -1)
    SetData("CheckInventory_"  .. tostring(c.entity), 0)

    activeCustomers[slot] = nil

    -- Reset spawn timer so the next customer comes after one full delay
    -- (not an arbitrary leftover value)
    spawnTimer = 0

    print("[CustomerSystem] Slot " .. slot .. " cleared. Spawn timer reset.")
end

-- =========================================================
-- PROGRESS BAR - PER SLOT
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
-- BILLBOARD - PER SLOT
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

    spawnTimer             = 0
    next_model_index       = 1
    ingredient_check_timer = 0.0
    end_phase_timer        = 0.0
    waiting_to_end         = false
    has_checked_ingredients = false
    is_free_play           = false
    free_play_timer        = 0.0

    -- Compute this night's spawn delay
    effective_spawn_delay = ComputeSpawnDelay()

    CustomerSystem:UpdateCurrencyDisplay()
end

-- =========================================================
-- END NIGHT PHASE
-- =========================================================
function CustomerSystem:EndNightPhase()
    print("[CustomerSystem] Ending Night Phase...")
    self:ClearAllLogs()

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