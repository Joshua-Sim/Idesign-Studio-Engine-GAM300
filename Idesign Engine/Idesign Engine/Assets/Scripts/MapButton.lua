-- MapButton.lua
-- Attach this to each map button: Forest Button, Quarry Button, House Button
-- Each button auto-identifies itself and teleports the player on click

local M = {}

-- Spawn data for each location
local locations = {
    ["Forest Button"] = {
        name = "Forest",
        checkUnlocked = IsForestUnlocked,  -- Function reference
        spawnPos = { x = -110.493, y = 7.644,  z = 124.351 },
        spawnRot = { x = 0.0,    y = 90.0,  z = 0.0 }
    },
    ["Quarry Button"] = {
        name = "Quarry",
        checkUnlocked = IsQuarryUnlocked,  -- Function reference
        spawnPos = { x = -170.856,  y = 7.730,  z = -170.782 },
        spawnRot = { x = 0.0,    y = 0.0,   z = 0.0 }
    },
    ["House Button"] = {
        name = "Home",
        checkUnlocked = function() return true end,  -- Home is always unlocked
        spawnPos = { x = 0.0,    y = 0.0,   z = 0.0 },
        spawnRot = { x = 0.0,    y = 0.0,   z = 0.0 }
    }
}

-- Resolved at init: which location this button maps to
local myLocation = nil
local myName    = nil

function M:on_init()
    -- Figure out which button we are by comparing entity IDs
    for name, _ in pairs(locations) do
        local id = FindEntityByName(name)
        if id == self.entity then
            myName     = name
            myLocation = locations[name]
            break
        end
    end

    if not myLocation then
        print("[MapButton] ERROR: Could not identify button for entity " .. tostring(self.entity))
        return
    end

    print("[MapButton] " .. myName .. " initialized (entity: " .. self.entity .. ")")

    -- Register the click handler
    RegisterButtonCallback(self.entity, function(ent)
        print("[MapButton] " .. myName .. " clicked!")

        -- Check if this location is unlocked
        if not myLocation.checkUnlocked() then
            print("[MapButton] " .. myLocation.name .. " is LOCKED! Cannot teleport.")
            -- Optional: Show a "Location Locked" message to the player
            -- SpawnPrefab("LocationLockedMessage")
            return
        end

        print("[MapButton] " .. myLocation.name .. " is unlocked. Teleporting...")

        -- Pop the MapSelection state so the map UI hides
        PopEngineState()
        SetUIFocused(false)

        -- Teleport the player to this location
        TeleportPlayer(
            myLocation.spawnPos.x, myLocation.spawnPos.y, myLocation.spawnPos.z,
            myLocation.spawnRot.x, myLocation.spawnRot.y, myLocation.spawnRot.z
        )

        print("[MapButton] Player teleported to " .. myName)
    end)
end

return M