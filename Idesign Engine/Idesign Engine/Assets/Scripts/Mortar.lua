local Mortar = {}

-- --- PREFABS ---
local PREFAB_CIRCLE = "TapCircle_Prefab"
local PREFAB_GOOD   = "GoodPound_Prefab"
local PREFAB_MEDIUM = "MediumPound_Prefab"
local PREFAB_POOR   = "PoorPound_Prefab"
local PREFAB_FRAME  = "ProgressBarFrame_Prefab"
local PREFAB_FILL   = "ProgressBarFill_Prefab"

-- --- CONFIGURATION ---
local DECAY_RATE = 5.0 
local MORTAR_CAM_HEIGHT = 2.5
local TARGET_CAM_PITCH = -70.0
local FLIGHT_DURATION = 0.8
local PLAYER_Y_OFFSET = 0.0

-- How far away from the mortar the camera should sit
local FIXED_OFFSET_X = 0.0   -- 0 means centered on X
local FIXED_OFFSET_Z = 1.0   -- 2.5 units "back" on Z
local FIXED_OFFSET_Y = 3.0   -- 4.0 units "up" on Y

-- The fixed angles the camera should have once it arrives
local FIXED_TARGET_PITCH = -70.0 -- Looking down at an angle
local FIXED_TARGET_YAW   = 270.0 -- Facing the workstation

-- --- INTERNAL STATE ---
local is_playing = false
local is_camera_flying = false
local camera_flight_t = 0.0
local start_cam_pos = {x=0, y=0, z=0}
local start_cam_rot = {pitch=0, yaw=0}
local target_cam_pos = {x=0, y=0, z=0}

local spawn_timer = 0.0
local active_circles = {} 
local feedback_texts = {} 
local click_was_down = false 
local my_entity_id = nil

local FINISH_DELAY = 1.0  -- Seconds to wait after finishing before closing the UI

-- [INGREDIENT STATE]
local stored_ingredients = {}
local visual_entities = {}
local MAX_INGREDIENTS = 3
local OFFSET_X_START, SPACING_X = -1.5, -1 
local OFFSET_Y, OFFSET_Z = 0.1, 0.0
local ROT_X, ROT_Y, ROT_Z = 180.0, 0.0, 0.0

-- [POUNDING/SPINNING STATE]
local current_progress, MAX_PROGRESS = 0, 100
local POINTS_GOOD, POINTS_MEDIUM, POINTS_POOR = 20, 10, 5
local CLICK_RADIUS = 75 
local is_spinning, spin_progress = false, 0.0
local SPIN_REQUIREMENT = math.pi * 10 
local spinner_visual_id, last_angle = nil, 0.0
local last_mx, last_my = nil, nil
local spoon_x, spoon_y = 0.0, 100.0

-- [2D UI]
local e_progress_frame, e_progress_fill = nil, nil
local UI_WIDTH, UI_HEIGHT = 1920.0, 1080.0
local BAR_WIDTH_PX, BAR_HEIGHT_PX, BAR_Y_POS = 600, 40, 400

-- --- INIT ---
function Mortar.on_init(self)
    my_entity_id = self.entity
    SetInteractionText(my_entity_id, "Requires Ingredient")
    math.randomseed(os.time()) 
end

