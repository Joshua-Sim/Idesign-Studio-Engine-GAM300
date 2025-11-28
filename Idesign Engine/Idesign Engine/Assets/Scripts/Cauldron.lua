local M = {}

-- =============================================================
-- [CONFIGURATION]
-- =============================================================
local CAULDRON_NAME = "Cauldron"
local name_stir = "CauldronStiring"       
local name_idle = "cauldronwithoutspoon" 

-- [RESULT SETTINGS]
local result_item_name = "strength_potion" 

-- [VISUAL ITEMS SETTINGS]
local orbit_radius_start = 0.5    
local orbit_speed_start = 1.5     
local orbit_radius_end = 0.0      -- Merge to center
local orbit_speed_end = 15.0      

local bob_speed = 1.5       
local bob_height = 0.1     
local height_offset = 1.2   
local center_offset_x = -0.5 
local center_offset_z = -0.3

-- [PROGRESS BAR SETTINGS]
local prefab_frame = "ProgressBarFrame_Prefab"
local prefab_fill = "ProgressBarFill_Prefab"
local bar_length_max = 0.5
local bar_thickness = 0.2
local y_offset = 1.5
local x_offset_right = -0.5
local tune_x = 1.6
local tune_y = -0.04
local billboard_depth_adjust = 0.055

-- [STIRRING SETTINGS]
local stir_sensitivity = 0.005
local finish_delay = 1.0

-- =============================================================
-- [STATE VARIABLES]
-- =============================================================
local my_entity_id = nil
local is_active = false
local max_distance = 5.0
local player_id = nil
local accumulated_time = 0.0 
local current_orbit_angle = 0.0 

-- Dynamic Prompt State
local last_prompt_state = ""
local debug_timer = 0.0

-- Progress State
local current_finish_timer = 0.0
local is_finished = false
local progress = 0.0
local entity_frame = nil
local entity_fill = nil

-- Mouse Stirring State
local last_mouse_x = 0
local last_mouse_y = 0
local first_frame = true
local spoon_x = 0.0
local spoon_y = 100.0
local last_angle = 0.0

-- Ingredient State
local guid_stir = ""
local guid_idle = ""

-- Unified List: Stores { data_id = int, visual_id = int }
local cauldron_contents = {} 

local item_names = { [0] = "unchopped_herb", [1] = "chopped_herb" }

-- =============================================================
-- [HELPERS]
-- =============================================================
local function ResolveID(arg1, arg2)
    if type(arg1) == "number" then return arg1 end
    if type(arg2) == "number" then return arg2 end
    return nil
end

local function GetMouseDelta()
    local mx, my = GetMousePosition() 
    if first_frame then
        last_mouse_x = mx
        last_mouse_y = my
        first_frame = false
        return 0, 0
    end
    local dx = mx - last_mouse_x
    local dy = my - last_mouse_y
    last_mouse_x = mx
    last_mouse_y = my
    return dx, dy
end

-- Extracts just the Ingredient IDs for the C++ Recipe Checker
local function GetIngredientIDs()
    local ids = {}
    for i, item in ipairs(cauldron_contents) do
        table.insert(ids, item.data_id)
    end
    return ids
end

-- =============================================================
-- 1. INIT
-- =============================================================
function M.on_init(arg1, arg2)
    local entity_id = ResolveID(arg1, arg2)
    if entity_id then
        my_entity_id = entity_id 
        print("[Lua] Cauldron Initialized: " .. tostring(my_entity_id))
        
        -- Force initial text
        if SetInteractionText then
            SetInteractionText(my_entity_id, "Stir Cauldron")
        end

        guid_stir = GetAnimationGUID(name_stir)
        guid_idle = GetAnimationGUID(name_idle)
        SetAnimationAsset(my_entity_id, guid_idle)
        SetAnimationMode(my_entity_id, 0)
        last_angle = math.atan2(spoon_y, spoon_x)
    end
    player_id = FindEntityByName("Player")
end

-- =============================================================
-- 2. INTERACT
-- =============================================================
function M.OnInteract(arg1, arg2)
    local id_to_use = my_entity_id or ResolveID(arg1, arg2) or FindEntityByName(CAULDRON_NAME)
    if id_to_use and not my_entity_id then my_entity_id = id_to_use end

    local held_item_id = GetSelectedItem()

    -- [BRANCH A] ADDING INGREDIENT
    if held_item_id ~= -1 then
        ConsumeSelectedItem()
        
        -- Spawn Visual
        local vis_id = -1
        if SpawnCauldronItem and id_to_use then
            vis_id = SpawnCauldronItem(id_to_use, held_item_id)
        end

        -- Store unified data
        table.insert(cauldron_contents, { 
            data_id = held_item_id, 
            visual_id = vis_id 
        })
        
        M.PrintContents()
        return 
    end

    -- [BRANCH B] TOGGLE STIRRING
    is_active = not is_active

    if is_active then
        local current_ids = GetIngredientIDs()
        local isValid = false
        
        if CheckValidRecipe then
            isValid = CheckValidRecipe(current_ids)
        else
            print("[Lua ERROR] CheckValidRecipe missing!")
        end

        if isValid then
            print("[Lua] Valid Recipe! Stirring...")
            
            -- Reset State
            progress = 0.0
            current_finish_timer = 0.0
            is_finished = false
            spoon_x = 0.0
            spoon_y = 100.0
            last_angle = math.atan2(spoon_y, spoon_x)
            
            SetCameraLock(true) -- [LOCKS CAMERA HERE]
            
            if id_to_use then
                SetAnimationAsset(id_to_use, guid_stir)
                SetAnimationMode(id_to_use, 1) 
                M.SpawnBar(id_to_use)
            end
            if player_id then SetGravity(player_id, 0.0) end
        else
            print("[Lua] Invalid Recipe!")
            is_active = false 
        end
    else
        M.StopInteraction(id_to_use)
    end
