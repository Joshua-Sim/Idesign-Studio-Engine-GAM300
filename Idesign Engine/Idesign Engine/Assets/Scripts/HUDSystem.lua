-- HUDSystem.lua
-- Screen-space HUD for the 2-slot customer counter panel.
-- Reads data from GetCustomerUIData() exposed by CustomerSystem.lua
--
-- Expected scene entities (pre-spawned, screen-space):
--
--   SlotPanel_1, SlotPanel_2                  -- parent panel per slot
--   Icon_Fish_1, Icon_Fish_2                  -- customer avatar (type 1)
--   Icon_Orc_1,  Icon_Orc_2                   -- customer avatar (type 2)
--   OrderText_1, OrderText_2                  -- potion name text
--   PatienceFrame_1, PatienceFrame_2
--   PatienceFill_1,  PatienceFill_2
--   ArrivingText_1, ArrivingText_2            -- "Arriving..." label
--   FreePlayPanel                             -- shown during free play (optional)
--   FreePlayTimerText                         -- countdown text (optional)

local HUDSystem = {}

local activeUISet   = {}  -- tracks which entities had SetIsUI(true) called
local textEntitySet = {}  -- guards text entities from SetIsUI calls

-- Safe wrappers - only call SetIsUI on image entities, never text entities
-- Text entities are hidden/shown via SetTextComponent opacity instead
local function ShowUI(entity)
    if not entity then return end
    if textEntitySet[entity] then return end  -- never SetIsUI on text
    SetIsUI(entity, true)
    activeUISet[entity] = true
end
local function HideUI(entity)
    if not entity then return end
    if textEntitySet[entity] then return end  -- never SetIsUI on text
    if activeUISet[entity] then
        SetIsUI(entity, false)
        activeUISet[entity] = nil
    end
end

-- =========================================================
-- CONFIGURATION
-- =========================================================
local POTION_DISPLAY_NAMES = {
    ["strength_potion"] = "Strength Potion",
    ["vitality_potion"] = "Vitality Potion",
    ["warmth_potion"]   = "Warmth Potion",
    ["Warmth_Potion"]   = "Warmth Potion",
}

-- TODO: Replace placeholder paths with your actual potion icon textures
local POTION_ICONS = {
    ["strength_potion"] = "Assets/Textures/placeholder_strength.png",
    ["vitality_potion"] = "Assets/Textures/placeholder_vitality.png",
    ["warmth_potion"]   = "Assets/Textures/placeholder_warmth.png",
    ["Warmth_Potion"]   = "Assets/Textures/placeholder_warmth.png",
}
local POTION_ICON_DEFAULT = "Assets/Textures/placeholder_potion.png"

local BAR_LENGTH_MAX   = 0.5
local BAR_THICKNESS    = 0.2
local TEX_GREEN        = "Assets/Textures/patiencebar_green.png"
local TEX_ORANGE       = "Assets/Textures/patiencebar_orange.png"
local TEX_RED          = "Assets/Textures/patiencebar_red.png"
local THRESHOLD_ORANGE = 0.6   -- swap to orange below 60%
local THRESHOLD_RED    = 0.3   -- swap to red below 30%

-- =========================================================
-- INTERNAL STATE
-- =========================================================
local MAX_SLOTS = 2

local slotPanels     = { nil, nil }
local fishIcons      = { nil, nil }
local orcIcons       = { nil, nil }
local orderTexts     = { nil, nil }
local arrivingTexts  = { nil, nil }
local patienceFrames = { nil, nil }
local patienceFills  = { nil, nil }
local potionIcons    = { nil, nil }
local potionSlots    = { nil, nil }
local barStates      = { "green", "green" }   -- "green", "orange", or "red"

-- Free play UI
local freePlayPanel     = nil
local freePlayTimerText = nil

local PHASE_NIGHT   = 2
local initialized   = false  -- deferred until first night phase

-- =========================================================
-- ON INIT
-- =========================================================
function HUDSystem:on_init()
    -- Do nothing on scene load. Entity refs and hide logic are
    -- deferred to first night phase to avoid deactivating scene objects.
    print("[HUDSystem] Script loaded. Waiting for night phase...")
end

