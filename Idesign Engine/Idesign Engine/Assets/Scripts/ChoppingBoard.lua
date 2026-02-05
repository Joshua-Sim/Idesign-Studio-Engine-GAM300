local M = {}

-- --- CONFIGURATION ---
M.chop_button = 0                
M.interaction_range = 2.5
M.max_health = 5         

-- Minigame Settings
M.marker_speed_base = 500.0     -- Starting Speed
M.marker_speed_increase = 150.0 -- How much faster per hit?
M.marker_limit = 540.0          -- Boundary
M.center_x = 960         
M.center_y = 340         

-- Collision Settings
M.zone_collision_width = 128 
M.zone_safe_margin = 460    

-- Herb Settings
M.place_offset_y = 0.1
M.place_offset_z = 0.3           
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
M.knife_animation_name = "Armature|KnifeAction" 

M.ui_prefabs = {
    "UI_SkillBar_BG_Prefab",
    "UI_SkillBar_Zone_Prefab",
    "UI_SkillBar_Marker_Prefab"
}

M.knife_offset_right = 0.8; M.knife_offset_up = -0.5; M.knife_offset_forward = 1    
M.knife_rot_rest_x = 0; M.knife_rot_rest_y = 90.0; M.knife_rot_rest_z = 270.0
M.knife_rot_held_x = 0; M.knife_rot_held_y = -90.0; M.knife_rot_held_z = 0.0

-- --- INTERNAL STATE ---
local player_entity = nil
local current_herb_entity = nil
local knife_entity = nil
local current_health = 0
local click_was_down = false

-- UI State
local spawned_ui_ids = {}
local ui_active = false
local active_marker_id = nil 
local active_zone_id = nil   

-- Movement & Game State
local marker_pos_x = 0.0    
local marker_dir = 1        
local zone_pos_x = 0.0 
local current_speed = 500.0 -- Tracks the current active speed

function M:on_init()
    print("[ChoppingBoard] Initialized.")
    player_entity = FindEntityByName("Player")
    knife_entity = FindEntityByName(self.knife_name)
    math.randomseed(os.time()) 
end

function M.OnInteract(entityID)
    if current_herb_entity ~= nil then return end

    local itemName = GetSelectedItemName()
    if itemName and itemName == "unchopped_herb" then
        M:StartChoppingSession(itemName)
        ConsumeSelectedItem()
        Sound.Play("event:/PlaceDownItem")
    end
end

function M:on_update(dt)
    if not player_entity then player_entity = FindEntityByName("Player") end
    
    -- --- 1. MINIGAME ANIMATION LOOP ---
    if ui_active and active_marker_id ~= nil then
        
        -- Use 'current_speed' instead of the static config
        marker_pos_x = marker_pos_x + (current_speed * marker_dir * dt)

        if marker_pos_x > M.marker_limit then
            marker_pos_x = M.marker_limit
            marker_dir = -1 
        elseif marker_pos_x < -M.marker_limit then
            marker_pos_x = -M.marker_limit
            marker_dir = 1  
        end

        SetPosition(active_marker_id, M.center_x + marker_pos_x, M.center_y, 0.5)
    end
    -- ----------------------------------

    -- --- 2. INPUT HANDLING ---
    if IsMouseButtonDown(self.chop_button) then
        if not self.click_was_down then
            self.click_was_down = true
            local heldItemName = GetSelectedItemName()
            
            if current_herb_entity ~= nil and self:IsPlayerInRange() and heldItemName == "knife" then
                self:AttemptChop() 
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
    local dx = bx - px; local dy = by - py; local dz = bz - pz
    return math.sqrt(dx*dx + dy*dy + dz*dz) <= self.interaction_range
end

