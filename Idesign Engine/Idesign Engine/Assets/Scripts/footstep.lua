-- footstep.lua
local M = {}

local stepTimer = 0
local stepInterval = 0.5

local indoorEvent = "event:/Footstep"
local outdoorEvent = "event:/FootstepBackyard"

-- Backyard bounds (calculated from your scene data)
-- Backyard position: (-45.93, 5.3, 14.72)
-- Backyard collider: size (19.5, 5.8, 20), center offset (-15.3, 2.8, -5)
-- Actual bounds in world space:
local backyardMin = { 
    x = -45.93 + (-15.3 - 19.5/2),  -- -70.68
    y = 5.3 + (2.8 - 5.8/2),          -- 5.2
    z = 14.72 + (-5 - 20/2)           -- -0.28
}
local backyardMax = { 
    x = -45.93 + (-15.3 + 19.5/2),  -- -51.48
    y = 5.3 + (2.8 + 5.8/2),          -- 11.2
    z = 14.72 + (-5 + 20/2)           -- 19.72
}

function M:on_init()
    print("[Footsteps] Script initialized for entity " .. tostring(self.entity))
    print(string.format("[Footsteps] Backyard bounds: X(%.2f to %.2f), Y(%.2f to %.2f), Z(%.2f to %.2f)", 
        backyardMin.x, backyardMax.x, backyardMin.y, backyardMax.y, backyardMin.z, backyardMax.z))
end

function M:IsInBackyard()
    local px, py, pz = GetPosition(self.entity)
    
    local inBackyard = px >= backyardMin.x and px <= backyardMax.x and
                       py >= backyardMin.y and py <= backyardMax.y and
                       pz >= backyardMin.z and pz <= backyardMax.z
    
    return inBackyard
end

function M:on_update(dt)
    -- Check if player is moving
    local isMoving = false
    
    if IsKeyDown(87) or IsKeyDown(83) or IsKeyDown(65) or IsKeyDown(68) then
        isMoving = true
    end
    
    if isMoving then
        stepTimer = stepTimer + dt
        if stepTimer >= stepInterval then
            -- Check zone and play appropriate footstep
            local isInBackyard = self:IsInBackyard()
            local eventToPlay = isInBackyard and outdoorEvent or indoorEvent
            
            Sound.Play(eventToPlay)
            stepTimer = 0
            
            -- Debug output (optional - remove later)
            print("[Footsteps] Playing: " .. eventToPlay .. " | In backyard: " .. tostring(isInBackyard))
        end
    else
        stepTimer = stepInterval * 0.5
    end
end

return M