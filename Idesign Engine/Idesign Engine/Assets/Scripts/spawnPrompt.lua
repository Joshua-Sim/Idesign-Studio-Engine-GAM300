local M = {}

M.spawnedTextID = -1 

function M.OnInteract(myEntityID, targetID, action)
    
    if myEntityID ~= targetID then return end

    -- 2. ENTER Logic (Action 1)
    if action == 1 then
        print("Player Entered Box! Spawning Text.")
        
        if M.spawnedTextID == -1 then
            
            -- [CHANGE 1] Get the name of the entity
            -- NOTE: You must have 'GetEntityName' exposed from C++ to Lua
            local name = GetEntityName(myEntityID)
            
            -- [CHANGE 2] Concatenate the string
            local promptText = "E to pickup: " .. name

            M.spawnedTextID = SpawnPrompt(myEntityID, promptText)
        end

    -- 3. EXIT Logic (Action 0)
    elseif action == 0 then
        print("Player Exited Box! Destroying Text.")

        if M.spawnedTextID ~= -1 then
            DestroyEntity(M.spawnedTextID)
            M.spawnedTextID = -1 
        end
    end
end

return M