local M = {}

M.spawnedTextID = -1 -- Store ID to delete it later

function M:on_init()
    -- Nothing needed here yet
end

function M:on_update(dt)
    -- Nothing needed here yet
end

-- This is the function name you type in the Editor inside "Lua Function"
function M:OnInteract(otherEntityID)
    
    print("Lua: Interaction Triggered by Entity " .. tostring(otherEntityID))

    -- Only spawn if we haven't already
    if M.spawnedTextID == -1 then
        
        -- CALL C++ FUNCTION
        -- Arg 1: self.entity (The Box's ID)
        -- Arg 2: The String
        -- Arg 3: Height Offset
        M.spawnedTextID = SpawnPrompt(self.entity, "Press E to Interact", 2.5)
        
        print("Lua: Spawned Prompt ID: " .. M.spawnedTextID)
    else
        print("Lua: Prompt already exists.")
    end
end

return M