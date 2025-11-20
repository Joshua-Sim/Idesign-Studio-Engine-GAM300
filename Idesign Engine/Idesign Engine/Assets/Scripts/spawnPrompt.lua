local M = {}

-- We store the text ID inside the table so we remember it
M.spawnedTextID = -1 

-- action: 1 = Enter, 0 = Exit
function M:OnInteract(targetID, action)
    
    -- 1. Security Check (Am I the one being hit?)
    if self.entity ~= targetID then return end

    -- 2. ENTER Logic
    if action == 1 then
        print("Player Entered Box! Spawning Text.")
        
        -- Only spawn if we don't have one already
        if M.spawnedTextID == -1 then
            M.spawnedTextID = SpawnPrompt(self.entity, "Press E", 2.5)
        end

    -- 3. EXIT Logic
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