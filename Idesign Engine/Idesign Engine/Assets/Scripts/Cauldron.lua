local M = {}

-- =============================================================
-- [CONFIGURATION]
-- =============================================================
local CAULDRON_NAME = "Cauldron"
local name_stir = "CauldronStiring"       
local name_idle = "cauldronwithoutspoon" 

-- [VISUAL ITEMS SETTINGS]
local orbit_radius = 0.5    
local orbit_speed = 1.5     
local bob_speed = 1.5       
local bob_height = 0.1     
local height_offset = 1.2   
local center_offset_x = -0.5 -- Tweak if needed
local center_offset_z = -0.3

-- [PROGRESS BAR SETTINGS] (Friend's Settings)
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
local finish_delay = 1

-- =============================================================
-- [STATE VARIABLES]
-- =============================================================
local my_entity_id = nil
local is_active = false
local max_distance = 5.0
local player_id = nil
local accumulated_time = 0.0 

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
    local mx, my = GetMousePosition() -- Ensure this is bound in C++
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
        
        -- Stirring Init
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

    -- [BRANCH A] ADDING INGREDIENT (Priority)
    if held_item_id ~= -1 then
        table.insert(stored_ingredients, held_item_id)
        ConsumeSelectedItem()
        
        if SpawnCauldronItem and id_to_use then
            local visual_id = SpawnCauldronItem(id_to_use, held_item_id)
            if visual_id then table.insert(visual_entities, visual_id) end
        end
        
        M.PrintContents()
        return 
    end

    -- [BRANCH B] TOGGLE STIRRING
    is_active = not is_active

    if is_active then
        -- START STIRRING
        print("[Lua] Stirring Started")
        
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
            M.SpawnBar(id_to_use) -- Spawn Friend's Bar
        end
    else
        -- STOP STIRRING
        M.StopInteraction(id_to_use)
    end
end

function M.StopInteraction(entity_id)
    local id = entity_id or my_entity_id
    is_active = false
    is_finished = false
    print("[Lua] Stirring Stopped")
    
    SetCameraLock(false)
    if id then
        SetAnimationAsset(id, guid_idle)
        SetAnimationMode(id, 0) 
    end
    
    -- Destroy Progress Bar
    if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
    if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
end

-- =============================================================
-- 3. UPDATE LOOP
-- =============================================================
function M.on_update(arg1, arg2)
    local dt = 0.016
    if type(arg1) == "number" then dt = arg1 end
    if type(arg2) == "number" then dt = arg2 end

    accumulated_time = accumulated_time + dt

    if not my_entity_id then my_entity_id = FindEntityByName(CAULDRON_NAME) end
    if not my_entity_id then return end 

    -- [PART A] ANIMATE FLOATING ITEMS
    local count = #visual_entities
    if count > 0 then
        local raw_cx, raw_cy, raw_cz = GetPosition(my_entity_id)
        if raw_cx then 
            local cx = raw_cx + center_offset_x
            local cy = raw_cy 
            local cz = raw_cz + center_offset_z
            local angle_step = (2 * math.pi) / count
            
            for i, vis_id in ipairs(visual_entities) do
                local current_angle = (accumulated_time * orbit_speed) + ((i - 1) * angle_step)
                local offset_x = math.cos(current_angle) * orbit_radius
                local offset_z = math.sin(current_angle) * orbit_radius
                local offset_y = math.sin((accumulated_time * bob_speed) + i) * bob_height
                
                if SetPosition then
                    SetPosition(vis_id, cx + offset_x, cy + height_offset + offset_y, cz + offset_z)
                end
                if SetRotation then
                    SetRotation(vis_id, 0.0, accumulated_time * 50.0, 0.0) 
                end
            end
        end
    end

    -- [PART B] STIRRING LOGIC
    if is_active and player_id then
        -- 1. Distance Check
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(my_entity_id)
        local dist = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

        if dist > max_distance then
            M.StopInteraction(my_entity_id)
            return
        end

        -- 2. Update Progress Bar Billboard
        if entity_fill and entity_frame then
            M.UpdateBillboard(px, pz, cx, cz, cy)

            -- 3. Finished Check
            if is_finished then
                current_finish_timer = current_finish_timer - dt
                SetAnimationSettings(my_entity_id, 0.0, true, true) -- Stop animation
                
                if current_finish_timer <= 0 then
                    M.OnPotionComplete() -- Done!
                end
                return
            end

            -- 4. Mouse Input (Stirring)
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
                SetAnimationSettings(my_entity_id, 1.0, true, true) -- Play animation
            else
                SetAnimationSettings(my_entity_id, 0.0, true, true) -- Pause animation
            end

            if progress >= 1.0 then
                progress = 1.0
                is_finished = true
                current_finish_timer = finish_delay
                print("[Lua] Soup 100% -> Finishing...")
            end

            -- 5. Update Bar Visual
            local current_length = progress * bar_length_max
            -- Note: Friend's code used * 0.9 thickness, preserved here
            SetScale(entity_fill, current_length, bar_thickness * 0.9, bar_thickness * 0.9)
        end
    end
end

-- =============================================================
-- [FRIEND'S FUNCTIONS]
-- =============================================================
function M.SpawnBar(cauldronID)
    -- Requires "SpawnPrefab" in C++
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

function M.UpdateBillboard(px, pz, cx, cz, cy)
    -- 1. Calculate Angle to Player
    local dx = px - cx
    local dz = pz - cz
    local angle_rad = math.atan2(dx, dz)
    local angle_deg = math.deg(angle_rad)

    -- 2. Position Frame (Center)
    local frame_pos_x = cx + x_offset_right
    local frame_pos_y = cy + y_offset
    local frame_pos_z = cz

    if entity_frame then
        SetPosition(entity_frame, frame_pos_x, frame_pos_y, frame_pos_z)
        SetRotation(entity_frame, 0.0, angle_deg + 180.0, 0.0)
    end

    if entity_fill then
        SetRotation(entity_fill, 0.0, angle_deg + 180.0, 0.0)

        -- 3. Calculate SIDEWAYS Offset
        local right_angle = angle_rad + (math.pi / 2.0)
        local side_dist = (bar_length_max * tune_x)
        local side_offset_x = side_dist * math.sin(right_angle)
        local side_offset_z = side_dist * math.cos(right_angle)

        -- 4. Calculate DEPTH Offset
        local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
        local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)

        -- 5. Combine
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
    M.StopInteraction(my_entity_id)
    M.ClearCauldron()
    print("[Lua] Potion Complete! Ingredients Consumed.")
    -- Spawn Result Potion Here if needed
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