
local M = {}
local isMuted = false

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        isMuted = not isMuted
        Sound.Mute("bgm_mute", isMuted)
    end)
end

return M
