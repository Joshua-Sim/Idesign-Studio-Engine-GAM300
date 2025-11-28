local M = {}

-- --- CONFIGURATION ---
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
M.mesh_stage2 = "Assets/Objects/chopped_herb.obj"
M.tex_stage2  = "Assets/Textures/ChoppedHerb2.png"

-- Knife Settings
M.knife_name = "Knife"
-- IMPORTANT: Change this to the exact name of your animation clip!
-- Check your console logs: "[AnimationLoader] Loaded clip: ..."
M.knife_animation_name = "Armature|KnifeAction" 

-- Knife Offsets (Relative to Player)
M.knife_offset_right = 0.8       
M.knife_offset_up = -0.5          
M.knife_offset_forward = 1    

-- Knife Rotations (Degrees)
M.knife_rot_rest_x = 0
M.knife_rot_rest_y = 90.0
M.knife_rot_rest_z = 270.0

M.knife_rot_held_x = 0
M.knife_rot_held_y = -90.0 
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
    
end

-- Change ':' to '.' to prevent Lua from expecting 'self' as the first arg
-- We accept 'entityID' because C++ passes the entity ID as the first argument
function M.OnInteract(entityID)
    print("[ChoppingBoard] Raycast Interaction Received from Entity: " .. tostring(entityID))

    -- 1. Check if board is occupied
    -- Note: We access current_herb_entity directly (it's a local variable)
    if current_herb_entity ~= nil then
        print("[ChoppingBoard] Board is already occupied.")
        return
    end

    -- 2. Get the item name
    local itemName = GetSelectedItemName()

    if itemName then
        print("[ChoppingBoard] Player is holding: " .. itemName)
        
        -- 3. Pass the name to the start function
        -- CRITICAL FIX: Use 'M' explicitly instead of 'self'
        if(itemName == "unchopped_herb") then
            M:StartChoppingSession(itemName)
            
            -- (Optional) Remove item from player inventory here
            -- RemoveSelectedItem() 
            
            ConsumeSelectedItem()
        end
    else
        print("[ChoppingBoard] Player is holding nothing.")
    end
end

function M:on_update(dt)
    if not player_entity then player_entity = FindEntityByName("Player") end
    

    -- --- CHOPPING LOGIC (Left Click) ---
    -- We still check range here because raycast doesn't handle clicks yet
    if IsMouseButtonDown(self.chop_button) then
        if not self.click_was_down then
            self.click_was_down = true
            local heldItemName = GetSelectedItemName()
            -- Only chop if:
            -- 1. There is a herb
            -- 2. Player is close enough
            if current_herb_entity ~= nil and self:IsPlayerInRange() and heldItemName == "knife" then
                self:ChopItem()
            end
        end
    else
        self.click_was_down = false
    end
end

function M:IsPlayerInRange()
    if not player_entity then return false end
    local bx, by, bz = GetPosition(self.entity)
    local px, py, pz = GetPosition(player_entity)
    -- Simple distance check
    local dx = bx - px; local dy = by - py; local dz = bz - pz
    return math.sqrt(dx*dx + dy*dy + dz*dz) <= self.interaction_range
end


function M:StartChoppingSession(itemName)
    -- 1. Construct the Prefab Name dynamically
    -- "unchopped_herb" + "_prefab" -> "unchopped_herb_prefab"
    local prefabName = itemName .. "_Prefab"
    
    print("[ChoppingBoard] Attempting to spawn: " .. prefabName)

    -- Get the position of the Chopping Board (self.entity)
    local bx, by, bz = GetPosition(self.entity)

    -- Calculate spawn position by adding the board's position to your offsets
    -- NOTE: If your board is rotated 90 degrees, you may need to add offset_z to 'bx' instead of 'bz'
    local spawnX = bx 
    local spawnY = by + self.place_offset_y
    local spawnZ = bz + self.place_offset_z

    -- Spawn at the calculated relative coordinates
    local newEntityID = SpawnPrefab(prefabName, spawnX, spawnY, spawnZ)

    -- 3. Validate Spawn
    if newEntityID ~= -1 and newEntityID ~= nil then
        print("[ChoppingBoard] Spawn successful! ID: " .. newEntityID)
        
        -- 4. Setup Chopping State
        current_herb_entity = newEntityID
        current_health = 5
        
        -- Note: If your prefab already has the correct mesh/texture, 
        -- you don't strictly need SetMesh/SetTexture here, but keeping them 
        -- ensures it resets to "Raw" state if you are reusing logic.
        SetMesh(current_herb_entity, self.mesh_raw)
        SetTexture(current_herb_entity, self.tex_raw)

    else
        print("[ChoppingBoard] ERROR: Failed to spawn prefab '" .. prefabName .. "'")
    end
end

function M:ChopItem()
    current_health = current_health - 1
    print("[ChoppingBoard] CHOP! Health: " .. current_health)
    
    PlayEntitySound(self.entity)  -- plays whatever AudioComponent is attached

    if knife_entity then
        PlayEntityAnimation(knife_entity, self.knife_animation_name)
    else
        print("[ChoppingBoard] Warning: No knife entity found to animate.")
    end

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

    if current_herb_entity then
        

        -- 2. Try to add it
        local slot = AddToInventory("chopped_herb")

        -- 3. Check result
        if slot >= 0 then
            print("Lua: Success! Added to slot " .. slot)
        elseif slot == -1 then
            -- This assumes your C++ returns -1 for full, but let's verify
            print("Lua Error: AddToInventory returned -1 (Inventory Full OR Item Not Found)")
        end    
        DestroyEntity(current_herb_entity)
        current_herb_entity = nil
    end

end

return M