local CustomerSystem = {}

-- =========================================================
-- CONFIGURATION
-- =========================================================
CustomerSystem.customerPrefabGUID = "Customer_Kinematic_Prefab"
CustomerSystem.spawnPointName = "SpawnPoint"
CustomerSystem.waitPointName = "CustomerWaitingPoint"
CustomerSystem.spawn_delay = 3.0

CustomerSystem.customer_models = {
    { 
        prefab = "Customer_Kinematic_Prefab",       -- Model A
        walk = "Armature|WalkAnimation",
        idle = "Armature|IdleAnimation"
    },
    { 
        prefab = "Customer_Kinematic_2_Prefab",     -- Model B
        walk = "Armature|WalkAnimation",    -- Change this if Model B has different anim names
        idle = "Armature|IdleAnimation"
    }
}    -- Starts at 1, switches to 2, then back to 1

-- [PROGRESS BAR CONFIG]
CustomerSystem.prefab_frame = "ProgressBarFrame_Prefab"
CustomerSystem.prefab_fill  = "ProgressBarFill_Prefab"
CustomerSystem.tex_green = "Assets/Textures/green.png" 
CustomerSystem.tex_red   = "Assets/Textures/red.png"

-- [INGREDIENT CHECK TIMER]
local ingredient_check_timer = 0.0
local ingredient_check_delay = 2.0      -- Wait 2 seconds after spawn
local end_phase_timer = 0.0
local end_phase_delay = 2.0             -- Wait 2 seconds before upgrade
local waiting_to_end = false            -- Flag: waiting to end phase
local has_checked_ingredients = false   -- Flag: already checked once

-- Log prefab
local prefab_log = "CustomerLog_Prefab"

-- Visual Settings
local color_swap_threshold = 0.3
local bar_length_max = 0.5
local bar_thickness  = 0.2
local y_offset       = 3.8    
local x_offset_right = -0.5    
local tune_x = 1.6
local tune_y = -0.04
local billboard_depth_adjust = 0.055

-- Patience Settings
local patience_duration = 120.0 
local current_patience = 0.0
local arrival_threshold = 1.5 

-- [SCORE TRACKING]
local score_text_entity = nil

-- [PHASE MANAGEMENT]
local PHASE_NIGHT = 2
local MAX_CUSTOMERS = 3
local customers_served = 0

-- =========================================================
-- INTERNAL STATE
-- =========================================================
local activeCustomer = nil
local waiting_queue = {} -- [NEW] Stores customers waiting at the door
local waitPointEntity = nil 
local hasSpawned = false -- Used to track if the ACTIVE slot is filled
local is_at_counter = false 
local spawnTimer = 0
local navMeshBuilt = false
local player_id = nil
local entity_frame = nil
local entity_fill = nil
local current_bar_state = "green"
local previousPhase = -1 
local active_logs = {} -- Track spawned logs

-- [ANIMATION STATE]
local current_idle_anim = ""    -- Stores the idle anim for the active customer
local current_walk_anim = ""    -- Stores the walk anim for the active customer
local next_model_index = 2      -- Tracks whose turn it is (1 or 2)
local last_pos_x = 0
local last_pos_z = 0
local is_moving_state = true    -- To prevent spamming PlayAnimation

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

function CustomerSystem:on_init()
    print("[CustomerSystem] >> Initialized.")
    
    player_id = FindEntityByName("Player")
    waitPointEntity = FindEntityByName(self.waitPointName)
    
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
    
    CustomerSystemNotifyComplete = function()
        self:OnCustomerComplete()
    end
