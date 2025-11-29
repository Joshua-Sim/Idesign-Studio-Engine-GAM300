local M = {}

-- =============================================================
-- [CONFIGURATION]
-- =============================================================
local CAULDRON_NAME = "Cauldron"
local name_stir = "CauldronStiring"       
local name_idle = "cauldronwithoutspoon" 

-- [RESULT SETTINGS]
-- The name of the item to spawn when done.
-- The script will look for "strength_potion_Prefab" (automatically adds "_Prefab")
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
local stored_ingredients = {} 
local visual_entities = {}    
local item_names = { [0] = "unchopped_herb", [1] = "chopped_herb", [2] = "pear" }

-- Player Interact Prompt
local last_prompt_state = ""

-- [AUDIO STATE]
local is_stirring_sound_active = false
local last_sound_trigger_time = 0.0 -- [NEW] Debounce timer

-- [UI LOG SETTINGS]
local prefab_log = "CauldronLog_Prefab" -- Reusing your existing prefab

-- =============================================================
-- [AUDIO HELPERS]
-- =============================================================
-- [AUDIO HELPERS]
local function PlayStirSound()
    -- [FIX] Debounce: Don't try to play more than once every 0.1 seconds.
    -- This prevents the "Machine Gun" effect while waiting for the engine to report IsPlaying.
    if (accumulated_time - last_sound_trigger_time) < 0.1 then return end

    -- 1. Ask Engine: Is this sound already playing?
    local is_playing = false
    if Sound and Sound.IsPlaying then
        is_playing = Sound.IsPlaying("event:/Stirring")
    end

    -- 2. Only play if stopped
    if not is_playing then
        if Sound and Sound.Play then
            Sound.Play("event:/Stirring")
            last_sound_trigger_time = accumulated_time -- [NEW] Update timer
        end
    end
end

local function StopStirSound()
    -- Only send stop command if it is actually playing
    if Sound and Sound.IsPlaying and Sound.IsPlaying("event:/Stirring") then
        if Sound.Stop then
            -- Note: Uses your fixed Stop function that takes an argument
            Sound.Stop("event:/Stirring")
        end
    end
end

-- =============================================================
-- [HELPERS]
-- =============================================================
local function ResolveID(arg1, arg2)
    -- 1. Is the argument the Script Table (self)?
    if type(arg1) == "table" and arg1.entity then 
        return arg1.entity 
    end
    
    -- 2. Is the argument a direct number? (Legacy support)
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

-- [HELPER] GetIngredientIDs
-- Changed to read from 'stored_ingredients' directly
local function GetIngredientIDs()
    return stored_ingredients -- Since this is already a list of ints, we just return it
end

local function SpawnLog(message)
    if SpawnPrefab and SetTextComponent then
        -- Spawn the log UI entity
        local logEntity = SpawnPrefab(prefab_log)
        
        if logEntity then
            -- Update the text component on that entity
            SetTextComponent(logEntity, "text", message)
            print("[Cauldron] UI Log: " .. message)
        else
            print("[Cauldron] Failed to spawn log prefab!")
        end
    end
end

