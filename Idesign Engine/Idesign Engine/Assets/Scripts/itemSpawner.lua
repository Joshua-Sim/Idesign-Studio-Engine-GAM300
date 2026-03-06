local M = {}

function M:RespawnHerbs()
    local herbs = {"Herb1", "Herb2", "Herb3", "Herb4", "ForestHerb", "ForestHerb2", "ForestHerb3", "ForestHerb4", "ForestHerb5", "ForestHerb6", "ForestHerb7", "Pear1", "Pear2", "Pear3", "Pear4", "Pear5", "Pear6", "Pear7", "Pickable-rock 1", "pickableIron1"}
    for _, name in ipairs(herbs) do
        local id = FindEntityByName(name)
        if not id or not IsEntityValid(id) then
            SpawnPrefab(name .. "_Prefab")
        end
    end
end

function M:on_init()
    -- Subscribe this entity to a global message
    -- This uses the binding we discussed earlier
    SubscribeToEvent("RequestHerbSpawn", "RespawnHerbs", self.entity)
end

return M