end

function M.StopInteraction(entity_id)
    local id = entity_id or my_entity_id
    is_active = false
    is_finished = false
    print("[Lua] Stirring Stopped")
    
    SetCameraLock(false) -- [UNLOCKS CAMERA HERE]
    
    if id then
        SetAnimationAsset(id, guid_idle)
        SetAnimationMode(id, 0) 
    end
    if player_id then SetGravity(player_id, 1.0) end
    
    M.DestroyProgressBar()
end

-- =============================================================
-- 3. UPDATE LOOP
-- =============================================================
function M.on_update(arg1, arg2)
    local dt = 0.016
    if type(arg1) == "number" then dt = arg1 end
    if type(arg2) == "number" then dt = arg2 end

    accumulated_time = accumulated_time + dt
    debug_timer = debug_timer + dt

    -- 1. Ensure ID is valid
    if not my_entity_id then 
        my_entity_id = FindEntityByName(CAULDRON_NAME)
        if not my_entity_id and debug_timer > 1.0 then 
            print("[Lua Info] Waiting for Cauldron ID...") 
            debug_timer = 0.0
            return 
        end
    end
    if not my_entity_id then return end 

    -- 2. DYNAMIC TEXT LOGIC (Runs every frame)
    if SetInteractionText then
        local held_item = GetSelectedItem() 
        local desired_text = ""

        if is_active then
            desired_text = "Stirring..." 
        elseif held_item ~= -1 then
            desired_text = "Add Ingredient"
        else
            desired_text = "Stir Cauldron"
        end

        if desired_text ~= last_prompt_state then
            SetInteractionText(my_entity_id, desired_text)
            last_prompt_state = desired_text
        end
    end

    -- 3. CLEANUP & ANIMATION (Passive)
    local count = #cauldron_contents
    local i = count

    while i > 0 do
        local item = cauldron_contents[i]
        
        -- [CRITICAL FIX]: Check 'visual_id > 0' first.
        -- If visual_id is -1, it means spawning failed, but the ingredient is still physically added.
        -- We don't want to auto-remove it just because the graphics failed.
        if item.visual_id > 0 and IsEntityValid and not IsEntityValid(item.visual_id) then
            print("[Lua] Item removed from cauldron: " .. tostring(item.data_id))
            table.remove(cauldron_contents, i)
            
            if is_active then M.StopInteraction(my_entity_id) end
        end
        i = i - 1
    end

    -- 4. APPLY ORBIT POSITIONS (After cleanup)
    count = #cauldron_contents
    if count > 0 then
        local raw_cx, raw_cy, raw_cz = GetPosition(my_entity_id)
        if raw_cx then 
            local cx = raw_cx + center_offset_x
            local cy = raw_cy 
            local cz = raw_cz + center_offset_z
            local angle_step = (2 * math.pi) / count
            
            local dynamic_radius = orbit_radius_start 
            local dynamic_speed = orbit_speed_start
            
            if is_active then
                dynamic_radius = orbit_radius_start + (orbit_radius_end - orbit_radius_start) * progress
                dynamic_speed  = orbit_speed_start  + (orbit_speed_end  - orbit_speed_start)  * progress
            end
            
            current_orbit_angle = current_orbit_angle + (dt * dynamic_speed)

            for k, item in ipairs(cauldron_contents) do
                local vis_id = item.visual_id
                
                -- Only update position if we have a valid visual entity
                if vis_id > 0 then 
                    local item_angle = current_orbit_angle + ((k - 1) * angle_step)
                    local offset_x = math.cos(item_angle) * dynamic_radius
                    local offset_z = math.sin(item_angle) * dynamic_radius
                    
                    local current_bob = is_active and 0.0 or bob_height
                    local offset_y = math.sin((accumulated_time * bob_speed) + k) * current_bob
                    
                    if SetPosition then
                        SetPosition(vis_id, cx + offset_x, cy + height_offset + offset_y, cz + offset_z)
                    end
                    
                    if SetRotation then
                        local rot_speed = 50.0 
                        if is_active then 
                            SetRotation(vis_id, 0.0, 0.0, 0.0) 
                        else
                            SetRotation(vis_id, 0.0, accumulated_time * rot_speed, 0.0) 
                        end
                    end
                end
            end
        end
    end

    -- 5. STIRRING INPUT LOGIC
    if is_active and player_id then
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(my_entity_id)
        local dist = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

        if dist > max_distance then
            M.StopInteraction(my_entity_id)
            return
        end

        if entity_fill and entity_frame then
            M.UpdateBillboard(px, pz, cx, cz, cy)

            if is_finished then
                current_finish_timer = current_finish_timer - dt
                SetAnimationSettings(my_entity_id, 0.0, true, true) 
                
                if current_finish_timer <= 0 then
                    M.OnPotionComplete() 
                end
                return
            end

            local dx, dy = GetMouseDelta()
            spoon_x = spoon_x + dx
            spoon_y = spoon_y + dy

            local len = math.sqrt(spoon_x*spoon_x + spoon_y*spoon_y)
            if len > 0.1 then
                spoon_x = (spoon_x / len) * 100.0
                spoon_y = (spoon_y / len) * 100.0
            end

            local current_angle = math.atan2(spoon_y, spoon_x)
            local angle_delta = current_angle - last_angle
            local PI = 3.14159265359

            if angle_delta > PI then angle_delta = angle_delta - (2 * PI) end
            if angle_delta < -PI then angle_delta = angle_delta + (2 * PI) end
            last_angle = current_angle

            local is_stirring = math.abs(angle_delta) > 0.02

            if is_stirring then
                progress = progress + (math.abs(angle_delta) * stir_sensitivity)
                SetAnimationSettings(my_entity_id, 1.0, true, true)
            else
                SetAnimationSettings(my_entity_id, 0.0, true, true)
            end

            if progress >= 1.0 then
                progress = 1.0
                is_finished = true
                current_finish_timer = finish_delay
                print("[Lua] Soup 100% -> Finishing...")
            end

            local current_length = progress * bar_length_max
            SetScale(entity_fill, current_length, bar_thickness * 0.9, bar_thickness * 0.9)
        end
    end
