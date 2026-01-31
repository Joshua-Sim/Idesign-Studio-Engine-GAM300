
local M = {}
local isMuted = false

-- Paths to mute and unmute icon
local icon_unmuted = "Assets/Textures/Unmute_Button.png"
local icon_muted   = "Assets/Textures/Mute_Button.png" 

function M:on_init()
    -- Set the initial icon state to match isMuted (unmuted on start)
    SetTexture(self.entity, icon_unmuted)

    RegisterButtonCallback(self.entity, function(ent)
        isMuted = not isMuted
        Sound.Mute("bgm_mute", isMuted)

        -- Swap the icon based on new state
        if isMuted then
            SetTexture(ent, icon_muted)
        else
            SetTexture(ent, icon_unmuted)
        end
    end)
end

return M
