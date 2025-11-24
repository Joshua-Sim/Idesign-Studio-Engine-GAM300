local M = {}

-- CONFIGURATION (Use the names exactly as they appear in your Inspector Dropdown)
local name_stir = "CauldronStiring"       
local name_idle = "cauldronwithoutspoon" 

-- State
local is_active = false
local max_distance = 5.0
local player_id = nil

-- We will cache the GUIDs here so we don't look them up every frame
local guid_stir = ""
local guid_idle = ""

function M:on_init()
    -- 1. Cache the GUIDs for performance
    guid_stir = GetAnimationGUID(name_stir)
    guid_idle = GetAnimationGUID(name_idle)
    
    print("[Lua] Cauldron Init. IDs: " .. guid_stir .. " / " .. guid_idle)

    -- 2. Start Idle
    SetAnimationAsset(self.entity, guid_idle)
    SetAnimationMode(self.entity, 0) -- Normal Mode
    
    player_id = FindEntityByName("Player")
end

function M:OnInteract()
    is_active = not is_active

    if is_active then
        print("[Lua] Stirring Started")
        SetCameraLock(true)
        
        -- SWAP TO STIRRING ASSET
        SetAnimationAsset(self.entity, guid_stir)
        SetAnimationMode(self.entity, 1) -- Stirring Mode (Mouse control)
        
        if player_id then SetGravity(player_id, 0.0) end
    else
        self:StopInteraction()
    end
end

function M:StopInteraction()
    is_active = false
    print("[Lua] Stirring Stopped")
    SetCameraLock(false)
    
    -- SWAP BACK TO IDLE ASSET
    SetAnimationAsset(self.entity, guid_idle)
    SetAnimationMode(self.entity, 0) -- Normal Mode
    
    if player_id then SetGravity(player_id, 1.0) end
end

function M:on_update(dt)
    if is_active and player_id then
        local px, py, pz = GetPosition(player_id)
        local cx, cy, cz = GetPosition(self.entity)
        local dist = math.sqrt((px - cx)^2 + (py - cy)^2 + (pz - cz)^2)

        if dist > max_distance then
            self:StopInteraction()
        end
    end
end

return M