end

-- =============================================================
-- [HELPER FUNCTIONS]
-- =============================================================

function M.SpawnBar(cauldronID)
    if SpawnPrefab then
        entity_frame = SpawnPrefab(prefab_frame, cauldronID)
        if entity_frame then
            SetScale(entity_frame, bar_length_max, bar_thickness, bar_thickness*5)
        end

        entity_fill = SpawnPrefab(prefab_fill, cauldronID)
        if entity_fill then
            SetScale(entity_fill, 0.0, bar_thickness, bar_thickness)
        end
    else
        print("[Lua ERROR] SpawnPrefab function missing!")
    end
end

function M.DestroyProgressBar()
    if entity_frame then 
        DestroyEntity(entity_frame) 
        entity_frame = nil
    end
    if entity_fill then 
        DestroyEntity(entity_fill) 
        entity_fill = nil
    end
end

function M.UpdateBillboard(px, pz, cx, cz, cy)
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz)
    local angle_deg = math.deg(angle_rad)

    local frame_pos_x = cx + x_offset_right
    local frame_pos_y = cy + y_offset
    local frame_pos_z = cz

    if entity_frame then
        SetPosition(entity_frame, frame_pos_x, frame_pos_y, frame_pos_z)
        SetRotation(entity_frame, 0.0, angle_deg + 180.0, 0.0)
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

-- =============================================================
-- UTILITY
-- =============================================================
function M.OnPotionComplete()
    -- 1. Stop Minigame
    M.StopInteraction(my_entity_id)
    M.ClearCauldron()
    print("[Lua] Potion Complete! Ingredients Consumed.")

    -- 2. CALCULATE SPAWN POSITION
    local cx, cy, cz = GetPosition(my_entity_id)
    if cx then
        local spawn_x = cx + center_offset_x
        local spawn_y = cy + height_offset 
        local spawn_z = cz + center_offset_z

        -- 3. SPAWN PREFAB
        local prefabName = result_item_name .. "_Prefab"
        
        if SpawnPrefab then
            local potionID = SpawnPrefab(prefabName, spawn_x, spawn_y, spawn_z)
            print("[Lua] Spawned Potion Prefab: " .. prefabName .. " (ID: " .. tostring(potionID) .. ")")
        else
            print("[Lua ERROR] SpawnPrefab missing!")
        end
    end
end

function M.PrintContents()
    local content_string = "Cauldron Contents: "
    if #cauldron_contents == 0 then print(content_string .. "Empty") return end
    
    for i, item in ipairs(cauldron_contents) do
        local name = item_names[item.data_id] or tostring(item.data_id)
        content_string = content_string .. "[" .. name .. "] "
    end
    print(content_string)
end

function M.ClearCauldron()
    for _, item in ipairs(cauldron_contents) do
        if IsEntityValid and IsEntityValid(item.visual_id) then
            DestroyEntity(item.visual_id)
        end
    end
    cauldron_contents = {}
    print("[Lua] Cauldron Cleared")
end

return M