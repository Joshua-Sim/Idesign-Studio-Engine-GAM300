local M = {}

-- --- CONFIGURATION ---
M.interaction_key = 69           -- 'E' key (Place/Start)
M.chop_button = 0                -- Left Click (Chop)
M.interaction_range = 2.5

-- Herb Settings
M.place_offset_y = 0.1
M.place_offset_z = 0.3           -- How far forward on the board
M.initial_item_name = "Herb"

-- Asset Paths
M.mesh_raw    = "Assets/Objects/Herb.obj"
M.tex_raw     = "Assets/Textures/Herb.png"
M.mesh_stage1 = "Assets/Objects/ChoppedHerb1.obj"
M.tex_stage1  = "Assets/Textures/ChoppedHerb1.png"
M.mesh_stage2 = "Assets/Objects/ChoppedHerb2.obj"
M.tex_stage2  = "Assets/Textures/ChoppedHerb2.png"

-- Knife Settings
M.knife_name = "Knife"
-- IMPORTANT: Change this to the exact name of your animation clip!
-- Check your console logs: "[AnimationLoader] Loaded clip: ..."
M.knife_animation_name = "Armature|KnifeAction" 

-- Knife Offsets (Relative to Player)
M.knife_offset_right = 0.5       
M.knife_offset_up = -0.5          
M.knife_offset_forward = -0.5     

-- Knife Rotations (Degrees)
M.knife_rot_rest_x = 0
M.knife_rot_rest_y = 90.0
M.knife_rot_rest_z = 270.0

M.knife_rot_held_x = 0
M.knife_rot_held_y = 0.0
M.knife_rot_held_z = 0.0

-- --- INTERNAL STATE ---
local player_entity = nil
local current_herb_entity = nil
local knife_entity = nil
local current_health = 0
local click_was_down = false

-- New state to track if knife should follow player
local is_holding_knife = false

-- Knife Home Position
local knife_start_x = 0
local knife_start_y = 0
local knife_start_z = 0

function M:on_init()
    print("[ChoppingBoard] Initialized.")
    player_entity = FindEntityByName("Player")
    
    knife_entity = FindEntityByName(self.knife_name)
    if knife_entity then
        knife_start_x, knife_start_y, knife_start_z = GetPosition(knife_entity)
        SetRotation(knife_entity, self.knife_rot_rest_x, self.knife_rot_rest_y, self.knife_rot_rest_z)
        
        -- --- NEW: Configure Knife Animation ---
        -- Entity, Speed=10.0, Loop=false, PlayOnStart=false
        SetAnimationSettings(knife_entity, 10.0, false, false)
        -- --------------------------------------

        print("[ChoppingBoard] Knife found. Settings applied.")
    else
        print("[ChoppingBoard] Warning: Knife '" .. self.knife_name .. "' not found.")
    end
end

function M:on_update(dt)
    if not player_entity then player_entity = FindEntityByName("Player") end
    
    -- --- Update Knife Position Every Frame ---
    if is_holding_knife then
        self:UpdateKnifePosition()
    end
    -- -----------------------------------------

    if not self:IsPlayerInRange() then return end

    -- 1. Place / Start Interaction (E Key)
    if IsKeyDown(self.interaction_key) then
        if not self.key_was_down then
            self.key_was_down = true
            -- Only start if board is empty
            if current_herb_entity == nil then
                self:StartChoppingSession()
            end
        end
    else
        self.key_was_down = false
    end

    -- 2. Chop Action (Left Click)
    if IsMouseButtonDown(self.chop_button) then
        if not click_was_down then
            click_was_down = true
            -- Only chop if herb is present
            if current_herb_entity ~= nil then
                self:ChopItem()
            end
        end
    else
        click_was_down = false
    end
end

function M:UpdateKnifePosition()
    if knife_entity and player_entity then
        local px, py, pz = GetPosition(player_entity)
        
        -- Simple offset calculation (World Space)
        local kx = px + self.knife_offset_right
        local ky = py + self.knife_offset_up
        local kz = pz + self.knife_offset_forward
        
        SetPosition(knife_entity, kx, ky, kz)
    end
end

function M:IsPlayerInRange()
    if not player_entity then return false end
    local bx, by, bz = GetPosition(self.entity)
    local px, py, pz = GetPosition(player_entity)
    local dx = bx - px; local dy = by - py; local dz = bz - pz
    return math.sqrt(dx*dx + dy*dy + dz*dz) <= self.interaction_range
end

function M:StartChoppingSession()
    -- 1. Place the Herb
    local itemID = FindEntityByName(M.initial_item_name)
    if itemID == nil then 
        print("[ChoppingBoard] Herb not found in scene.")
        return 
    end

    local bx, by, bz = GetPosition(self.entity)
    SetPosition(itemID, bx, by + self.place_offset_y, bz - self.place_offset_z)
    
    current_herb_entity = itemID
    current_health = 5
    
    SetMesh(current_herb_entity, self.mesh_raw)
    SetTexture(current_herb_entity, self.tex_raw)
    print("[ChoppingBoard] Placed raw herb.")
    
    -- 2. Pick up the Knife
    if knife_entity then
        is_holding_knife = true -- Start following
        SetRotation(knife_entity, self.knife_rot_held_x, self.knife_rot_held_y, self.knife_rot_held_z)
    end
end

function M:ChopItem()
    current_health = current_health - 1 -- Decrement health (Fixed logic: was +1)
    print("[ChoppingBoard] CHOP! Health: " .. current_health)
    
    PlayEntitySound(self.entity)  -- plays whatever AudioComponent is attached

    -- --- TRIGGER ANIMATION ---
    if knife_entity then
        PlayEntityAnimation(knife_entity, self.knife_animation_name)
    end
    -- -------------------------

    if current_health == 3 then
        SetMesh(current_herb_entity, self.mesh_stage1)
        SetTexture(current_herb_entity, self.tex_stage1)
        print("[ChoppingBoard] Herb -> Stage 1")

    elseif current_health == 1 then
        SetMesh(current_herb_entity, self.mesh_stage2)
        SetTexture(current_herb_entity, self.tex_stage2)
        print("[ChoppingBoard] Herb -> Stage 2")

    elseif current_health <= 0 then
        self:EndChoppingSession()
    end
end

function M:EndChoppingSession()
    print("[ChoppingBoard] Finished! Herb collected.")

    -- 1. Destroy the Herb entity
    if current_herb_entity then
        DestroyEntity(current_herb_entity)
        current_herb_entity = nil
    end
    
    -- 2. Return Knife to Home
    if knife_entity then
        is_holding_knife = false -- Stop following
        SetPosition(knife_entity, knife_start_x, knife_start_y, knife_start_z)
        SetRotation(knife_entity, self.knife_rot_rest_x, self.knife_rot_rest_y, self.knife_rot_rest_z)
    end
end

return M