function M:StartChoppingSession(itemName)
    local prefabName = itemName .. "_Prefab"
    local bx, by, bz = GetPosition(self.entity)
    local newEntityID = SpawnPrefab(prefabName, bx, by + M.place_offset_y, bz + M.place_offset_z)

    if newEntityID ~= -1 and newEntityID ~= nil then
        current_herb_entity = newEntityID
        current_health = 5
        self:ClearUI() 
        SetMesh(current_herb_entity, self.mesh_raw)
        SetTexture(current_herb_entity, self.tex_raw)
        
        -- Reset speed when starting a new herb
        current_speed = M.marker_speed_base
    end
end

function M:AttemptChop()
    if not ui_active then
        self:StartMinigameUI()
        return
    end

    local diff = math.abs(marker_pos_x - zone_pos_x)
    local hit_threshold = M.zone_collision_width / 2

    if diff <= hit_threshold then
        print("[Minigame] HIT! Distance: " .. diff)
        self:ExecuteSuccessfulChop()
    else
        print("[Minigame] MISS! Distance: " .. diff)
        
        -- PENALTY: Decrease Speed on Miss
        current_speed = current_speed - M.marker_speed_increase
        
        -- Clamp so it never goes below base speed
        if current_speed < M.marker_speed_base then
            current_speed = M.marker_speed_base
        end
        
        print("[Minigame] Speed Decreased to: " .. current_speed)
    end
end

function M:StartMinigameUI()
    print("[ChoppingBoard] Starting Minigame...")
    ui_active = true
    marker_pos_x = 0 
    marker_dir = 1   
    
    -- Ensure speed is reset if UI is reopened
    current_speed = M.marker_speed_base
    
    zone_pos_x = math.random(-M.zone_safe_margin, M.zone_safe_margin)

    for _, prefabName in ipairs(M.ui_prefabs) do
        local z_layer = 0.1
        local spawn_y = M.center_y
        local spawn_x = M.center_x

        if prefabName == "UI_SkillBar_BG_Prefab" then z_layer = 0.0      
        elseif prefabName == "UI_SkillBar_Marker_Prefab" then z_layer = 0.5 
        elseif prefabName == "UI_SkillBar_Zone_Prefab" then
            spawn_x = M.center_x + zone_pos_x 
        end
        
        local uiID = SpawnPrefab(prefabName, spawn_x, spawn_y, z_layer)
        
        if uiID ~= -1 then
            table.insert(spawned_ui_ids, uiID)
            
            if prefabName == "UI_SkillBar_Marker_Prefab" then active_marker_id = uiID end
            if prefabName == "UI_SkillBar_Zone_Prefab" then active_zone_id = uiID end
        end
    end
end

function M:ExecuteSuccessfulChop()
    current_health = current_health - 1
    
    -- BONUS: Increase Speed on Hit!
    current_speed = current_speed + M.marker_speed_increase
    print("[Minigame] Speed Increased to: " .. current_speed)
    
    PlayEntitySound(self.entity)
    if knife_entity then PlayEntityAnimation(knife_entity, self.knife_animation_name) end

    if current_health <= 0 then
        self:EndChoppingSession()
        return
    end

    if current_health == 3 then
        SetMesh(current_herb_entity, self.mesh_stage1)
        SetTexture(current_herb_entity, self.tex_stage1)
    elseif current_health == 1 then
        SetMesh(current_herb_entity, self.mesh_stage2)
        SetTexture(current_herb_entity, self.tex_stage2)
    end

    if active_zone_id then
        zone_pos_x = math.random(-M.zone_safe_margin, M.zone_safe_margin)
        SetPosition(active_zone_id, M.center_x + zone_pos_x, M.center_y, 0.1)
    end
end

function M:EndChoppingSession()
    if current_herb_entity then
        if AddToInventory("chopped_herb") >= 0 then
            print("Lua: Added to inventory.")
        end    
        DestroyEntity(current_herb_entity)
        current_herb_entity = nil
        self:ClearUI()
    end
end

function M:ClearUI()
    if #spawned_ui_ids > 0 then
        for _, id in ipairs(spawned_ui_ids) do DestroyEntity(id) end
    end
    spawned_ui_ids = {} 
    ui_active = false
    active_marker_id = nil 
    active_zone_id = nil
end

return M