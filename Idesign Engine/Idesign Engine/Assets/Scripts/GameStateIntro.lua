
local M = {}

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        GameStateIntro()
    end)
end

return M