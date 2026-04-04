
local M = {}

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        GameStateLose()
    end)
end

return M