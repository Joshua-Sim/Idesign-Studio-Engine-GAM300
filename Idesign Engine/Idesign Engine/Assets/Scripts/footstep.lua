-- footstep.lua
local M = {}

local stepTimer = 0
local stepInterval = 0.5

local indoorEvent = "event:/Footstep"
local outdoorEvent = "event:/FootstepBackyard"
local quarryEvent = "event:/QuarryAmbient"

--Track state
local wasInBackyard = false
local backyardEntity = nil
local backyardCopyEntity = nil
local frogEntity = nil 
local quarryPlaying = false

-- Forest/Backyard bounds (measured in-game)
local backyardMin = { 
    x = -296, 
    y = 300, 
    z = 533 
}

local backyardMax = { 
    x = -120, 
    y = 330, 
    z = 726 
}

-- Quarry bounds (calculated from QuarryGround collider)
local quarryMin = { 
    x = -246.3, 
    y = -45.9, 
    z = -174.8 
}

local quarryMax = { 
    x = -143.3, 
    y = 54.2,  
    z = -72.8  
}

function M:on_init()
    print("[Footsteps] Script initialized for entity " .. tostring(self.entity))
    print(string.format("[Footsteps] Backyard bounds: X(%.2f to %.2f), Y(%.2f to %.2f), Z(%.2f to %.2f)", 
        backyardMin.x, backyardMax.x, backyardMin.y, backyardMax.y, backyardMin.z, backyardMax.z))

    -- Find both backyard entities for volume control
    backyardEntity = FindEntityByName("Backyard")
    backyardCopyEntity = FindEntityByName("Backyard (Copy)")

    if backyardEntity then
        print("[Footsteps] Found Backyard entity: " .. backyardEntity)
    else
        print("[Footsteps] WARNING: Could not find Backyard entity!")
    end 

    if backyardCopyEntity then
        print("[Footsteps] Found Backyard (Copy) entity: " .. backyardCopyEntity)
    else
        print("[Footsteps] WARNING: Could not find Backyard (Copy) entity!")
    end

    -- Find the frog entity for volume control
    frogEntity = FindEntityByName("Frog")
    if frogEntity then
        print("[Footsteps] Found Frog entity: " .. frogEntity)
    else
        print("[Footsteps] WARNING: Could not find Frog entity!")
    end
end

function M:IsInBackyard()
    local px, py, pz = GetPosition(self.entity)
    
    local inBackyard = px >= backyardMin.x and px <= backyardMax.x and
                       py >= backyardMin.y and py <= backyardMax.y and
                       pz >= backyardMin.z and pz <= backyardMax.z
    
    return inBackyard
end

function M:IsInQuarry()
    local px, py, pz = GetPosition(self.entity)
    return px >= quarryMin.x and px <= quarryMax.x and
           py >= quarryMin.y and py <= quarryMax.y and
           pz >= quarryMin.z and pz <= quarryMax.z
end

function M:on_update(dt)
    local isInBackyard = self:IsInBackyard()

    -- Handle ambient occlusion (volume control based on location)
    -- Must mute BOTH Backyard and Backyard (Copy) entities
    if isInBackyard then
        if backyardEntity then SetEntityVolume(backyardEntity, 1.0) end
        if backyardCopyEntity then SetEntityVolume(backyardCopyEntity, 1.0) end
    else
        if backyardEntity then SetEntityVolume(backyardEntity, 0.0) end
        if backyardCopyEntity then SetEntityVolume(backyardCopyEntity, 0.0) end
    end

    -- Handle frog occlusion (opposite of backyard)
    if frogEntity then
        if isInBackyard then
            -- Muted when in backyard (outdoor)
            SetEntityVolume(frogEntity, 0.0)
        else
            -- Full volume when indoors
            SetEntityVolume(frogEntity, 1.0)
        end
    end

    -- Handle quarry ambient sound
    local isInQuarry = self:IsInQuarry()
    if isInQuarry and not quarryPlaying then
        Sound.Play(quarryEvent)
        quarryPlaying = true
        print("[Footsteps] Entered quarry - playing QuarryAmbient")
    elseif not isInQuarry and quarryPlaying then
        Sound.Stop(quarryEvent)
        quarryPlaying = false
        print("[Footsteps] Left quarry - stopped QuarryAmbient")
    end

    -- Log zone transitions for debugging 
    if isInBackyard and not wasInBackyard then
        print("[Footsteps] Entered backyard - ambient at full volume, frog muted")
    elseif not isInBackyard and wasInBackyard then
        print("[Footsteps] Left backyard - ambient muffled, frog audible")
    end 

    wasInBackyard = isInBackyard
    
    -- Footstep logic 
    -- Check if player is moving
    local isMoving = false
    
    if IsKeyDown(87) or IsKeyDown(83) or IsKeyDown(65) or IsKeyDown(68) then
        isMoving = true
    end
    
    if isMoving then
        stepTimer = stepTimer + dt
        if stepTimer >= stepInterval then
            -- Check zone and play appropriate footstep
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