end
function CustomerSystem:on_update(dt)
    local queue_count = #waiting_queue
    local has_active = (activeCustomer ~= nil)
    
    -- Update score text visibility based on engine state
    if score_text_entity then
        if IsEngineState("Play") or IsEngineState("Upgrade") or IsEngineState("Editor") then
            SetTextComponent(score_text_entity, "opacity", 1.0)
        else
            SetTextComponent(score_text_entity, "opacity", 0.0)
        end
    end

    if not navMeshBuilt then return end
    
    local currentPhase = GetPhase()
    
    if currentPhase == PHASE_NIGHT and previousPhase ~= PHASE_NIGHT then
        print("[CustomerSystem] --- Transition INTO Night Phase ---")
        self:ResetNightPhase()
    end
    previousPhase = currentPhase
    
    if currentPhase ~= PHASE_NIGHT then return end
    
    -- === DELAYED END PHASE LOGIC ===
    if waiting_to_end then
        end_phase_timer = end_phase_timer + dt
        if end_phase_timer >= end_phase_delay then
            print("[CustomerSystem] Timer complete. Ending night phase now.")
            self:EndNightPhase()
            return
        end
        -- Still waiting, don't do anything else
        return
    end
    
    -- === INGREDIENT CHECK (2 seconds after active customer exists) ===
    local total_alive_count = #waiting_queue
    if activeCustomer then total_alive_count = total_alive_count + 1 end
    
    if activeCustomer and not has_checked_ingredients then
        ingredient_check_timer = ingredient_check_timer + dt
        
        if ingredient_check_timer >= ingredient_check_delay then
            -- Time to check ingredients
            if not HasCraftablePotions() then
                print("[CustomerSystem] No craftable potions detected!")
                self:SpawnLog("No potions available! Starting upgrade phase...")
                Sound.Play("event:/wrongSound")  -- Optional: play warning sound
                
                -- Start end phase countdown
                waiting_to_end = true
                end_phase_timer = 0.0
                has_checked_ingredients = true
                return
            else
                -- Potions ARE craftable, mark as checked and continue normally
                has_checked_ingredients = true
                print("[CustomerSystem] Ingredient check passed. Continuing night phase.")
            end
        end
    end
    
    -- === NORMAL END CHECK: Max customers reached ===
    if customers_served >= MAX_CUSTOMERS and total_alive_count == 0 then
        print("[CustomerSystem] Max customers reached. Ending Night Phase.")
        self:EndNightPhase()
        return
    end
    
   -- === SPAWN LOGIC (with pre-spawn ingredient check) ===
if customers_served < MAX_CUSTOMERS and total_alive_count == 0 then
    spawnTimer = spawnTimer + dt
    if spawnTimer >= self.spawn_delay then
        -- [PRE-SPAWN CHECK] Don't spawn if no potions available
        if not HasCraftablePotions() then
            print("[CustomerSystem] Cannot spawn - no craftable potions!")
            self:SpawnLog("No potions available! Starting upgrade phase...")
            Sound.Play("event:/wrongSound")
            
            -- Start end phase countdown
            waiting_to_end = true
            end_phase_timer = 0.0
            return
        end
        
        print("[CustomerSystem] Scene empty. Spawning next customer...")
        self:spawn_customer()
        spawnTimer = 0
        
        -- Reset ingredient check for new customer
        ingredient_check_timer = 0.0
        has_checked_ingredients = false
    end
else
    spawnTimer = 0 
