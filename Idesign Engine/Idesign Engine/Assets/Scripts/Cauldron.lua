local M = {}

-- =============================================================
-- CONFIGURATION
-- =============================================================
local name_stir = "CauldronStiring"       
local name_idle = "cauldronwithoutspoon" 

-- Animation Settings
local orbit_radius = 0.5    -- How wide the circle is
local orbit_speed = 1.5     -- How fast they spin
local bob_speed = 1.0       -- How fast they bounce up/down
local bob_height = 0.05     -- How high they bounce
local height_offset = 1.2   -- Height above the cauldron pivot point

-- =============================================================
-- STATE VARIABLES
-- =============================================================
local my_entity_id = nil
local is_active = false
local max_distance = 5.0
local player_id = nil
local accumulated_time = 0.0 -- CRITICAL: Must be defined here
local debug_timer = 0.0      -- CRITICAL: Must be defined here
local CAULDRON_NAME = "Cauldron" -- Check your Inspector for the exact name!

-- GUID Cache
local guid_stir = ""
local guid_idle = ""

-- Storage
local stored_ingredients = {} 
local visual_entities = {}    

local item_names = {
    [0] = "unchopped_herb",
    [1] = "chopped_herb"
}

-- [NEW] MANUAL CENTERING OFFSETS
-- Change these if the items aren't spinning around the visual center
local center_offset_x = -0.5  -- Try 0.1 or -0.1 to shift left/right
local center_offset_z = -0.3  -- Try 0.1 or -0.1 to shift forward/back

-- =============================================================
-- HELPER: Resolve ID (Fixes Table vs Number issue)
-- =============================================================
local function ResolveID(arg1, arg2)
    if type(arg1) == "number" then return arg1 end
    if type(arg2) == "number" then return arg2 end
    return nil
end

-- =============================================================
-- 1. INIT
-- =============================================================
function M.on_init(arg1, arg2)
    local entity_id = ResolveID(arg1, arg2)
    if not entity_id then return end

    my_entity_id = entity_id 

    guid_stir = GetAnimationGUID(name_stir)
    guid_idle = GetAnimationGUID(name_idle)
    
    print("[Lua] Cauldron Init. ID: " .. my_entity_id)

    SetAnimationAsset(my_entity_id, guid_idle)
    SetAnimationMode(my_entity_id, 0)
    
    player_id = FindEntityByName("Player")
end

-- =============================================================
-- 2. INTERACT
-- =============================================================
function M.OnInteract(arg1, arg2)
    -- 1. RESOLVE ID (The fix)
    local id_to_use = ResolveID(arg1, arg2)

    -- [SELF-REPAIR] If on_init failed, capture the ID now!
    if not my_entity_id and id_to_use then
        my_entity_id = id_to_use
        print("[Lua DEBUG] Recovered missing Entity ID: " .. my_entity_id)
        
        -- We should also ensure init logic runs if it was skipped
        if guid_idle == "" then
             guid_stir = GetAnimationGUID(name_stir)
             guid_idle = GetAnimationGUID(name_idle)
        end
    end

    -- Fallback if we still don't have an ID
    if not id_to_use then 
        id_to_use = my_entity_id 
    end

    print("[Lua DEBUG] Interact Triggered on ID: " .. tostring(id_to_use))

    -- ... (The rest of your function stays exactly the same) ...
    local held_item_id = GetSelectedItem()
    
    if held_item_id ~= -1 then
         -- ... (Your drop item logic) ...
         table.insert(stored_ingredients, held_item_id)
         ConsumeSelectedItem()
         
         if SpawnCauldronItem then
            local visual_id = SpawnCauldronItem(id_to_use, held_item_id)
            if visual_id then 
                table.insert(visual_entities, visual_id) 
            end
         end
         return
    end

    -- Stirring Logic
    is_active = not is_active
    if is_active then
        SetCameraLock(true)
        SetAnimationAsset(id_to_use, guid_stir)
        SetAnimationMode(id_to_use, 1) 
        if player_id then SetGravity(player_id, 0.0) end
    else
        M.StopInteraction(id_to_use)
    end
end
function M.StopInteraction(entity_id)
    local id = entity_id or my_entity_id
    is_active = false
    print("[Lua DEBUG] Stirring Stopped")
    SetCameraLock(false)
    SetAnimationAsset(id, guid_idle)
    SetAnimationMode(id, 0) 
    if player_id then SetGravity(player_id, 1.0) end
end

-- =============================================================
-- 3. UPDATE LOOP (Animation)
-- =============================================================
function M.on_update(arg1, arg2)
    -- Resolve DT
    --print("Update Running!")
    local dt = 0.016
    if type(arg1) == "number" then dt = arg1 end
    if type(arg2) == "number" then dt = arg2 end
    
    -- Safe Accumulate
    accumulated_time = accumulated_time + dt

    -- [FIX] LAZY LOAD CAULDRON ID
    if not my_entity_id then
        -- Attempt to find myself by name if on_init failed
        my_entity_id = FindEntityByName(CAULDRON_NAME)
        
        if my_entity_id then
            print("[Lua DEBUG] Recovered Cauldron ID via Name: " .. my_entity_id)
        else
            -- If still nil, we can't do anything
            return 
        end
    end

    -- ANIMATION LOGIC
    local count = #visual_entities
    if count > 0 then
        local raw_cx, raw_cy, raw_cz = GetPosition(my_entity_id)
        local cx = raw_cx + center_offset_x;
        local cy = raw_cy;
        local cz = raw_cz + center_offset_z;
        local angle_step = (2 * math.pi) / count
        
        for i, vis_id in ipairs(visual_entities) do
            local current_angle = (accumulated_time * orbit_speed) + ((i - 1) * angle_step)
            
            local offset_x = math.cos(current_angle) * orbit_radius
            local offset_z = math.sin(current_angle) * orbit_radius
            local offset_y = math.sin((accumulated_time * bob_speed) + i) * bob_height
            
            -- Move Item
            if SetPosition then
                SetPosition(vis_id, cx + offset_x, cy + height_offset + offset_y, cz + offset_z)
            else
                if debug_timer > 2.0 then print("[Lua ERROR] SetPosition missing!") end
            end
            
            -- Rotate Item
            if SetRotation then
                SetRotation(vis_id, 0.0, accumulated_time * 50.0, 0.0) 
            end
        end
    end

    -- Reset debug timer
    if debug_timer > 2.0 then debug_timer = 0.0 end

    -- Distance Check
    if is_active and player_id then
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(my_entity_id) 
        
        local dist = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

        if dist > max_distance then
            M.StopInteraction(my_entity_id)
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