-- =========================================================
-- DEFERRED INIT (called once on first night phase)
-- =========================================================
function HUDSystem:NightInit()
    print("[HUDSystem] Night phase detected. Initializing UI references...")

    for i = 1, MAX_SLOTS do
        local s = tostring(i)
        slotPanels[i]     = FindEntityByName("SlotPanel_"     .. s)
        fishIcons[i]      = FindEntityByName("Icon_Fish_"     .. s)
        orcIcons[i]       = FindEntityByName("Icon_Orc_"      .. s)
        orderTexts[i]     = FindEntityByName("OrderText_"     .. s)
        arrivingTexts[i]  = FindEntityByName("ArrivingText_"  .. s)
        patienceFrames[i] = FindEntityByName("PatienceFrame_" .. s)
        patienceFills[i]  = FindEntityByName("PatienceFill_"  .. s)
        potionIcons[i]    = FindEntityByName("PotionIcon_"    .. s)
        potionSlots[i]    = FindEntityByName("PotionSlot_"    .. s)

        if not slotPanels[i]     then print("[HUDSystem] WARNING: SlotPanel_"     .. s .. " not found!") end
        if not fishIcons[i]      then print("[HUDSystem] WARNING: Icon_Fish_"     .. s .. " not found!") end
        if not orcIcons[i]       then print("[HUDSystem] WARNING: Icon_Orc_"      .. s .. " not found!") end
        if not orderTexts[i]     then print("[HUDSystem] WARNING: OrderText_"     .. s .. " not found!") end
        if not arrivingTexts[i]  then print("[HUDSystem] WARNING: ArrivingText_"  .. s .. " not found!") end
        if not patienceFrames[i] then print("[HUDSystem] WARNING: PatienceFrame_" .. s .. " not found!") end
        if not patienceFills[i]  then print("[HUDSystem] WARNING: PatienceFill_"  .. s .. " not found!") end
        if not potionIcons[i]    then print("[HUDSystem] WARNING: PotionIcon_"    .. s .. " not found!") end
        if not potionSlots[i]    then print("[HUDSystem] WARNING: PotionSlot_"    .. s .. " not found!") end
    end

    freePlayPanel     = FindEntityByName("FreePlayPanel")
    freePlayTimerText = FindEntityByName("FreePlayTimerText")

    -- Register all text entities so ShowUI/HideUI never accidentally calls SetIsUI on them
    textEntitySet = {}
    for i = 1, MAX_SLOTS do
        if orderTexts[i]    then textEntitySet[orderTexts[i]]    = true end
        if arrivingTexts[i] then textEntitySet[arrivingTexts[i]] = true end
    end
    if freePlayTimerText then textEntitySet[freePlayTimerText] = true end

    -- Entities already start as isUI=false in the editor, no need to hide on init.
    -- HideSlot is only called when leaving night phase or when a slot clears.

    initialized = true
    print("[HUDSystem] UI initialized for night phase.")
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function HUDSystem:on_update(dt)
    -- Only run during night phase
    local phase = GetPhase()
    if phase ~= PHASE_NIGHT then
        -- If we were initialized and just left night phase, hide everything
        if initialized then
            for i = 1, MAX_SLOTS do HUDSystem:HideSlot(i) end
            HUDSystem:HideFreePlayTimer()
            initialized = false
        end
        return
    end

    -- First tick of night phase - initialize UI references
    if not initialized then
        HUDSystem:NightInit()
    end

    -- Customer slots
    local uiData = GetCustomerUIData()
    if uiData then
        for slot = 1, MAX_SLOTS do
            local data = uiData[slot]
            if data then
                HUDSystem:ShowSlot(slot, data)
            else
                HUDSystem:HideSlot(slot)
            end
        end
    end

    -- Free play timer
    -- GetFreePlayTimeLeft() is exposed by CustomerSystem; returns nil when not in free play
    if GetFreePlayTimeLeft then
        local timeLeft = GetFreePlayTimeLeft()
        if timeLeft then
            HUDSystem:ShowFreePlayTimer(timeLeft)
        else
            HUDSystem:HideFreePlayTimer()
        end
    end
end

-- =========================================================
-- SHOW SLOT
-- =========================================================
function HUDSystem:ShowSlot(slot, data)
    if slotPanels[slot] then
        ShowUI(slotPanels[slot])
    end

    HUDSystem:SetIcon(slot, data.icon_type)

    if data.is_arriving then
        HUDSystem:SetArrivingMode(slot, true)
    else
        HUDSystem:SetArrivingMode(slot, false)
        HUDSystem:SetOrderText(slot, data.order)
        HUDSystem:SetPotionIcon(slot, data.order)
        HUDSystem:SetPatienceBar(slot, data.patience_pct)
    end
end

-- =========================================================
-- HIDE SLOT
-- =========================================================
function HUDSystem:HideSlot(slot)
    if slotPanels[slot]     then HideUI(slotPanels[slot]) end
    if fishIcons[slot]      then HideUI(fishIcons[slot]) end
    if orcIcons[slot]       then HideUI(orcIcons[slot]) end
    if orderTexts[slot]     then SetTextComponent(orderTexts[slot], "opacity", 0.0) end
    if arrivingTexts[slot]  then SetTextComponent(arrivingTexts[slot], "opacity", 0.0) end
    if patienceFrames[slot] then HideUI(patienceFrames[slot]) end
    if patienceFills[slot]  then HideUI(patienceFills[slot]) end
    if potionIcons[slot]    then HideUI(potionIcons[slot]) end
    if potionSlots[slot]    then HideUI(potionSlots[slot]) end
    barStates[slot] = "green"