end

    -- 2. CUSTOMER ACTIVE LOGIC
    if activeCustomer then
        
        -- [VELOCITY CHECK]
        if not is_at_counter then
            local cx, cy, cz = GetPosition(activeCustomer)
            
            -- Calculate distance moved since last frame
            local dx = cx - last_pos_x
            local dz = cz - last_pos_z
            local dist_moved = math.sqrt(dx*dx + dz*dz)
            local speed = dist_moved / dt 

            -- Threshold: If speed < 0.1 units/sec, they are effectively stopped
            if speed < 0.1 then
                if is_moving_state == true then
                    print("[CustomerSystem] Blocked! Switching to Idle.")
                    SetAnimationSettings(activeCustomer, 1.0, true, true)
                    PlayEntityAnimation(activeCustomer, current_idle_anim)
                    is_moving_state = false
                end
            else
                if is_moving_state == false then
                    print("[CustomerSystem] Moving! Switching to Walk.")
                    SetAnimationSettings(activeCustomer, 1.0, true, true)
                    PlayEntityAnimation(activeCustomer, current_walk_anim)
                    is_moving_state = true
                end
            end

            -- Update last position for next frame
            last_pos_x = cx
            last_pos_z = cz
        end

        -- CHECK IF ARRIVED
        if not is_at_counter then
            if waitPointEntity then
                local cx, cy, cz = GetPosition(activeCustomer)
                local wx, wy, wz = GetPosition(waitPointEntity)
                
                local dist = math.sqrt((cx - wx)^2 + (cy - wy)^2 + (cz - wz)^2)
                
                if dist < arrival_threshold then
                    print("[CustomerSystem] >> ARRIVED! Switching to Idle...")
                    is_at_counter = true
                    is_moving_state = false 
                    
                    SetAnimationSettings(activeCustomer, 1.0, true, true)
                    PlayEntityAnimation(activeCustomer, current_idle_anim)
                    
                    current_patience = patience_duration
                    self:SpawnBar(activeCustomer)
                    FireEvent("OnArrival", activeCustomer, activeCustomer, 0)
                end
            end
        else
            -- [ARRIVED] HANDLE PATIENCE
            if entity_frame and entity_fill then
                current_patience = current_patience - dt
                
                if current_patience <= 0 then       
                    print("[CustomerSystem] Time expired! Customer leaving.")
                    CustomerSystem:SpawnLog("Time's up!")
                    if activeCustomer then
                        FireEvent("CustomerTimeout", activeCustomer, activeCustomer, 0)
                        FireEvent("CustomerFailed", activeCustomer, activeCustomer, 0)
                    end
                    return
                end
                
                -- Visual updates (bar color/scale)
                local pct = current_patience / patience_duration
                if pct <= color_swap_threshold then
                    if current_bar_state == "green" then
                        SetTexture(entity_fill, self.tex_red)
                        current_bar_state = "red"
                    end
                else
                    if current_bar_state == "red" then
                        SetTexture(entity_fill, self.tex_green)
                        current_bar_state = "green"
                    end
                end
                local current_length = pct * bar_length_max
                SetScale(entity_fill, current_length, bar_thickness * 0.9, bar_thickness * 0.9)

                if player_id then
                    local px, py, pz = GetPosition(player_id)
                    local cx, cy, cz = GetPosition(activeCustomer)
                    self:UpdateBillboard(px, pz, cx, cy, cz)
                end
            end
        end
    end
end

