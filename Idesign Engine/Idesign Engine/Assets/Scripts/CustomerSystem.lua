local CustomerSystem = {}

-- =========================================================
-- CONFIGURATION
-- =========================================================
CustomerSystem.customerPrefabGUID = "Customer_Kinematic_Prefab"
CustomerSystem.spawnPointName     = "SpawnPoint"
CustomerSystem.spawn_delay        = 5.0     -- Time Delay per customer

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

CustomerSystem.prefab_frame = "ProgressBarFrame_Prefab"
CustomerSystem.prefab_fill  = "ProgressBarFill_Prefab"
CustomerSystem.tex_green    = "Assets/Textures/green.png"
CustomerSystem.tex_red      = "Assets/Textures/red.png"

-- =========================================================
-- CONSTANTS
-- =========================================================
local MAX_COUNTER_SLOTS      = 2
local PHASE_NIGHT            = 2
local patience_duration      = 120.0
local arrival_threshold      = 1.5
local color_swap_threshold   = 0.3
local bar_length_max         = 0.5
local bar_thickness          = 0.2
local y_offset               = 3.8
local x_offset_right         = -0.5
local tune_x                 = 1.6
local tune_y                 = -0.04
local billboard_depth_adjust = 0.055
local end_phase_delay        = 2.0      -- Time to delay for ""End Night Phase""
local prefab_log             = "CustomerLog_Prefab"


-- =========================================================
-- INTERNAL STATE
-- =========================================================
local score_text_entity = nil
local previousPhase     = -1
local customers_served  = 0
local active_logs       = {}
local spawnTimer        = 0
local navMeshBuilt      = false
local player_id         = nil
local next_model_index  = 1
local end_phase_timer   = 0.0
local waiting_to_end    = false
local waitPointEntities = { nil, nil }
local activeCustomers   = { nil, nil }

-- Pre-built order queue
local orderQueue  = {}
local queueIndex  = 1

-- String store: maps entity ID -> order string
-- Used because SetData only accepts integers
local customerOrderStrings = {}

-- Global getter so Customer.lua can read the order by entity ID
function GetCustomerOrder(entityId)
    return customerOrderStrings[entityId]
end

