-- footsteps.lua
-- Attach this script to your player entity

local M = {}

local stepTimer = 0
local stepInterval = 0.5    -- seconds between footsteps 

function M:on_init()
    print("[Footsteps] Script initialized for entity " .. tostring(self.entity))
end

function M:on_update(dt)
    -- Check if player is moving using input
    local isMoving = false
    
    -- WASD movement check
    if IsKeyDown(87) or IsKeyDown(83) or IsKeyDown(65) or IsKeyDown(68) then
        isMoving = true
    end
    
    if isMoving then
        stepTimer = stepTimer + dt
        if stepTimer >= stepInterval then
            -- Play footstep sound
            Sound.Play("event:/Footstep")  -- Changed back to Audio
            stepTimer = 0
        end
    else
        -- Reset timer when not moving
        stepTimer = stepInterval * 0.5
    end
end

return M