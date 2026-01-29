--  Animal Wander AI
local M = {}

M.wanderRange = 10.0
M.waitMin = 2.0
M.waitMax = 5.0
M.arrivalDist = 1.5

M.timer = 0.0
M.targetPos = nil
M.waiting = true

function M:on_init()
    M.timer = math.random() * (M.waitMax - M.waitMin) + M.waitMin
end

function M:on_update(dt)
    if M.waiting then
        M.timer = M.timer - dt
        if M.timer <= 0 then
            -- Pick random nearby position
            local x, y, z = GetPosition(self.entity)
            local newX = x + (math.random() - 0.5) * 2 * M.wanderRange
            local newZ = z + (math.random() - 0.5) * 2 * M.wanderRange
            
            -- Pathfind there!
            PathfindToPos(self.entity, newX, y, newZ)
            
            M.targetPos = {x = newX, y = y, z = newZ}
            M.waiting = false
        end
    else
        -- Check if arrived
        local x, y, z = GetPosition(self.entity)
        local dx = M.targetPos.x - x
        local dz = M.targetPos.z - z
        local dist = math.sqrt(dx*dx + dz*dz)
        
        if dist <= M.arrivalDist then
            M.waiting = true
            M.timer = math.random() * (M.waitMax - M.waitMin) + M.waitMin
        end
    end
end


return M