-- =========================================================
-- BUILD ORDER QUEUE
-- Runs once at night start. Counts ingredients, works out
-- how many of each potion type can be crafted, then
-- greedily picks cheapest potion each round so shared ingredients
-- are correctly deducted and total customer count is maximised.
-- =========================================================
local function BuildOrderQueue()
    orderQueue = {}
    queueIndex = 1

    local cost = GetPotionIngredientCost("mending_potion")
    print("[DEBUG] mending_potion base cost:")
    for _, e in ipairs(cost) do
        print("  itemId=" .. e.itemId .. " count=" .. e.count)
    end

    local craftable = GetCraftablePotionNames()
    print("[DEBUG] craftable list:")
    if not craftable or #craftable == 0 then
        print("[CustomerSystem] BuildOrderQueue: No potions available.")
        return
    end

    -- Snapshot current ingredient counts into a virtual pool
    local pool = {}
    for _, potionName in ipairs(craftable) do
        local cost = GetPotionIngredientCost(potionName)
        if cost then
            for _, entry in ipairs(cost) do
                if not pool[entry.itemId] then
                    pool[entry.itemId] = GetIngredientCount(entry.itemId)
                end
            end
        end

        -- NEW: also seed direct inputs (e.g. iron_bar, copper_bar)
        local direct = GetPotionDirectCost(potionName)
        if direct then
            for _, entry in ipairs(direct) do
                if not pool[entry.itemId] then
                    pool[entry.itemId] = GetIngredientCount(entry.itemId)
                end
            end
        end

        -- Also include finished potions already in inventory
        local potionId = GetItemIdByName(potionName)
        if potionId >= 0 and not pool[potionId] then
            pool[potionId] = GetIngredientCount(potionId)
        end
    end

    -- Can we afford one of this potion from the virtual pool?
    local function CanAffordOne(potionName)
        -- Check if already have the finished potion
        local potionId = GetItemIdByName(potionName)
        if potionId >= 0 and (pool[potionId] or 0) >= 1 then
            return true, "ready"
        end
        -- Check direct recipe inputs first
        local direct = GetPotionDirectCost(potionName)  -- need new binding
        if direct and #direct > 0 then
            local ok = true
            for _, entry in ipairs(direct) do
                if (pool[entry.itemId] or 0) < entry.count then ok = false; break end
            end
            if ok then return true, "direct" end
        end
        -- Fall back to base ingredients
        local cost = GetPotionIngredientCost(potionName)
        if not cost or #cost == 0 then return false, nil end
        for _, entry in ipairs(cost) do
            if (pool[entry.itemId] or 0) < entry.count then return false, nil end
        end
        return true, "craft"
    end

    -- Deduct ingredients from virtual pool for one of this potion
    local function DeductOne(potionName, source)
        if source == "ready" then
            local potionId = GetItemIdByName(potionName)
            pool[potionId] = pool[potionId] - 1
        elseif source == "direct" then
            local direct = GetPotionDirectCost(potionName)
            for _, entry in ipairs(direct) do
                pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
            end
        else
            local cost = GetPotionIngredientCost(potionName)
            for _, entry in ipairs(cost) do
                pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
            end
        end
    end

    -- Total ingredient cost of one potion (for tiebreaking)
    local function TotalCost(potionName)
        local cost = GetPotionIngredientCost(potionName)
        if not cost then return 0 end
        local n = 0
        for _, entry in ipairs(cost) do n = n + entry.count end
        return n
    end

    -- Each round, queue ALL currently affordable potions (deducting as we go).
    -- This ensures shared ingredients are split fairly across potion types.
    local keepGoing = true
    while keepGoing do
        keepGoing = false
        for _, name in ipairs(craftable) do
            local ok, source = CanAffordOne(name)
            if ok then
                table.insert(orderQueue, name)
                DeductOne(name, source)
                keepGoing = true
                print("[CustomerSystem] Queued: " .. name .. " (via " .. source .. ")")
            end
        end
    end

    if #orderQueue == 0 then
        print("[CustomerSystem] BuildOrderQueue: No potions affordable.")
        return
    end

    -- Shuffle for variety (Fisher-Yates)
    for i = #orderQueue, 2, -1 do
        local j = math.random(1, i)
        orderQueue[i], orderQueue[j] = orderQueue[j], orderQueue[i]
    end

    print("[CustomerSystem] Queue built with " .. #orderQueue .. " orders:")
    for i, o in ipairs(orderQueue) do
        print("  [" .. i .. "] " .. o)
    end
end

local function DequeueOrder()
    if queueIndex > #orderQueue then return nil end
    local order = orderQueue[queueIndex]
    queueIndex  = queueIndex + 1
    return order
end

-- Called after a customer completes or fails.
-- Rebuilds remaining queue slots using a virtual ingredient pool
-- so all affordable potions are queued at once, filling both slots.
local function ReplaceFailedOrder()
    local craftable = GetCraftablePotionNames()
    if not craftable or #craftable == 0 then return end

    -- Build virtual pool from current inventory
    local pool = {}
    for _, potionName in ipairs(craftable) do
        local cost = GetPotionIngredientCost(potionName)
        if cost then
            for _, entry in ipairs(cost) do
                if not pool[entry.itemId] then
                    pool[entry.itemId] = GetIngredientCount(entry.itemId)
                end
            end
        end
        -- NEW: also seed direct inputs (e.g. iron_bar, copper_bar)
        local direct = GetPotionDirectCost(potionName)
        if direct then
            for _, entry in ipairs(direct) do
                if not pool[entry.itemId] then
                    pool[entry.itemId] = GetIngredientCount(entry.itemId)
                end
            end
        end
        local potionId = GetItemIdByName(potionName)
        if potionId >= 0 and not pool[potionId] then
            pool[potionId] = GetIngredientCount(potionId)
        end
    end

    -- Deduct ingredients already committed to pending queue entries
    for i = queueIndex, #orderQueue do
        local cost = GetPotionIngredientCost(orderQueue[i])
        if cost then
            for _, entry in ipairs(cost) do
                pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
            end
        end
    end

    -- Deduct ingredients committed to active customers
    for slot = 1, MAX_COUNTER_SLOTS do
        local c = activeCustomers[slot]
        if c and c.order and c.order ~= "" then
            local cost = GetPotionIngredientCost(c.order)
            if cost then
                for _, entry in ipairs(cost) do
                    pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
                end
            end
        end
    end

    -- Can we afford one from the remaining pool?
    local function CanAffordOne(potionName)
        local potionId = GetItemIdByName(potionName)
        if potionId >= 0 and (pool[potionId] or 0) >= 1 then
            return true, "ready"
        end
        local direct = GetPotionDirectCost(potionName)
        if direct and #direct > 0 then
            local ok = true
            for _, entry in ipairs(direct) do
                if (pool[entry.itemId] or 0) < entry.count then ok = false; break end
            end
            if ok then return true, "direct" end
        end
        local cost = GetPotionIngredientCost(potionName)
        if not cost or #cost == 0 then return false, nil end
        for _, entry in ipairs(cost) do
            if (pool[entry.itemId] or 0) < entry.count then return false, nil end
        end
        return true, "craft"
    end

    local function DeductOne(potionName, source)
        if source == "ready" then
            local potionId = GetItemIdByName(potionName)
            pool[potionId] = pool[potionId] - 1
        elseif source == "direct" then
            local direct = GetPotionDirectCost(potionName)
            for _, entry in ipairs(direct) do
                pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
            end
        else
            local cost = GetPotionIngredientCost(potionName)
            for _, entry in ipairs(cost) do
                pool[entry.itemId] = (pool[entry.itemId] or 0) - entry.count
            end
        end
    end

    -- Shuffle craftable list so no potion type is always prioritised
    local shuffled = {}
    for _, name in ipairs(craftable) do table.insert(shuffled, name) end
    for i = #shuffled, 2, -1 do
        local j = math.random(1, i)
        shuffled[i], shuffled[j] = shuffled[j], shuffled[i]
    end

    -- Keep adding affordable potions until pool is exhausted
    local added = 0
    local keepGoing = true
    while keepGoing do
        keepGoing = false
        for _, name in ipairs(shuffled) do
            local ok, source = CanAffordOne(name)
            if ok then
                table.insert(orderQueue, name)
                DeductOne(name, source)
                added = added + 1
                keepGoing = true
                print("[CustomerSystem] ReplaceFailedOrder: Added " .. name .. " (via " .. source .. ")")
            end
        end
    end

    if added == 0 then
        print("[CustomerSystem] ReplaceFailedOrder: No affordable replacements found.")
    else
        print("[CustomerSystem] ReplaceFailedOrder: Added " .. added .. " orders to queue.")
    end
end

-- =========================================================
-- SLOT HELPERS
-- =========================================================
local function CountActiveSlots()
    local n = 0
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] ~= nil then n = n + 1 end
    end
    return n
