
local M = {}

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        GameStateWin()
    end)
end

return M