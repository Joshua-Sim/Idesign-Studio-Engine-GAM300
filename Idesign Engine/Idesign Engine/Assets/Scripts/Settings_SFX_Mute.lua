
local M = {}
local isMuted = false

-- Paths to mute and unmute icon
local icon_unmuted = "Assets/Textures/Unmute_Button.png"
local icon_muted   = "Assets/Textures/Mute_Button.png" 

-- Entity references (will be found at init)
local knobEntity = nil
local barEntity = nil
local textEntity = nil

-- Saved state
local savedVolume = 1.0
local savedKnobX = 0.0

function M:on_init()

    -- Set the initial icon state to match isMuted (unmuted on start)
    SetTexture(self.entity, icon_unmuted)

    -- Find the slider entities by tag
    local knobs = GetEntitiesByTag("sliderKnob_sfx")
    if knobs and #knobs > 0 then
        knobEntity = knobs[1]
    else
        print("[BGM Mute] Warning: Could not find sliderKnob_sfx")
    end
    
    local bars = GetEntitiesByTag("sliderBar_sfx")
    if bars and #bars > 0 then
        barEntity = bars[1]
    else
        print("[BGM Mute] Warning: Could not find sliderBar_sfx")
    end
    
    local texts = GetEntitiesByTag("volumeText_sfx")
    if texts and #texts > 0 then
        textEntity = texts[1]
    else
        print("[BGM Mute] Warning: Could not find volumeText_sfx")
    end

    RegisterButtonCallback(self.entity, function(ent)
        isMuted = not isMuted
        Sound.Mute("sfx_mute", isMuted)

        -- Swap the icon based on new state
        if isMuted then
            -- MUTE: Move to 0%
            SetTexture(ent, icon_muted)

            if knobEntity and barEntity then
                -- Save current position before muting
                local kx, ky, kz = GetPosition(knobEntity)
                savedKnobX = kx
                
                -- Calculate bar bounds using GetScale
                local bx, by, bz = GetPosition(barEntity)
                local barWidth, barHeight, barDepth = GetScale(barEntity)
                local barLeft = bx - 0.5 * barWidth
                
                -- Move knob to left edge (0%)
                SetPosition(knobEntity, barLeft, ky, kz)
            end
            
            if textEntity then
                SetText(textEntity, "0%")
            end
        else
            -- UNMUTE: Restore to saved position
            SetTexture(ent, icon_unmuted)

            if knobEntity then
                local kx, ky, kz = GetPosition(knobEntity)
                SetPosition(knobEntity, savedKnobX, ky, kz)
            end
            
            if textEntity then
                -- Recalculate percentage from saved position
                if barEntity then
                    local bx, by, bz = GetPosition(barEntity)
                    local barWidth, barHeight, barDepth = GetScale(barEntity)
                    local barLeft = bx - 0.5 * barWidth
                    local barRight = bx + 0.5 * barWidth
                    
                    savedVolume = (savedKnobX - barLeft) / (barRight - barLeft)
                    savedVolume = math.max(0.0, math.min(1.0, savedVolume))
                end
                
                local percent = math.floor(savedVolume * 100)
                SetText(textEntity, percent .. "%")
            end
        end
        
    end)
end

return M