end

local function GetFreeSlot()
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] == nil then return i end
    end
    return nil
end

-- =========================================================
-- CURRENCY UI
-- =========================================================
function UpdateCurrencyUI()
    if score_text_entity then
        local c = GetCurrency()
        SetTextComponent(score_text_entity, "text", "        :" .. tostring(c))
        print("[UpdateCurrencyUI] updated: " .. c)
    else
        print("[UpdateCurrencyUI] Warning: score_text_entity is nil!")
    end
end

-- =========================================================
-- HUD DATA ACCESSOR
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
-- ON INIT
-- =========================================================
function CustomerSystem:on_init()
    print("[CustomerSystem] >> Initialized.")
    player_id = FindEntityByName("Player")

    for i = 1, MAX_COUNTER_SLOTS do
        waitPointEntities[i] = FindEntityByName(self.waitPointNames[i])
        if waitPointEntities[i] then
            print("[CustomerSystem] Found wait point: " .. self.waitPointNames[i])
        else
            print("[CustomerSystem] WARNING: Missing wait point: " .. self.waitPointNames[i])
        end
    end

    local existing_score = FindEntityByName("ScoreText")
    if existing_score then
        score_text_entity = existing_score
    else
        score_text_entity = SpawnPrefab("ScoreText_Prefab")
        if score_text_entity then
            SetTransformComponent(score_text_entity, "position", {x = 0.010, y = 0.060, z = 1.0})
            SetTextComponent(score_text_entity, "color", {r = 0, g = 0, b = 0})
            SetTextComponent(score_text_entity, "scale", 1.5)
            CustomerSystem:UpdateCurrencyDisplay()
        end
    end

    if not navMeshBuilt then
        local success = BuildNavMesh()
        if success then
            navMeshBuilt = true
            print("[CustomerSystem] NavMesh built.")
        else
            print("[CustomerSystem] ERROR: NavMesh failed!")
        end
    end

    self:ResetNightPhase()
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function CustomerSystem:on_update(dt)

    if score_text_entity then
        if IsEngineState("Play") or IsEngineState("Upgrade") or IsEngineState("Editor") then
            SetTextComponent(score_text_entity, "opacity", 1.0)
        else
            SetTextComponent(score_text_entity, "opacity", 0.0)
        end
    end

    if not navMeshBuilt then return end

    local currentPhase = GetPhase()

    -- Night phase transition: reset and build queue
    if currentPhase == PHASE_NIGHT and previousPhase ~= PHASE_NIGHT then
        print("[CustomerSystem] --- Transition INTO Night Phase ---")
        self:ResetNightPhase()

        -- DEBUG: dump inventory
        print("[DEBUG] iron_ore count: " .. GetIngredientCount(8))
        print("[DEBUG] copper_ore count: " .. GetIngredientCount(9))
        print("[DEBUG] iron_bar count: " .. GetIngredientCount(GetItemIdByName('iron_bar')))

        BuildOrderQueue()
        if #orderQueue == 0 then
            print("[CustomerSystem] No craftable potions. Ending night immediately.")
            waiting_to_end  = true
            end_phase_timer = 0.0
        end
    end
    previousPhase = currentPhase

    if currentPhase ~= PHASE_NIGHT then return end

    -- Delayed end phase
    if waiting_to_end then
        end_phase_timer = end_phase_timer + dt
        if end_phase_timer >= end_phase_delay then
            self:EndNightPhase()
        end
        return
    end

    local active_count = CountActiveSlots()
    local queue_done   = (queueIndex > #orderQueue)

    -- End when queue done and no customers active.
    -- Try ReplaceFailedOrder first in case leftover ingredients allow a new order.
    if queue_done and active_count == 0 then
        ReplaceFailedOrder()
        queue_done = (queueIndex > #orderQueue)
        if queue_done then
            print("[CustomerSystem] All customers served. Ending night.")
            waiting_to_end  = true
            end_phase_timer = 0.0
            return
        end
    end

    -- Spawn next customer if slot free and queue has orders
    if not queue_done and active_count < MAX_COUNTER_SLOTS then
        spawnTimer = spawnTimer + dt
        if spawnTimer >= self.spawn_delay then
            local freeSlot = GetFreeSlot()
            if freeSlot then
                local order = DequeueOrder()
                if order then
                    self:spawn_customer(freeSlot, order)
                    spawnTimer = 0
                end
            end
        end
    else
        -- Reset timer while slots full so next spawn is prompt when one clears
        if active_count >= MAX_COUNTER_SLOTS then
            spawnTimer = 0
        end
    end

    -- Update active slots
    for slot = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[slot] then
            self:UpdateSlot(slot, activeCustomers[slot], dt)
        end
    end
end

-- =========================================================
-- UPDATE SLOT
-- =========================================================
function CustomerSystem:UpdateSlot(slot, c, dt)
    local waitPt = waitPointEntities[slot]

    if not c.is_at_counter then
        local ok, cx, cy, cz = pcall(GetPosition, c.entity)
        if not ok then
            self:ClearSlot(slot)
            return
        end

        local dx    = cx - c.last_pos_x
        local dz    = cz - c.last_pos_z
        local speed = math.sqrt(dx*dx + dz*dz) / dt

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

        if waitPt then
            local wx, wy, wz = GetPosition(waitPt)
            local dist = math.sqrt((cx-wx)^2 + (cy-wy)^2 + (cz-wz)^2)
            if dist < arrival_threshold then
                print("[CustomerSystem] Slot " .. slot .. " arrived!")
                c.is_at_counter = true
                c.is_moving     = false
                SetAnimationSettings(c.entity, 1.0, true, true)
                PlayEntityAnimation(c.entity, c.idle_anim)
                c.patience = patience_duration
                self:SpawnBarForSlot(slot, c)
                FireEvent("OnArrival", c.entity, c.entity, 0)
            end
        end

    else
        c.patience = c.patience - dt

        if c.patience <= 0 then
            print("[CustomerSystem] Slot " .. slot .. " timed out!")
            local timedEntity = c.entity
            -- Apply penalty using native AddCurrency binding
            local penalty = 100
            AddCurrency(-penalty)
            UpdateCurrencyUI()
            CustomerSystem:SpawnLog("Time's up! -" .. penalty .. " gold")
            print("[CustomerSystem] Timeout penalty: -" .. penalty)
            CustomerSystem:ClearSlot(slot, "timeout")
            ReplaceFailedOrder()
            FireEvent("CustomerTimeout", timedEntity, timedEntity, 0)
            FireEvent("CustomerFailed",  timedEntity, timedEntity, 0)
            DestroyEntity(timedEntity)
            return
        end

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
            SetScale(c.bar_fill, pct * bar_length_max, bar_thickness * 0.9, bar_thickness * 0.9)
        end

        if player_id then
            local ok, cx, cy, cz = pcall(GetPosition, c.entity)
            if not ok then
                CustomerSystem:ClearSlot(slot)
                return
            end
            local px, py, pz = GetPosition(player_id)
            self:UpdateBillboardForSlot(c, px, pz, cx, cy, cz)
        end
    end
end

-- =========================================================
-- SPAWN CUSTOMER
-- =========================================================
function CustomerSystem:spawn_customer(slot, order)
    local spawnPoint = FindEntityByName(self.spawnPointName)
    if not spawnPoint then
        print("[CustomerSystem] ERROR: SpawnPoint not found!")
        return
    end

    self:play_doorbell()

    local modelData = self.customer_models[next_model_index]
    local customer  = SpawnPrefab(modelData.prefab, spawnPoint)
    if not customer then
        print("[CustomerSystem] ERROR: Failed to spawn prefab!")
        return
    end

    local uniqueName = "Customer_Slot_" .. slot
    SetEntityName(customer, uniqueName)
    SetData("Type_" .. customer, next_model_index)
    SetData("Slot_" .. customer, slot)
    -- Store order string in Lua table (SetData is int-only)
    customerOrderStrings[customer] = order

    activeCustomers[slot] = {
        entity        = customer,
        entity_name   = uniqueName,
        order         = order,
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

    SetAnimationSettings(customer, 1.0, true, true)
    PlayEntityAnimation(customer, modelData.walk)

    local sx, sy, sz = GetPosition(customer)
    activeCustomers[slot].last_pos_x = sx
    activeCustomers[slot].last_pos_z = sz

    PathfindTo(uniqueName, self.waitPointNames[slot])
    next_model_index = (next_model_index % #self.customer_models) + 1
    print("[CustomerSystem] Spawned slot " .. slot .. " order: " .. order)
end

-- =========================================================
-- ON ARRIVAL
-- =========================================================
function CustomerSystem.OnArrival(senderID, targetID, actionType)
    local slot = GetData("Slot_" .. targetID)
    if not slot or slot < 1 or slot > MAX_COUNTER_SLOTS then return end
    local c = activeCustomers[slot]
    if not c then return end
    print("[CustomerSystem] Slot " .. slot .. " arrived. Order: " .. c.order)
end

-- =========================================================
-- ON CUSTOMER COMPLETE
-- =========================================================
function CustomerSystem.OnCustomerComplete(senderID, targetID, actionType)
    print("[CustomerSystem] CUSTOMER COMPLETE")
    local slot = nil
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] and activeCustomers[i].entity == senderID then
            slot = i
            break
        end
    end
    if not slot then
        print("[CustomerSystem] OnCustomerComplete: entity " .. tostring(senderID) .. " not found in active slots")
        return
    end
    CustomerSystem:ClearSlot(slot, "complete")
    ReplaceFailedOrder()
end

-- =========================================================
-- ON CUSTOMER FAILED
-- =========================================================
function CustomerSystem.OnCustomerFailed(senderID, targetID, actionType)
    print("[CustomerSystem] CUSTOMER FAILED")
    local slot = nil
    for i = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[i] and activeCustomers[i].entity == senderID then
            slot = i
            break
        end
    end
    if not slot then
        print("[CustomerSystem] OnCustomerFailed: entity " .. tostring(senderID) .. " not found in active slots")
        return
    end
    CustomerSystem:ClearSlot(slot, "failed")
    ReplaceFailedOrder()
end

-- =========================================================
-- CLEAR SLOT
-- reason: "complete", "failed", or "timeout"
-- =========================================================
function CustomerSystem:ClearSlot(slot, reason)
    local c = activeCustomers[slot]
    if not c then return end
    customers_served = customers_served + 1
    self:DestroyBarForSlot(slot, c)
    SetData("Slot_" .. tostring(c.entity), -1)
    customerOrderStrings[c.entity] = nil
    activeCustomers[slot] = nil
    print("[CustomerSystem] Slot " .. slot .. " cleared (" .. tostring(reason) .. ").")
end

-- =========================================================
-- PROGRESS BAR
-- =========================================================
function CustomerSystem:SpawnBarForSlot(slot, c)
    c.bar_frame = SpawnPrefab(self.prefab_frame, c.entity)
    if c.bar_frame then SetScale(c.bar_frame, bar_length_max, bar_thickness, bar_thickness * 5) end
    c.bar_fill = SpawnPrefab(self.prefab_fill, c.entity)
    if c.bar_fill then SetScale(c.bar_fill, bar_length_max, bar_thickness, bar_thickness) end
    SetTexture(c.bar_fill, self.tex_green)
    c.bar_state = "green"
end

function CustomerSystem:DestroyBarForSlot(slot, c)
    if c.bar_frame then DestroyEntity(c.bar_frame); c.bar_frame = nil end
    if c.bar_fill  then DestroyEntity(c.bar_fill);  c.bar_fill  = nil end
    c.bar_state = "green"
end

-- =========================================================
-- BILLBOARD
-- =========================================================
function CustomerSystem:UpdateBillboardForSlot(c, px, pz, cx, cy, cz)
    local angle_rad = math.atan2(px - cx, pz - cz)
    local angle_deg = math.deg(angle_rad)
    local frame_x   = cx + x_offset_right
    local frame_y   = cy + y_offset
    local frame_z   = cz

    if c.bar_frame then
        SetPosition(c.bar_frame, frame_x, frame_y, frame_z)
        SetRotation(c.bar_frame, 0.0, angle_deg, 0.0)
    end

    if c.bar_fill then
        SetRotation(c.bar_fill, 0.0, angle_deg + 180.0, 0.0)
        local right_angle   = angle_rad + (math.pi / 2.0)
        local side_offset_x = (bar_length_max * tune_x) * math.sin(right_angle)
        local side_offset_z = (bar_length_max * tune_x) * math.cos(right_angle)
        SetPosition(c.bar_fill,
            frame_x + side_offset_x + billboard_depth_adjust * math.sin(angle_rad),
            frame_y + tune_y,
            frame_z + side_offset_z + billboard_depth_adjust * math.cos(angle_rad))
    end
end

-- =========================================================
-- RESET
-- =========================================================
function CustomerSystem:ResetNightPhase()
    print("[CustomerSystem] Resetting...")
    self:ClearAllLogs()
    customers_served = 0
    SetData("fixedQueue", 0)

    for slot = 1, MAX_COUNTER_SLOTS do
        if activeCustomers[slot] then
            self:DestroyBarForSlot(slot, activeCustomers[slot])
            DestroyEntity(activeCustomers[slot].entity)
            activeCustomers[slot] = nil
        end
    end

    orderQueue            = {}
    queueIndex            = 1
    customerOrderStrings  = {}
    spawnTimer       = 0
    next_model_index = 1
    end_phase_timer  = 0.0
    waiting_to_end   = false

    CustomerSystem:UpdateCurrencyDisplay()
end

-- =========================================================
-- END NIGHT
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
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then multiplier_int = 100; SetData("currencyMultiplier", 100) end
    local multiplier      = multiplier_int / 100.0
    local modified_amount = math.floor(amount * multiplier)
    local new_currency    = math.max(0, GetCurrency() + modified_amount)
    SetCurrency(new_currency)
    print("[CustomerSystem] Currency +" .. modified_amount .. " total: " .. new_currency)
    CustomerSystem:UpdateCurrencyDisplay()
end

function CustomerSystem:UpdateCurrencyDisplay() UpdateCurrencyUI() end
function CustomerSystem:GetScore() return GetScore() end

-- =========================================================
-- LOGS
-- =========================================================
function CustomerSystem:SpawnLog(message)
    local e = SpawnPrefab(prefab_log)
    if e then SetTextComponent(e, "text", message); table.insert(active_logs, e) end
end

function CustomerSystem:ClearAllLogs()
    for _, e in ipairs(active_logs) do if e then DestroyEntity(e) end end
    active_logs = {}
end

-- =========================================================
-- DOORBELL
-- =========================================================
function CustomerSystem:play_doorbell()
    local f = FindEntityByName("Frog")
    if f then PlayEntitySound(f) end
end

return CustomerSystem