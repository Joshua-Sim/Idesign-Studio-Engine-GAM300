local M = {}

function M:on_init()
    print("[MenuScript] on_init called for entity:", self.entity)

    RegisterButtonCallback(self.entity, function(ent)
        print("[MenuScript] Button clicked! Entity:", ent)

        -- 1. Start the game state
        GameStateStart()

        -- 2. Spawn Herbs if missing
        local herbs = {"Herb1", "Herb2", "Herb3", "Herb4"}

        for _, name in ipairs(herbs) do
            -- Try to find the entity
            local id = FindEntityByName(name)
            
            -- Check if ID is nil (not found) or invalid (destroyed)
            if not id or not IsEntityValid(id) then
                print("[MenuScript] Spawning missing herb: " .. name)
                SpawnPrefab(name .. "_Prefab")
            else
                print("[MenuScript] " .. name .. " already exists.")
            end
        end
    end)
end

return M