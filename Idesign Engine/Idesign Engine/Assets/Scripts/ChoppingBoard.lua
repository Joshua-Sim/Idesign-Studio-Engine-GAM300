local M = {}

-- Configuration
M.interaction_key = 69           -- 'E' key
M.chop_button = 0                -- Left Click
M.interaction_range = 2.5
M.place_offset_y = 0.1
M.place_offset_z = 0.3
M.initial_item_name = "Herb"

-- Asset Paths (These must match your file structure EXACTLY)
M.mesh_raw    = "Assets/Objects/Herb.obj"
M.tex_raw     = "Assets/Textures/Herb.png"

M.mesh_stage1 = "Assets/Objects/ChoppedHerb1.obj"
M.tex_stage1  = "Assets/Textures/ChoppedHerb1.png"

M.mesh_stage2 = "Assets/Objects/ChoppedHerb2.obj"
M.tex_stage2  = "Assets/Textures/ChoppedHerb2.png"

-- Internal State
local player_entity = nil
local current_herb_entity = nil
local current_health = 0
local click_was_down = false

function M:on_init()
    print("[ChoppingBoard] Initialized.")
    player_entity = FindEntityByName("Player")
end

function M:on_update(dt)
    if not player_entity then player_entity = FindEntityByName("Player") end
    if not self:IsPlayerInRange() then return end

    -- 1. Place
    if IsKeyDown(self.interaction_key) then
        if not self.key_was_down then
            self.key_was_down = true
            if current_herb_entity == nil then
                self:PlaceItem()
            end
        end
    else
        self.key_was_down = false
    end

    -- 2. Chop
    if IsMouseButtonDown(self.chop_button) then
        if not click_was_down then
            click_was_down = true
            if current_herb_entity ~= nil then
                self:ChopItem()
            end
        end
    else
        click_was_down = false
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

function M:PlaceItem()
    local itemID = FindEntityByName(M.initial_item_name)
    if itemID == nil then return end

    local bx, by, bz = GetPosition(self.entity)
    
    -- Move item to board
    SetPosition(itemID, bx, by + self.place_offset_y, bz - self.place_offset_z)
    
    current_herb_entity = itemID
    current_health = 5
    
    -- Ensure it starts raw
    SetMesh(current_herb_entity, self.mesh_raw)
    SetTexture(current_herb_entity, self.tex_raw)
    
    print("[ChoppingBoard] Placed raw herb.")
end

function M:ChopItem()
    current_health = current_health - 1
    print("[ChoppingBoard] CHOP! Health: " .. current_health)

    if current_health == 3 then
        -- Change to Stage 1
        SetMesh(current_herb_entity, self.mesh_stage1)
        SetTexture(current_herb_entity, self.tex_stage1)
        print("[ChoppingBoard] Visuals updated to Stage 1")

    elseif current_health == 1 then
        -- Change to Stage 2
        SetMesh(current_herb_entity, self.mesh_stage2)
        SetTexture(current_herb_entity, self.tex_stage2)
        print("[ChoppingBoard] Visuals updated to Stage 2")

    elseif current_health <= 0 then
        -- Finished
        DestroyEntity(current_herb_entity)
        current_herb_entity = nil
        print("[ChoppingBoard] Finished! Herb collected.")
    end
end

return M