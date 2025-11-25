local M = {}

-- We store the text ID inside the table so we remember it
M.spawnedTextID = -1 

-- CHANGED: Use DOT (.) syntax.
-- C++ Arguments: (ListenerEntityID, TargetEntityID, ActionType)
function M.OnInteract(myEntityID, targetID, action)
    
    -- 1. Security Check 
    -- (Physics event broadcasts globally, ensure the event was meant for ME)
    -- 'myEntityID' is the entity holding this script.
    -- 'targetID' is the entity that was actually hit.
    if myEntityID ~= targetID then return end

    -- 2. ENTER Logic (Action 1)
    if action == 1 then
        print("Player Entered Box! Spawning Text.")
        
        -- Only spawn if we don't have one already
        if M.spawnedTextID == -1 then
            -- CHANGED: Use 'myEntityID' directly instead of 'self.entity'
            M.spawnedTextID = SpawnPrompt(myEntityID, "Press E", 2.5)
        end

    -- 3. EXIT Logic (Action 0)
    elseif action == 0 then
        print("Player Exited Box! Destroying Text.")

        -- Only destroy if we actually have one
        if M.spawnedTextID ~= -1 then
            DestroyEntity(M.spawnedTextID)
            M.spawnedTextID = -1 -- Reset ID so we can spawn again later
        end
    end
end

return M