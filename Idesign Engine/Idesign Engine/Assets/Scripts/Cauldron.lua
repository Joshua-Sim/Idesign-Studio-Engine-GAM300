local M = {}

-- =========================================================
-- [INPUT HELPER]
-- =========================================================
local last_mouse_x = 0
local last_mouse_y = 0
local first_frame = true

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

-- CONFIGURATION
local name_stir = "CauldronStiring"
local name_idle = "cauldronwithoutspoon"
local prefab_frame = "ProgressBarFrame_Prefab"
local prefab_fill = "ProgressBarFill_Prefab"

local max_distance = 5.0

-- [STIRRING SETTINGS]
local stir_sensitivity = 0.005

-- [TIMER SETTINGS]
local finish_delay = 1        
local current_finish_timer = 0.0
local is_finished = false       

-- VISUAL SETTINGS
local bar_length_max = 0.5
local bar_thickness  = 0.2

-- OFFSET SETTINGS
local y_offset       = 1.5 
local x_offset_right = -0.5 
local tune_x = 1.6
local tune_y = -0.04

-- [BILLBOARD DEPTH ADJUST]
-- Positive (+) moves towards the player
-- Negative (-) moves away from the player
local billboard_depth_adjust = 0.055

-- STATE
local is_active = false
local player_id = nil
local guid_stir = ""
local guid_idle = ""

-- PROGRESS BAR STATE
local entity_frame = nil
local entity_fill = nil
local progress = 0.0

-- VIRTUAL SPOON STATE
local spoon_x = 0.0
local spoon_y = 100.0
local last_angle = 0.0

-- FORWARD DECLARATIONS
local SpawnBar
local StopInteraction
local UpdateBillboard

function M:on_init()
    guid_stir = GetAnimationGUID(name_stir)
    guid_idle = GetAnimationGUID(name_idle)
    
    PlayEntityAnimation(self.entity, name_idle)
    SetAnimationMode(self.entity, 0)
    
    player_id = FindEntityByName("Player")
    last_angle = math.atan2(spoon_y, spoon_x)
end

function M.OnInteract(entityID)
    local myEntity = entityID 
    is_active = not is_active

    if is_active then
        print("[Lua] Stirring Started")
        progress = 0.0 
        current_finish_timer = 0.0
        is_finished = false
        
        spoon_x = 0.0
        spoon_y = 100.0
        last_angle = math.atan2(spoon_y, spoon_x)
        
        SetCameraLock(true)
        SetAnimationAsset(myEntity, guid_stir)
        SetAnimationMode(myEntity, 1)
        SpawnBar(myEntity)
    else
        StopInteraction(myEntity)
    end
end

function SpawnBar(cauldronID)
    entity_frame = SpawnPrefab(prefab_frame, cauldronID)
    if entity_frame then
        SetScale(entity_frame, bar_length_max, bar_thickness, bar_thickness*5)
    end

    entity_fill = SpawnPrefab(prefab_fill, cauldronID)
    if entity_fill then
        SetScale(entity_fill, 0.0, bar_thickness, bar_thickness)
    end
end

function StopInteraction(e)
    is_active = false
    is_finished = false
    print("[Lua] Stirring Stopped")
    SetCameraLock(false) 
    SetAnimationAsset(e, guid_idle)
    SetAnimationMode(e, 0) 
    if entity_frame then DestroyEntity(entity_frame); entity_frame = nil end
    if entity_fill then DestroyEntity(entity_fill); entity_fill = nil end
end

-- [FIXED BILLBOARD FUNCTION]
function UpdateBillboard(px, pz, cx, cz, cy)
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
        
        -- 3. Calculate SIDEWAYS Offset (Left/Right)
        -- This moves the bar to the left/right edge of the frame
        local right_angle = angle_rad + (math.pi / 2.0)
        local side_dist = (bar_length_max * tune_x)
        
        local side_offset_x = side_dist * math.sin(right_angle)
        local side_offset_z = side_dist * math.cos(right_angle)

        -- 4. Calculate DEPTH Offset (Forward/Back) [THE FIX]
        -- This moves the bar "Forward" towards the player (or back) based on angle
        -- We use angle_rad because that vector points AT the player
        local depth_offset_x = billboard_depth_adjust * math.sin(angle_rad)
        local depth_offset_z = billboard_depth_adjust * math.cos(angle_rad)

        -- 5. Combine
        local fill_x = frame_pos_x + side_offset_x + depth_offset_x
        local fill_y = frame_pos_y + tune_y
        local fill_z = frame_pos_z + side_offset_z + depth_offset_z

        SetPosition(entity_fill, fill_x, fill_y, fill_z)
    end
end

function M:on_update(dt)
    if is_active and player_id then
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(self.entity)
        local dist = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

        if dist > max_distance then
            StopInteraction(self.entity)
            return
        end

        if entity_fill and entity_frame then
            
            UpdateBillboard(px, pz, cx, cz, cy)

            if is_finished then
                current_finish_timer = current_finish_timer - dt
                SetAnimationSettings(self.entity, 0.0, true, true) 
                if current_finish_timer <= 0 then
                    StopInteraction(self.entity)
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
                SetAnimationSettings(self.entity, 1.0, true, true) 
            else
                SetAnimationSettings(self.entity, 0.0, true, true) 
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

return M