-- =============================================================
-- 1. INIT
-- =============================================================
function M.on_init(arg1, arg2)
    local entity_id = ResolveID(arg1, arg2)
    if entity_id then
        my_entity_id = entity_id 
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
        table.insert(stored_ingredients, held_item_id)
        ConsumeSelectedItem()
        
        -- Default to -1 (No Visual) logic
        local vis_id = -1
        if SpawnCauldronItem and id_to_use then
            local res = SpawnCauldronItem(id_to_use, held_item_id)
            if res then vis_id = res end
        end
        
        -- CRITICAL: Insert into visual list even if it failed (-1) to keep lists synced
        table.insert(visual_entities, vis_id)
        
        M.PrintContents()
        return 
    end

    -- [BRANCH B] TOGGLE STIRRING
    is_active = not is_active

    if is_active then
        -- Check Recipe using stored_ingredients
        local resultID = -1
        if CheckValidRecipe then
            resultID = CheckValidRecipe(stored_ingredients)
        else
            print("[Lua ERROR] CheckValidRecipe missing!")
        end

        if resultID ~= -1 then
            print("[Lua] Valid Recipe Found! Result ID: " .. resultID)
            
            if GetItemNameById then
                result_item_name = GetItemNameById(resultID)
            end

            -- Reset State
            progress = 0.0
            current_finish_timer = 0.0
            is_finished = false
            spoon_x = 0.0
            spoon_y = 100.0
            last_angle = math.atan2(spoon_y, spoon_x)
            
            SetCameraLock(true) 
            
            if id_to_use then
                SetAnimationAsset(id_to_use, guid_stir)
                SetAnimationMode(id_to_use, 1) 
                M.SpawnBar(id_to_use)
            end
            if player_id then SetGravity(player_id, 0.0) end
        else
            Sound.Play("event:/InvalidRecipe")
            print("[Lua] Invalid Recipe!")
            SpawnLog("Invalid Recipe!") 
            
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
    
    StopStirSound() 
    
    SetCameraLock(false)
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
    if not debug_timer then debug_timer = 0.0 end
    debug_timer = debug_timer + dt

    if not my_entity_id then 
        my_entity_id = FindEntityByName(CAULDRON_NAME)
        if not my_entity_id then return end 
    end

    -- 1. DYNAMIC TEXT
    if SetInteractionText then
        local held_item = GetSelectedItem() 
        local desired_text = ""
        if is_active then desired_text = "Stirring..." 
        elseif held_item ~= -1 then desired_text = "Add Ingredient"
        else desired_text = "Stir Cauldron" end

        if desired_text ~= last_prompt_state then
            SetInteractionText(my_entity_id, desired_text)
            last_prompt_state = desired_text
        end
    end

    -- 2. CLEANUP & ANIMATION (Using Separate Lists)
    local count = #visual_entities
    local i = count

    while i > 0 do
        local vis_id = visual_entities[i]
        local is_valid = true
        
        -- Check validity (Only if ID > 0)
        if vis_id > 0 and IsEntityValid and not IsEntityValid(vis_id) then
            is_valid = false
        end

        if is_valid and vis_id > 0 then
            -- Animation Math
            local angle_step = (2 * math.pi) / count
            local item_angle = current_orbit_angle + ((i - 1) * angle_step)
            
            local dynamic_radius = orbit_radius_start 
            if is_active then
                dynamic_radius = orbit_radius_start + (orbit_radius_end - orbit_radius_start) * progress
            end

            local offset_x = math.cos(item_angle) * dynamic_radius
            local offset_z = math.sin(item_angle) * dynamic_radius
            
            local current_bob = is_active and 0.0 or bob_height
            local offset_y = math.sin((accumulated_time * bob_speed) + i) * current_bob
            
            local raw_cx, raw_cy, raw_cz = GetPosition(my_entity_id)
            if raw_cx then 
                local tx = raw_cx + center_offset_x + offset_x
                local ty = raw_cy + height_offset + offset_y
                local tz = raw_cz + center_offset_z + offset_z

                -- [FIX] pcall SetPosition
                if SetPosition then
                    local ok = pcall(SetPosition, vis_id, tx, ty, tz)
                    if not ok then is_valid = false end
                end
                
                -- [FIX] pcall SetRotation
                if is_valid and SetRotation then
                    local rot_speed = 50.0 
                    local ok = true
                    if is_active then 
                        ok = pcall(SetRotation, vis_id, 0.0, 0.0, 0.0) 
                    else
                        ok = pcall(SetRotation, vis_id, 0.0, accumulated_time * rot_speed, 0.0) 
                    end
                    if not ok then is_valid = false end
                end
            end
        end

        -- Cleanup
        if not is_valid then
            print("[Lua] Item removed/invalid at index " .. i)
            table.remove(visual_entities, i)
            table.remove(stored_ingredients, i) -- Remove from data list too
            
            if is_active then M.StopInteraction(my_entity_id) end
        end
        i = i - 1
    end
    
    -- Update Timer
    local dynamic_speed = orbit_speed_start
    if is_active then
        dynamic_speed = orbit_speed_start + (orbit_speed_end - orbit_speed_start) * progress
    end
    current_orbit_angle = current_orbit_angle + (dt * dynamic_speed)

    -- 3. STIRRING LOGIC
    if is_active and player_id then
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(my_entity_id)
        
        if not px or not cx then 
            M.StopInteraction(my_entity_id)
            return
        end

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
                
                if current_finish_timer <= 0 then M.OnPotionComplete() end
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
                PlayStirSound() -- Safe call now
            else
                SetAnimationSettings(my_entity_id, 0.0, true, true)
                StopStirSound() -- Safe call now
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
    -- 1. Stop Minigame (This also stops the stirring sound via StopInteraction)
    M.StopInteraction(my_entity_id)
    M.ClearCauldron()
    print("[Lua] Potion Complete! Ingredients Consumed.")

    -- [NEW] Play Success Sound
    if Sound and Sound.Play then
        Sound.Play("event:/RecipeComplete")
    end

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
    if #stored_ingredients == 0 then print(content_string .. "Empty") return end
    for i, id in ipairs(stored_ingredients) do
        local name = item_names[id] or tostring(id)
        content_string = content_string .. "[" .. name .. "] "
    end
    print(content_string)
end

function M.ClearCauldron()
    stored_ingredients = {}
    for i, vis_id in ipairs(visual_entities) do
        if DestroyEntity then DestroyEntity(vis_id) end
    end
    visual_entities = {}
    print("[Lua] Cauldron Cleared")
end

return M