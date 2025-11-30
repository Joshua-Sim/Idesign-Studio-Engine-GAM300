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


-- Threshold to turn red (0.3 = 30% time remaining)
local color_swap_threshold = 0.3

-- Visual Settings
local bar_length_max = 0.5
local bar_thickness  = 0.2
local y_offset       = 3.5    
local x_offset_right = -0.5    

-- [OFFSETS FROM CAULDRON SCRIPT]
local tune_x = 1.6
local tune_y = -0.04

-- [BILLBOARD DEPTH ADJUST]
local billboard_depth_adjust = 0.055

-- Patience Settings
local patience_duration = 120.0 
local current_patience = 0.0

-- Arrival Settings
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

-- Bar Entity IDs
local entity_frame = nil
local entity_fill = nil

local current_bar_state = "green"
local previousPhase = -1 

function CustomerSystem:on_init()
    print("[CustomerSystem] Initialized.")
    
    player_id = FindEntityByName("Player")
    waitPointEntity = FindEntityByName(self.waitPointName)
    
    -- Spawn score text prefab
    score_text_entity = SpawnPrefab("ScoreText_Prefab")
    if score_text_entity then
        print("[CustomerSystem] Spawned ScoreText entity: " .. score_text_entity)
        CustomerSystem:UpdateScoreDisplay()
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
    
    -- Keep the old callback for backwards compatibility
    CustomerSystemNotifyComplete = function()
        print("[CustomerSystem] Customer completed order (legacy callback).")
        self:OnCustomerComplete()
    end
end

function CustomerSystem:on_update(dt)
    if not navMeshBuilt then return end
    
    local currentPhase = GetPhase()
    
    -- Phase transition detection
    if currentPhase == PHASE_NIGHT and previousPhase ~= PHASE_NIGHT then
        print("[CustomerSystem] --- Detected transition INTO Night Phase. Resetting state. ---")
        self:ResetNightPhase()
    end
    
    previousPhase = currentPhase
    
    -- Check if we're in Night phase
    if currentPhase ~= PHASE_NIGHT then
        return
    end
    
    -- Check if we've reached max customers
    if customers_served >= MAX_CUSTOMERS then
        print("[CustomerSystem] Max customers reached (" .. MAX_CUSTOMERS .. "), ending night phase...")
        self:EndNightPhase()
        return
    end
    
    -- 1. SPAWN LOGIC
    if not hasSpawned then
        spawnTimer = spawnTimer + dt
        if spawnTimer >= self.spawn_delay then
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
                        print("[CustomerSystem] Customer arrived at counter!")
                        is_at_counter = true
                        
                        current_patience = patience_duration
                        
                        self:SpawnBar(activeCustomer)
                        
                        -- Fire arrival event to customer
                        FireEvent("OnArrival", activeCustomer, activeCustomer, 0)
                    end
                end
            else
                -- [ARRIVED] HANDLE PATIENCE & BILLBOARD
                if entity_frame and entity_fill then
                    
                    -- Update Timer
                    current_patience = current_patience - dt
                    if current_patience <= 0 then     
                        current_patience = 0
                        
                        -- TIME EXPIRED - Fire CustomerFailed event
                        print("[CustomerSystem] Time expired! Firing CustomerFailed event.")
                        CustomerSystem:SpawnLog("Time's up! Customer left.")

                        if activeCustomer then

                                 -- First, let customer cleanup and spawn log
                            FireEvent("CustomerTimeout", activeCustomer, activeCustomer, 0)

                            FireEvent("CustomerFailed", activeCustomer, activeCustomer, 0)
                        end
                        return
                    end
                    
                    -- Update Scale
                    local pct = current_patience / patience_duration

                    -- Color swap based on threshold
                    if pct <= color_swap_threshold then
                        if current_bar_state == "green" then
                            print("[CustomerSystem] Patience Low! Switching to Red.")
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

                    -- Update Billboard
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
    if spawnPoint == nil then return end
    
    self:play_doorbell()

    local customer = SpawnPrefab(self.customerPrefabGUID, spawnPoint)
    if customer == nil then return end
    
    print("[CustomerSystem] Spawned customer: " .. customer)
    
    activeCustomer = customer
    hasSpawned = true
    is_at_counter = false 
    
    PathfindTo("Customer_Kinematic", self.waitPointName)
end

-- =========================================================
-- SCORE MANAGEMENT
-- =========================================================

function CustomerSystem:AddScore(amount)
    -- Get current engine score
    local current_score = GetScore()
    
    -- Add the delta
    local new_score = current_score + amount
    
    -- Clamp to minimum of 0
    if new_score < 0 then
        new_score = 0
    end
    
    -- Set the new score in engine
    SetScore(new_score)
    
    print("[CustomerSystem] Score changed by " .. amount .. ", new score: " .. new_score)
    CustomerSystem:UpdateScoreDisplay()
