
local M = {}

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        GameStateSettings()
    end)
end

return M