function CustomerSystem:spawn_customer()
    local spawnPoint = FindEntityByName(self.spawnPointName)
    if spawnPoint == nil then return end
    
    self:play_doorbell()

    local modelData = self.customer_models[next_model_index]
    local customer = SpawnPrefab(modelData.prefab, spawnPoint)
    
    if customer == nil then return end

    SetData("Type_" .. customer, next_model_index)
    
    local customerData = {
        entity = customer,
        idle = modelData.idle,
        walk = modelData.walk
    }

    -- Go straight to the counter since we know the scene is empty
    self:SetActiveCustomer(customerData)

    -- Flip index for next time
    next_model_index = (next_model_index % #self.customer_models) + 1
end

-- [NEW] Helper to activate a customer (make them walk to counter)
function CustomerSystem:SetActiveCustomer(data)
    activeCustomer = data.entity
    current_idle_anim = data.idle
    current_walk_anim = data.walk
    
    hasSpawned = true
    is_at_counter = false
    
    -- Play Walk
    SetAnimationSettings(activeCustomer, 1.0, true, true)
    PlayEntityAnimation(activeCustomer, current_walk_anim)

    -- Reset Movement State
    local sx, sy, sz = GetPosition(activeCustomer)
    last_pos_x = sx
    last_pos_z = sz
    is_moving_state = true

    print("[CustomerSystem] >> Finding path to " .. self.waitPointName)
    PathfindTo("Customer_Kinematic", self.waitPointName)
end

-- [NEW] Helper to check if someone is waiting
function CustomerSystem:CheckQueue()
    if #waiting_queue > 0 then
        print("[CustomerSystem] Active customer left. Promoting from queue...")
        local nextData = table.remove(waiting_queue, 1)
        self:SetActiveCustomer(nextData)
    else
        print("[CustomerSystem] Queue empty. Waiting for next spawn.")
        hasSpawned = false -- Allow logic to know active slot is empty
    end
end

-- =========================================================
-- HELPER FUNCTIONS
-- =========================================================

function CustomerSystem:play_doorbell()
    local frogEntity = FindEntityByName("Frog")
    if frogEntity then
        PlayEntitySound(frogEntity)
    end
end 

function CustomerSystem:SpawnLog(message)
    local logEntity = SpawnPrefab(prefab_log)
    if logEntity then
        SetTextComponent(logEntity, "text", message)
        table.insert(active_logs, logEntity)    -- Track active logEntity
    end
end

    -- Function to clear all logs
function CustomerSystem:ClearAllLogs()
    for i, logEntity in ipairs(active_logs) do
        if logEntity then
            DestroyEntity(logEntity)
        end
    end
    active_logs = {}
end

function CustomerSystem:AddCurrency(amount)
    local current_currency = GetCurrency()
    
    -- Get multiplier from C++ GameSystem (stored as int, 100 = 1.0x)
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then 
        multiplier_int = 100  -- Default to 1.0x
        SetData("currencyMultiplier", 100)
    end
    
    -- Convert to decimal (100 = 1.0x, 200 = 2.0x, 150 = 1.5x)
    local multiplier = multiplier_int / 100.0
    
    local modified_amount = math.floor(amount * multiplier)
    local new_currency = current_currency + modified_amount
    if new_currency < 0 then new_currency = 0 end
    
    SetCurrency(new_currency)
    print("[CustomerSystem] Currency changed by " .. modified_amount .. 
          " (" .. amount .. " x " .. multiplier .. "x), new currency: " .. new_currency)

    CustomerSystem:UpdateCurrencyDisplay()
end

function CustomerSystem:UpdateCurrencyDisplay()
    UpdateCurrencyUI()
end

function CustomerSystem:GetScore()
    return GetScore()
end

function CustomerSystem:ResetNightPhase()
    print("[CustomerSystem] Resetting night phase state...")
    self:ClearAllLogs() -- Clear logs
    customers_served = 0
    SetData("fixedQueue", 0)
    
    -- Clean up active
    if activeCustomer then 
        DestroyEntity(activeCustomer) 
        activeCustomer = nil
    end
    
    -- Clean up queue
    for i, data in ipairs(waiting_queue) do
        if data.entity then DestroyEntity(data.entity) end
    end
    waiting_queue = {}
    
    self:DestroyBar()
    hasSpawned = false
    is_at_counter = false
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    
    -- [NEW] Reset ingredient check timers
    ingredient_check_timer = 0.0
    end_phase_timer = 0.0
    waiting_to_end = false
    has_checked_ingredients = false
    
    CustomerSystem:UpdateCurrencyDisplay()
end

function CustomerSystem:EndNightPhase()
    print("[CustomerSystem] Ending Night Phase...")
    self:ClearAllLogs() -- Clear logs
    self:DestroyBar()
    GameStateEndNightPhase()
end

function CustomerSystem.OnCustomerComplete(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER COMPLETE ==========")
    -- Calculate actual currency earned (with multiplier)
    local base_amount = 100
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then 
        multiplier_int = 100  -- Default to 1.0x
    end
    
    local multiplier = multiplier_int / 100.0
    local actual_earned = math.floor(base_amount * multiplier)
    
    -- Award the currency
    CustomerSystem:AddCurrency(base_amount)  -- This uses the multiplier internally

    -- Show dynamic log message
    if multiplier > 1.0 then
        -- Show multiplier in message
        CustomerSystem:SpawnLog("Customer served! +" .. actual_earned .. " currency (" .. multiplier .. "x)")
    else
        -- Normal message
        CustomerSystem:SpawnLog("Customer served! +" .. actual_earned .. " currency")
    end

    customers_served = customers_served + 1
    CustomerSystem:DestroyBar()
    if activeCustomer then DestroyEntity(activeCustomer) end
    activeCustomer = nil
    
    is_at_counter = false 
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    
    -- [NEW] Check if anyone is waiting
    CustomerSystem:CheckQueue()
end

function CustomerSystem.OnCustomerFailed(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER FAILED ==========")
    
    local base_penalty = 100  -- Base penalty amount (positive number)
    
    -- Get multiplier
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then multiplier_int = 100 end
    local multiplier = multiplier_int / 100.0
    
    -- Calculate penalty multiplier (50% of the bonus)
    local penalty_multiplier = 1.0 + ((multiplier - 1.0) * 0.5)
    local actual_penalty = math.floor(base_penalty * penalty_multiplier)
    
    -- Deduct currency (using negative value)
    local current_currency = GetCurrency()
    local new_currency = current_currency - actual_penalty
    if new_currency < 0 then new_currency = 0 end
    SetCurrency(new_currency)
    
    CustomerSystem:UpdateCurrencyDisplay()

    -- Show dynamic penalty message
    if penalty_multiplier > 1.0 then
        CustomerSystem:SpawnLog("Customer left angry! -" .. actual_penalty .. " currency (" .. penalty_multiplier .. "x)")
    else
        CustomerSystem:SpawnLog("Customer left angry! -" .. actual_penalty .. " currency")
    end
    
    print("[CustomerSystem] Penalty: -" .. actual_penalty .. " (base " .. base_penalty .. " x " .. penalty_multiplier .. "x)")
    
    customers_served = customers_served + 1
    CustomerSystem:DestroyBar()
    if activeCustomer then DestroyEntity(activeCustomer) end
    activeCustomer = nil
    
    is_at_counter = false
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    
    -- [NEW] Check if anyone is waiting
    CustomerSystem:CheckQueue()
end

function CustomerSystem:SpawnBar(parentID)
    entity_frame = SpawnPrefab(self.prefab_frame, parentID)
    if entity_frame then
        SetScale(entity_frame, bar_length_max, bar_thickness, bar_thickness * 5)
    end
    entity_fill = SpawnPrefab(self.prefab_fill, parentID)
    if entity_fill then
        SetScale(entity_fill, bar_length_max, bar_thickness, bar_thickness) 
    end
    SetTexture(entity_fill, self.tex_green)
end

function CustomerSystem:DestroyBar()
    if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
    if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
    current_bar_state = "green"
end

function CustomerSystem:UpdateBillboard(px, pz, cx, cy, cz)
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz) 
    local angle_deg = math.deg(angle_rad)

    local frame_pos_x = cx + x_offset_right
    local frame_pos_y = cy + y_offset
    local frame_pos_z = cz 

    if entity_frame then
        SetPosition(entity_frame, frame_pos_x, frame_pos_y, frame_pos_z)
        SetRotation(entity_frame, 0.0, angle_deg, 0.0)
    end

    if entity_fill then
        SetRotation(entity_fill, 0.0, angle_deg + 180.0, 0.0)
        
        local right_angle = angle_rad + (math.pi / 2.0)
        local side_dist = (bar_length_max * tune_x)
        local side_offset_x = side_dist * math.sin(right_angle)
        local side_offset_z = side_dist * math.cos(right_angle)
        local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
        local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)

        local fill_x = frame_pos_x + side_offset_x + depth_offset_x
        local fill_y = frame_pos_y + tune_y
        local fill_z = frame_pos_z + side_offset_z + depth_offset_z

        SetPosition(entity_fill, fill_x, fill_y, fill_z)
    end
end

return CustomerSystem