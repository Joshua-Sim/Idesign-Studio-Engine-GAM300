-- FloatUpAndDelete.lua
-- This script makes the attached entity float up and delete itself (Text and logs popups)

-- FloatUpAndDelete.lua
local FloatUpAndDelete = {}

-- Configuration
local float_speed = 0.05
local lifetime = 2.0

-- State
local elapsed_time = 0.0
local initial_y = nil
local is_destroyed = false  -- ADD THIS FLAG

function FloatUpAndDelete.on_init(self)
    print("[FloatUpAndDelete] Initialized for entity: " .. tostring(self.entity))
    
    local x, y, z = GetPosition(self.entity)
    initial_y = y
    elapsed_time = 0.0
    is_destroyed = false
end

function FloatUpAndDelete.on_update(self, dt)
    -- GUARD: Don't run if already destroyed
    if is_destroyed then
        return
    end
    
    elapsed_time = elapsed_time + dt
    
    -- Get current position
    local x, y, z = GetPosition(self.entity)
    
    -- Move upward
    local new_y = initial_y + (float_speed * elapsed_time)
    SetPosition(self.entity, x, new_y, z)
    
    -- Destroy when lifetime is reached
    if elapsed_time >= lifetime then
        print("[FloatUpAndDelete] Lifetime reached, destroying entity: " .. self.entity)
        is_destroyed = true  -- SET FLAG FIRST
        DestroyEntity(self.entity)
        return
    end
end

return FloatUpAndDelete