local CustomerSystem = {}

-- =========================================================
-- CONFIGURATION
-- =========================================================
CustomerSystem.customerPrefabGUID = "Customer_Kinematic_Prefab"
CustomerSystem.spawnPointName = "SpawnPoint"
CustomerSystem.waitPointName = "CustomerWaitingPoint"
CustomerSystem.spawn_delay = 3.0

-- [PROGRESS BAR CONFIG]
CustomerSystem.prefab_frame = "ProgressBarFrame_Prefab"
CustomerSystem.prefab_fill  = "ProgressBarFill_Prefab"
CustomerSystem.tex_green = "Assets/Textures/green.png" 
CustomerSystem.tex_red   = "Assets/Textures/red.png"

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
local MAX_CUSTOMERS = 2
local customers_served = 0

-- =========================================================
-- INTERNAL STATE
-- =========================================================
local activeCustomer = nil
local waitPointEntity = nil 
local hasSpawned = false
local is_at_counter = false 
local spawnTimer = 0
local navMeshBuilt = false
local player_id = nil
local entity_frame = nil
local entity_fill = nil
local current_bar_state = "green"
local previousPhase = -1 
local active_logs = {} -- Track spawned logs

-- =========================================================
-- GLOBAL HELPER FUNCTIONS
-- =========================================================
function UpdateCurrencyUI()
    if score_text_entity then
        local current_currency = GetCurrency()
        local currency_text = "Currency: " .. tostring(current_currency)
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
    
    score_text_entity = SpawnPrefab("ScoreText_Prefab")
    if score_text_entity then
        CustomerSystem:UpdateCurrencyDisplay()
    else
        print("[CustomerSystem] ERROR: Failed to spawn ScoreText prefab!")
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
    if not navMeshBuilt then return end
    
    local currentPhase = GetPhase()
    
    if currentPhase == PHASE_NIGHT and previousPhase ~= PHASE_NIGHT then
        print("[CustomerSystem] --- Transition INTO Night Phase ---")
        self:ResetNightPhase()
    end
    previousPhase = currentPhase
    
    if currentPhase ~= PHASE_NIGHT then return end
    
    if customers_served >= MAX_CUSTOMERS then
        print("[CustomerSystem] Max customers reached. Ending Night Phase.")
        self:EndNightPhase()
        return
    end
    
    -- 1. SPAWN LOGIC
    if not hasSpawned then
        spawnTimer = spawnTimer + dt
        if spawnTimer >= self.spawn_delay then
            print("[CustomerSystem] Spawn timer reached (" .. self.spawn_delay .. "s). Spawning now...")
            self:spawn_customer()
            spawnTimer = 0
        end
    else
        -- 2. CUSTOMER ACTIVE LOGIC
        if activeCustomer then
            
            -- CHECK IF ARRIVED
            if not is_at_counter then
                if waitPointEntity then
                    local cx, cy, cz = GetPosition(activeCustomer)
                    local wx, wy, wz = GetPosition(waitPointEntity)
                    
                    local dist = math.sqrt((cx - wx)^2 + (cy - wy)^2 + (cz - wz)^2)
                    
                    if dist < arrival_threshold then
                        print("[CustomerSystem] >> ARRIVED! Distance " .. dist .. " < " .. arrival_threshold)
                        
                        is_at_counter = true
                        
                        -- [REMOVED] StopAnimation call (Prevent Crash)
                        StopAnimation(activeCustomer)
                        
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
end

function CustomerSystem:spawn_customer()
    local spawnPoint = FindEntityByName(self.spawnPointName)
    if spawnPoint == nil then 
        print("[CustomerSystem] ERROR: SpawnPoint not found!")
        return 
    end
    
    self:play_doorbell()

    print("[CustomerSystem] Spawning Prefab: " .. self.customerPrefabGUID)
    local customer = SpawnPrefab(self.customerPrefabGUID, spawnPoint)
    
    if customer == nil then 
        print("[CustomerSystem] ERROR: Failed to spawn customer entity!")
        return 
    end
    
    print("[CustomerSystem] >> Spawned customer ID: " .. customer)
    
    activeCustomer = customer
    hasSpawned = true
    is_at_counter = false 
    

    print("[CustomerSystem] >> Finding path to " .. self.waitPointName .. " for entity " .. customer)
    
    -- Use the Entity ID, not the string name
    PathfindTo("Customer_Kinematic", self.waitPointName)
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
    SetData("fixedQueue", 2)
    if activeCustomer then 
        DestroyEntity(activeCustomer) 
        activeCustomer = nil
    end
    self:DestroyBar()
    hasSpawned = false
    is_at_counter = false
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    CustomerSystem:UpdateCurrencyDisplay()  -- Changed from UpdateScoreDisplay
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
    hasSpawned = false
    is_at_counter = false 
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
end

function CustomerSystem.OnCustomerFailed(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER FAILED ==========")
    
    local base_penalty = 100  -- Base penalty amount (positive number)
    
    -- Get multiplier
    local multiplier_int = GetData("currencyMultiplier")
    if multiplier_int <= 0 then multiplier_int = 100 end
    local multiplier = multiplier_int / 100.0
    
    -- Calculate penalty multiplier (50% of the bonus)
    -- If multiplier is 2.0x, penalty multiplier is 1.5x
    -- Formula: 1.0 + ((multiplier - 1.0) * 0.5)
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
    hasSpawned = false
    is_at_counter = false
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
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