end

function CustomerSystem:UpdateScoreDisplay()
    if score_text_entity then
        -- Get score from engine
        local current_score = GetScore()
        local score_text = "Score: " .. tostring(current_score)
        SetTextComponent(score_text_entity, "text", score_text)
    end
end

function CustomerSystem:GetScore()
    -- Return engine score
    return GetScore()
end

-- =========================================================
-- PHASE MANAGEMENT
-- =========================================================

function CustomerSystem:ResetNightPhase()
    print("[CustomerSystem] Resetting night phase state...")
    
    customers_served = 0
    
    -- Reset fixed queue to 2 for ordered customer requests
    SetData("fixedQueue", 2)
    print("[CustomerSystem] Set fixedQueue to 2 (strength -> vitality)")
    
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
    
    -- Update score display (reads from engine)
    CustomerSystem:UpdateScoreDisplay()
    
    print("[CustomerSystem] Night phase reset complete. Customers served: " .. customers_served)
end

function CustomerSystem:EndNightPhase()
    print("[CustomerSystem] ========== ENDING NIGHT PHASE ==========")
    
    local final_score = GetScore()
    print("[CustomerSystem] Final Score: " .. final_score)
    print("[CustomerSystem] Customers Served: " .. customers_served .. "/" .. MAX_CUSTOMERS)
    
    self:DestroyBar()
    
    GameStateEndNightPhase()
    
    print("[CustomerSystem] Night phase ended, transitioning to upgrade phase...")
end

-- =========================================================
-- EVENT HANDLERS (called via EventReceiverComponent)
-- =========================================================

-- Called when customer successfully completes order
function CustomerSystem.OnCustomerComplete(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER COMPLETE ==========")
    print("[CustomerSystem] Event - Sender: " .. tostring(senderID) .. ", Target: " .. tostring(targetID))
    
    -- Add score reward
    CustomerSystem:AddScore(10)
    print("[CustomerSystem] +10 score reward!")
    
    -- Spawn success log
    CustomerSystem:SpawnLog("Customer served! +10 points")
    
    -- Increment customer count
    customers_served = customers_served + 1
    print("[CustomerSystem] Customers served: " .. customers_served .. "/" .. MAX_CUSTOMERS)
    
    -- Clean up
    CustomerSystem:DestroyBar()
    
    if activeCustomer then 
        print("[CustomerSystem] Destroying customer entity: " .. activeCustomer)
        DestroyEntity(activeCustomer) 
    end
    
    -- Reset state
    activeCustomer = nil
    hasSpawned = false
    is_at_counter = false 
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    
    print("[CustomerSystem] Ready for next customer.")
end

-- Called when customer fails (timeout OR too many wrong items)
function CustomerSystem.OnCustomerFailed(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER FAILED ==========")
    print("[CustomerSystem] Event - Sender: " .. tostring(senderID) .. ", Target: " .. tostring(targetID))
    
    -- Apply score penalty
    CustomerSystem:AddScore(-10)
    print("[CustomerSystem] -10 score penalty!")
    
    -- Increment customer count (failed customer still counts)
    customers_served = customers_served + 1
    print("[CustomerSystem] Customers served: " .. customers_served .. "/" .. MAX_CUSTOMERS)
    
    -- Clean up progress bar
    CustomerSystem:DestroyBar()
    
    -- Destroy the customer entity (textbox cleanup handled by CustomerTimeout event)
    if activeCustomer then
        print("[CustomerSystem] Destroying failed customer: " .. activeCustomer)
        DestroyEntity(activeCustomer)
    end
    
    -- Reset state for next customer
    activeCustomer = nil
    hasSpawned = false
    is_at_counter = false
    current_patience = 0.0
    spawnTimer = 0
    current_bar_state = "green"
    
    print("[CustomerSystem] Ready for next customer.")
end

-- =========================================================
-- BAR HELPER FUNCTIONS
-- =========================================================

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

function CustomerSystem:play_doorbell()
    local frogEntity = FindEntityByName("Frog")
    
    if frogEntity == nil then 
        print("[CustomerSystem] WARNING: Frog doorbell entity not found")
        return
    end

    PlayEntitySound(frogEntity)
    print("[CustomerSystem] Frog croaked, customer entering shop")
end 

-- =========================================================
-- LOG HELPER FUNCTION
-- =========================================================

function CustomerSystem:SpawnLog(message)
    local logEntity = SpawnPrefab(prefab_log)
    
    if logEntity then
        SetTextComponent(logEntity, "text", message)
        print("[CustomerSystem] Spawned log: " .. message)
    else
        print("[CustomerSystem] ERROR: Failed to spawn log!")
    end
end

return CustomerSystem