-- --- UPDATE ---
function Mortar.on_update(self, dt)
    -- 1. MANUAL CAMERA FLIGHT
    if is_camera_flying then
        camera_flight_t = camera_flight_t + dt
        local t = math.min(camera_flight_t / FLIGHT_DURATION, 1.0)
        local smooth_t = t * t * (3 - 2 * t)

        -- Shortest Path Yaw Calculation
        local delta_yaw = target_cam_rot.yaw - start_cam_rot.yaw
        if delta_yaw > 180 then delta_yaw = delta_yaw - 360
        elseif delta_yaw < -180 then delta_yaw = delta_yaw + 360 end

        local cur_x = start_cam_pos.x + (target_cam_pos.x - start_cam_pos.x) * smooth_t
        local cur_y = start_cam_pos.y + (target_cam_pos.y - start_cam_pos.y) * smooth_t
        local cur_z = start_cam_pos.z + (target_cam_pos.z - start_cam_pos.z) * smooth_t
        
        local cur_p = start_cam_rot.pitch + (target_cam_rot.pitch - start_cam_rot.pitch) * smooth_t
        local cur_y_rot = start_cam_rot.yaw + (delta_yaw * smooth_t)

        TeleportPlayer(cur_x, cur_y, cur_z, 0, cur_y_rot, 0)
        SetCameraRot(cur_p, cur_y_rot)

        if t >= 1.0 then is_camera_flying = false end
        return 
    end

    -- 2. INGREDIENT VISUALS (Always update positions)
    if my_entity_id then
        local cx, cy, cz = GetPosition(my_entity_id)
        
        for i = #visual_entities, 1, -1 do
            local vis_id = visual_entities[i]
            local item_id = stored_ingredients[i] -- Get the actual item ID

            if vis_id > 0 and (not IsItemZipping or not IsItemZipping(vis_id)) and cx then
                -- 1. Calculate Position
                local current_x_offset = OFFSET_X_START + ((i - 1) * SPACING_X)
                SetPosition(vis_id, cx + current_x_offset, cy + OFFSET_Y, cz + OFFSET_Z)
                
                -- 2. Conditional Rotation Logic
                local itemName = GetItemNameById(item_id)
                
                -- We check if "Herb" is in the name and it doesn't say "Chopped"
                if string.find(itemName, "Herb") and not string.find(itemName, "Chopped") then
                    -- Rotate the unchopped herb (e.g., laying it flat)
                    SetRotation(vis_id, 180.0, 0.0, 0.0) 
                else
                    -- Keep everything else (Mushrooms, Chopped herbs, etc.) at default upright
                    SetRotation(vis_id, 0.0, 0.0, 0.0)
                end
            end
        end
    end

    if not is_playing then return end

    -- =============================================================
    -- 3. PHASE 1: POUNDING
    -- =============================================================
    if not is_spinning then
        current_progress = math.max(0, current_progress - (DECAY_RATE * dt))
        
        if e_progress_fill then
            local ratio = math.min(current_progress / MAX_PROGRESS, 1.0)
            SetScale(e_progress_fill, ratio * BAR_WIDTH_PX, BAR_HEIGHT_PX * 0.8, 1.0)
            SetPosition(e_progress_fill, (UI_WIDTH / 2) - (BAR_WIDTH_PX / 2) + (ratio * BAR_WIDTH_PX / 2), BAR_Y_POS, 0.9)
        end

        spawn_timer = spawn_timer + dt
        if spawn_timer >= 1.0 then
            spawn_timer = 0
            local rx, ry = math.random(200, 1720), math.random(200, 880)
            local cid = SpawnPrefab(PREFAB_CIRCLE, rx, ry, 0.5)
            if cid and cid ~= -1 then
                local sx, sy, sz = GetScale(cid)
                table.insert(active_circles, { id = cid, life = 0.0, x = rx, y = ry, base_sx = sx, base_sy = sy, base_sz = sz })
            end
        end

        -- Circle Animation
        for i = #active_circles, 1, -1 do
            local c = active_circles[i]
            c.life = c.life + dt
            if c.life >= 3.0 then 
                DestroyEntity(c.id); table.remove(active_circles, i)
            else
                local s = math.max(1.0 - (c.life / 3.0), 0.05)
                SetScale(c.id, c.base_sx * s, c.base_sy * s, c.base_sz * s)
            end
        end

        if IsMouseButtonDown(0) and not click_was_down then
            click_was_down = true
            local uvx, uvy = GetMouseUV()
            local mx, my = uvx * UI_WIDTH, UI_HEIGHT - (uvy * UI_HEIGHT)
            for i = #active_circles, 1, -1 do
                local circ = active_circles[i]
                local dyn_rad = CLICK_RADIUS * math.max(1.0 - (circ.life / 3.0), 0.05)
                if math.sqrt((mx - circ.x)^2 + (my - circ.y)^2) <= dyn_rad then
                    current_progress = math.min(current_progress + ((circ.life <= 1.0) and POINTS_GOOD or (circ.life <= 2.0 and POINTS_MEDIUM or POINTS_POOR)), MAX_PROGRESS)
                    
                    local prefab = (circ.life <= 1.0 and PREFAB_GOOD or (circ.life <= 2.0 and PREFAB_MEDIUM or PREFAB_POOR))
                    local fb_id = SpawnPrefab(prefab, mx, my + 20, 0.6)
                    if fb_id and fb_id ~= -1 then
                        local dsx, dsy, dsz = GetScale(fb_id)
                        table.insert(feedback_texts, { id = fb_id, life = 0.0, start_x = mx, start_y = my + 20, base_sx = dsx, base_sy = dsy, base_sz = dsz })
                    end
                    DestroyEntity(circ.id); table.remove(active_circles, i); break
                end
            end
        elseif not IsMouseButtonDown(0) then click_was_down = false end

        -- TRANSITION TO SPINNING
        if current_progress >= MAX_PROGRESS then
            is_spinning = true
            -- UI Snap to Full
            if e_progress_fill then 
                SetPosition(e_progress_fill, UI_WIDTH / 2, BAR_Y_POS, 0.9)
                SetScale(e_progress_fill, BAR_WIDTH_PX, BAR_HEIGHT_PX * 0.8, 1.0) 
            end
            
            -- Clear Pounding UI
            for _, c in ipairs(active_circles) do DestroyEntity(c.id) end; active_circles = {}
            
            -- Initialize Spinning State
            local uvx, uvy = GetMouseUV()
            last_mx, last_my = uvx * UI_WIDTH, UI_HEIGHT - (uvy * UI_HEIGHT)
            spin_progress = 0.0
            
            spinner_visual_id = SpawnPrefab(PREFAB_GOOD, UI_WIDTH / 2, UI_HEIGHT / 2, 0.5)
            SetScale(spinner_visual_id, 200, 200, 1)
            last_angle = math.atan2(100, 0)
        end

    -- =============================================================
    -- 4. PHASE 2: SPINNING
    -- =============================================================
    else
        if is_finished then
            -- Subtract delta time from the timer
            -- This now has a value because we set it below!
            finish_timer = finish_timer - dt
            if finish_timer <= 0 then 
                Mortar.StopMinigame(my_entity_id, true) 
            end
        else
            local uvx, uvy = GetMouseUV()
            local mx, my = uvx * UI_WIDTH, UI_HEIGHT - (uvy * UI_HEIGHT)
            
            if IsMouseButtonDown(0) then
                -- Calculate movement delta for the stirring motion
                spoon_x, spoon_y = spoon_x + (mx - last_mx), spoon_y + (my - last_my)
                local len = math.sqrt(spoon_x^2 + spoon_y^2)
                if len > 0.1 then spoon_x, spoon_y = (spoon_x/len)*100, (spoon_y/len)*100 end
                
                local cur_ang = math.atan2(spoon_y, spoon_x)
                local delta = cur_ang - last_angle
                
                -- Wrap angle difference [-pi, pi]
                if delta > math.pi then delta = delta - 2*math.pi 
                elseif delta < -math.pi then delta = delta + 2*math.pi end
                
                spin_progress, last_angle = spin_progress + math.abs(delta), cur_ang
                
                if spinner_visual_id then 
                    SetRotation(spinner_visual_id, 0, 0, math.deg(cur_ang)) 
                end

                -- CHECK FOR COMPLETION
                if spin_progress >= SPIN_REQUIREMENT then 
                    is_finished = true
                    -- INITIALIZE TIMER HERE to prevent Nil error on next frame
                    finish_timer = FINISH_DELAY 
                end
            end
            last_mx, last_my = mx, my
        end
    end

    -- 5. FEEDBACK TEXT ANIMATION (Moved outside phases so they always clean up)
    for i = #feedback_texts, 1, -1 do
        local f = feedback_texts[i]
        f.life = f.life + dt
        if f.life >= 1.0 then 
            DestroyEntity(f.id); table.remove(feedback_texts, i)
        else
            SetPosition(f.id, f.start_x, f.start_y + (f.life * 100), 0.6)
            local s = math.max(1.0 - f.life, 0.01)
            SetScale(f.id, f.base_sx * s, f.base_sy * s, f.base_sz * s)
        end
    end