end

-- =========================================================
-- SET ICON
-- =========================================================
function HUDSystem:SetIcon(slot, icon_type)
    if fishIcons[slot] then HideUI(fishIcons[slot]) end
    if orcIcons[slot]  then HideUI(orcIcons[slot]) end

    if icon_type == 1 then
        if fishIcons[slot] then ShowUI(fishIcons[slot]) end
    elseif icon_type == 2 then
        if orcIcons[slot]  then ShowUI(orcIcons[slot]) end
    else
        print("[HUDSystem] WARNING: Unknown icon_type: " .. tostring(icon_type) .. " for slot " .. slot)
    end
end

-- =========================================================
-- SET ORDER TEXT
-- =========================================================
function HUDSystem:SetOrderText(slot, order)
    if not orderTexts[slot] then return end
    local displayName = POTION_DISPLAY_NAMES[order] or order
    SetTextComponent(orderTexts[slot], "text", displayName)
    SetTextComponent(orderTexts[slot], "opacity", 1.0)
end

-- =========================================================
-- SET POTION ICON
-- =========================================================
function HUDSystem:SetPotionIcon(slot, order)
    -- TODO: uncomment SetTexture once real potion icon assets exist
    -- local tex = POTION_ICONS[order] or POTION_ICON_DEFAULT
    -- if potionIcons[slot] then SetTexture(potionIcons[slot], tex) end
    if potionIcons[slot] then ShowUI(potionIcons[slot]) end
    if potionSlots[slot] then ShowUI(potionSlots[slot]) end
end

-- =========================================================
-- SET ARRIVING MODE
-- =========================================================
function HUDSystem:SetArrivingMode(slot, is_arriving)
    if arrivingTexts[slot] then
        SetTextComponent(arrivingTexts[slot], "opacity", is_arriving and 1.0 or 0.0)
        if is_arriving then
            SetTextComponent(arrivingTexts[slot], "text", "Arriving...")
        end
    end

    if orderTexts[slot]     then SetTextComponent(orderTexts[slot], "opacity", is_arriving and 0.0 or 1.0) end
    if potionIcons[slot]    then if is_arriving then HideUI(potionIcons[slot]) else ShowUI(potionIcons[slot]) end end
    if potionSlots[slot]    then if is_arriving then HideUI(potionSlots[slot]) else ShowUI(potionSlots[slot]) end end
    if patienceFrames[slot] then if is_arriving then HideUI(patienceFrames[slot]) else ShowUI(patienceFrames[slot]) end end
    if patienceFills[slot]  then if is_arriving then HideUI(patienceFills[slot]) else ShowUI(patienceFills[slot]) end end
end

-- =========================================================
-- SET PATIENCE BAR
-- =========================================================
function HUDSystem:SetPatienceBar(slot, patience_pct)
    local pct = math.max(0.0, math.min(1.0, patience_pct))

    if patienceFills[slot] then
        -- TODO: re-enable once bar scaling is confirmed safe
        -- local current_length = pct * BAR_LENGTH_MAX
        -- SetScale(patienceFills[slot], current_length, BAR_THICKNESS * 0.9, BAR_THICKNESS * 0.9)
        ShowUI(patienceFills[slot])
    end

    if patienceFrames[slot] then
        ShowUI(patienceFrames[slot])
    end

    if patienceFills[slot] then
        local targetState
        if pct <= THRESHOLD_RED then
            targetState = "red"
        elseif pct <= THRESHOLD_ORANGE then
            targetState = "orange"
        else
            targetState = "green"
        end

        if barStates[slot] ~= targetState then
            if targetState == "red" then
                SetTexture(patienceFills[slot], TEX_RED)
                print("[HUDSystem] Slot " .. slot .. " patience critical - bar turned red!")
            elseif targetState == "orange" then
                SetTexture(patienceFills[slot], TEX_ORANGE)
                print("[HUDSystem] Slot " .. slot .. " patience low - bar turned orange!")
            else
                SetTexture(patienceFills[slot], TEX_GREEN)
            end
            barStates[slot] = targetState
        end
    end
end

-- =========================================================
-- FREE PLAY TIMER
-- =========================================================
function HUDSystem:ShowFreePlayTimer(timeLeft)
    if freePlayPanel     then ShowUI(freePlayPanel)  end
    if freePlayTimerText then
        SetTextComponent(freePlayTimerText, "opacity", 1.0)
        local mins = math.floor(timeLeft / 60)
        local secs = math.floor(timeLeft % 60)
        SetTextComponent(freePlayTimerText, "text",
            string.format("Free Play: %d:%02d", mins, secs))
    end
end

function HUDSystem:HideFreePlayTimer()
    if freePlayPanel     then HideUI(freePlayPanel) end
    if freePlayTimerText then SetTextComponent(freePlayTimerText, "opacity", 0.0) end
end

return HUDSystem