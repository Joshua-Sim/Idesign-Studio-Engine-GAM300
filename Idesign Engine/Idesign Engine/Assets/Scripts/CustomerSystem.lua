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
local current_score = 0
local score_text_entity = nil

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
    
    -- Keep the old callback for backwards compatibility
    CustomerSystemNotifyComplete = function()
        print("[CustomerSystem] Customer completed order (legacy callback).")
        self:OnCustomerComplete()
    end
end

function CustomerSystem:on_update(dt)
    if not navMeshBuilt then return end
    
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
                        
                        -- TIME EXPIRED - CUSTOMER LEAVES UNHAPPY
                        print("[CustomerSystem] Time expired! Customer leaving unhappy.")
                        
                        -- Apply score penalty
                        CustomerSystem:AddScore(-10)
                        print("[CustomerSystem] -10 score penalty!")
                        
                        -- Clean up progress bar
                        if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
                        if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
                        
                        -- Tell customer to clean up and leave
                        if activeCustomer then
                            FireEvent("CustomerTimeout", activeCustomer, activeCustomer, 0)
                        end
                        
                        -- Reset state
                        activeCustomer = nil
                        hasSpawned = false
                        is_at_counter = false
                        current_patience = 0.0
                        spawnTimer = 0
                        current_bar_state = "green"
                        
                        print("[CustomerSystem] Ready for next customer.")
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
    
    -- Play doorbell sound BEFORE spawning customer
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
    current_score = current_score + amount
    print("[CustomerSystem] Score changed by " .. amount .. ", new score: " .. current_score)
    CustomerSystem:UpdateScoreDisplay()
end

function CustomerSystem:UpdateScoreDisplay()
    if score_text_entity then
        local score_text = "Score: " .. tostring(current_score)
        SetTextComponent(score_text_entity, "text", score_text)
    end
end

function CustomerSystem:GetScore()
    return current_score
end

-- =========================================================
-- EVENT HANDLERS
-- =========================================================

function CustomerSystem.OnCustomerComplete(senderID, targetID, actionType)
    print("[CustomerSystem] ========== CUSTOMER DONE ==========")
    print("[CustomerSystem] Event received - Sender: " .. tostring(senderID) .. ", Target: " .. tostring(targetID))
    
    -- Add score reward
    CustomerSystem:AddScore(10)
    print("[CustomerSystem] +10 score reward!")
    
    -- Clean up progress bar
    if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
    if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
    
    -- Destroy customer entity
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
end

function CustomerSystem:DestroyBar()
    if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
    if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
end

function CustomerSystem:UpdateBillboard(px, pz, cx, cy, cz)
    -- 1. Calculate Angle to Player
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz) 
    local angle_deg = math.deg(angle_rad)

    -- 2. Position Frame (Center above customer)
    local frame_pos_x = cx + x_offset_right
    local frame_pos_y = cy + y_offset
    local frame_pos_z = cz 

    if entity_frame then
        SetPosition(entity_frame, frame_pos_x, frame_pos_y, frame_pos_z)
        SetRotation(entity_frame, 0.0, angle_deg, 0.0)
    end

    if entity_fill then
        -- Rotate Fill to face player (Flipped 180 to match fill direction)
        SetRotation(entity_fill, 0.0, angle_deg + 180.0, 0.0)
        
        -- 3. Calculate SIDEWAYS Offset (Left/Right alignment)
        local right_angle = angle_rad + (math.pi / 2.0)
        local side_dist = (bar_length_max * tune_x)
        
        local side_offset_x = side_dist * math.sin(right_angle)
        local side_offset_z = side_dist * math.cos(right_angle)

        -- 4. Calculate DEPTH Offset (Forward/Back alignment)
        local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
        local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)

        -- 5. Combine positions
        local fill_x = frame_pos_x + side_offset_x + depth_offset_x
        local fill_y = frame_pos_y + tune_y
        local fill_z = frame_pos_z + side_offset_z + depth_offset_z

        SetPosition(entity_fill, fill_x, fill_y, fill_z)
    end
end

-- =========================================================
-- AUDIO
-- =========================================================

function CustomerSystem:play_doorbell()
    local frogEntity = FindEntityByName("Frog")
    
    if frogEntity == nil then 
        print("[CustomerSystem] WARNING: Frog doorbell entity not found")
        return
    end

    PlayEntitySound(frogEntity)
    print("[CustomerSystem] Frog croaked, customer entering shop")
end 

return CustomerSystem