end

-- --- STOP MINIGAME ---
function Mortar.StopMinigame(targetID, success)
    is_playing = false
    is_camera_flying = false
    is_spinning = false
    is_finished = false
    
    -- 1. RESET INTERACTION TEXT
    -- This removes the "Grinding..." prompt immediately.
    SetInteractionText(my_entity_id, "Requires Ingredient")
    
    -- 2. Return Player to Original Location
    if start_cam_pos and start_cam_rot then
        TeleportPlayer(start_cam_pos.x, start_cam_pos.y, start_cam_pos.z, 0, start_cam_rot.yaw, 0)
        SetCameraRot(start_cam_rot.pitch, start_cam_rot.yaw)
    end
    
    SetUIFocused(false)
    SetCameraLock(false)
    
    -- 3. Cleanup UI Entities
    if e_progress_frame then DestroyEntity(e_progress_frame); e_progress_frame = nil end
    if e_progress_fill then DestroyEntity(e_progress_fill); e_progress_fill = nil end
    if spinner_visual_id then DestroyEntity(spinner_visual_id); spinner_visual_id = nil end
    
    for _, c in ipairs(active_circles) do DestroyEntity(c.id) end
    active_circles = {}
    
    -- 4. Handle Crafting Success
    if success and current_recipe_result_name ~= "" then
        stored_ingredients = {}
        for _, v in ipairs(visual_entities) do 
            if v ~= -1 then DestroyEntity(v) end 
        end
        visual_entities = {}
        
        local prefabName = current_recipe_result_name .. "_Prefab"
        -- Spawning at the collection coordinates
        SpawnPrefab(prefabName, -47, 7.4, 1.2) 
        Sound.Play("event:/RecipeComplete")
    end
