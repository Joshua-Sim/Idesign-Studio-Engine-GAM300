-- FloatUpAndDelete.lua
-- This script makes the attached float up and delete itself (Text and logs popups)

-- FloatUpAndDelete.lua
-- This script makes the attached entity float up and delete itself (Text and logs popups)

local FloatUpAndDelete = {}

-- Configuration
local float_speed = 0.5          -- Units per second to move upward
local lifetime = 3.0              -- Seconds before destruction
local fade_start_time = 2.0       -- When to start fading (seconds)

-- State
local elapsed_time = 0.0
local initial_y = nil

function FloatUpAndDelete.on_init(self)
    print("[FloatUpAndDelete] Initialized for entity: " .. tostring(self.entity))
    
    -- Store initial Y position
    local x, y, z = GetPosition(self.entity)
    initial_y = y
    
    elapsed_time = 0.0
end

function FloatUpAndDelete.on_update(self, dt)
    elapsed_time = elapsed_time + dt
    
    -- Get current position
    local x, y, z = GetPosition(self.entity)
    
    -- Move upward
    local new_y = initial_y + (float_speed * elapsed_time)
    SetPosition(self.entity, x, new_y, z)
    
    -- Handle fading (if elapsed_time >= fade_start_time)
    if elapsed_time >= fade_start_time then
        local fade_duration = lifetime - fade_start_time
        local fade_progress = (elapsed_time - fade_start_time) / fade_duration
        local opacity = 1.0 - fade_progress  -- 1.0 -> 0.0
        
        -- Clamp opacity
        if opacity < 0.0 then opacity = 0.0 end
        
        -- Update text opacity
        SetTextComponent(self.entity, "opacity", opacity)
    end
    
    -- Destroy when lifetime is reached
    if elapsed_time >= lifetime then
        print("[FloatUpAndDelete] Lifetime reached, destroying entity: " .. self.entity)
        DestroyEntity(self.entity)
    end
end

return FloatUpAndDelete