end

-- --- INTERACT ---
function Mortar.OnInteract(senderID, targetID, actionType)
    local id_to_use = my_entity_id or targetID
    
    -- 1. Prevent interaction if already playing
    if is_playing then 
        Mortar.StopMinigame(id_to_use, false)
        return 
    end
    
    local held_item_id = GetSelectedItem()

    -- [BRANCH A] ADDING INGREDIENTS (Hand is NOT empty)
    -- This only adds items to the bowl. It DOES NOT start the camera or game.
    if held_item_id ~= -1 then
        if #stored_ingredients < MAX_INGREDIENTS then
            table.insert(stored_ingredients, held_item_id)
            ConsumeSelectedItem()
            
            local vid = -1
            if SpawnCauldronItem then 
                vid = SpawnCauldronItem(id_to_use, held_item_id) 
            end
            table.insert(visual_entities, vid)
            
            if vid ~= -1 then 
                SetRotation(vid, ROT_X, ROT_Y, ROT_Z) 
            end
            Sound.Play("event:/PlaceDownItem")
        else
            -- Optional: Play a "Full" sound if they try to add a 4th item
            print("[Mortar] Bowl is full!")
        end

    -- [BRANCH B] STARTING THE PROCESS (Hand IS empty)
    -- This only runs if the player clicks the mortar with nothing in their hand.
    elseif #stored_ingredients > 0 then
        local resID = CheckValidRecipe(stored_ingredients, "MortarPestle")
        if resID ~= -1 then
            current_recipe_result_name = GetItemNameById(resID)
            local player_id = FindEntityByName("Player")
            
            if player_id then
                -- 1. CAPTURE STARTING POINT (For the Lerp)
                local px, py, pz = GetPosition(player_id)
                local cp, cy = GetCameraRot() 
                start_cam_pos = {x = px, y = py, z = pz}
                start_cam_rot = {pitch = cp, yaw = cy}

                -- 2. DEFINE THE FIXED TARGET (Relative to Mortar)
                local mx, my, mz = GetPosition(id_to_use)
                
                target_cam_pos = {
                    x = mx + FIXED_OFFSET_X,
                    y = my + FIXED_OFFSET_Y,
                    z = mz + FIXED_OFFSET_Z
                }

                target_cam_rot = {
                    pitch = FIXED_TARGET_PITCH,
                    yaw   = FIXED_TARGET_YAW
                }

                -- 3. INITIALIZE FLIGHT
                if SetLinearVelocity then SetLinearVelocity(player_id, 0, 0, 0) end
                SetCameraLock(true)
                SetUIFocused(true)
                is_camera_flying = true
                camera_flight_t = 0.0
            end

            -- 2. RESET MINIGAME STATE
            is_playing = true
            is_spinning = false
            is_finished = false
            current_progress = 0 
            click_was_down = false 
            
            -- 3. SPAWN 2D UI PROGRESS BAR
            e_progress_frame = SpawnPrefab(PREFAB_FRAME, UI_WIDTH / 2, BAR_Y_POS, 0.8)
            e_progress_fill  = SpawnPrefab(PREFAB_FILL, UI_WIDTH / 2, BAR_Y_POS, 0.9)
            
            if e_progress_frame and e_progress_fill then
                SetIsUI(e_progress_frame, true)
                SetIsUI(e_progress_fill, true)
                SetScale(e_progress_frame, BAR_WIDTH_PX, BAR_HEIGHT_PX, 1.0)
                SetScale(e_progress_fill, 0, BAR_HEIGHT_PX * 0.8, 1.0) 
                SetPosition(e_progress_frame, UI_WIDTH / 2, BAR_Y_POS, 0.8)
                -- Anchor the fill to the left side of the frame
                SetPosition(e_progress_fill, (UI_WIDTH / 2) - (BAR_WIDTH_PX / 2), BAR_Y_POS, 0.9)
            end

            SetInteractionText(id_to_use, "Grinding...") 
            print("[Mortar] Valid recipe: " .. current_recipe_result_name .. ". Starting grind.")
        else
            -- Player clicked with empty hand but the items don't match a recipe
            Sound.Play("event:/InvalidRecipe")
            print("[Mortar] These ingredients don't make anything.")
        end
    end
end


return Mortar