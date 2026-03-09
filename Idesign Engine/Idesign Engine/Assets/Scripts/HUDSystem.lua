-- HUDSystem.lua
local HUDSystem = {}

-- =========================================================
-- CONSTANTS
-- =========================================================
local PHASE_NIGHT = 2
local MAX_SLOTS   = 2

local THRESHOLD_ORANGE = 0.6
local THRESHOLD_RED    = 0.3

local TEX_GREEN  = "Assets/Textures/patiencebar_green.png"
local TEX_ORANGE = "Assets/Textures/patiencebar_orange.png"
local TEX_RED    = "Assets/Textures/patiencebar_red.png"

local BAR_WIDTH_MAX = 350.0
local BAR_HEIGHT    = 25.0
local BAR_CENTER_Y  = 910.0
local BAR_CENTER_X  = { 180.0, 575.0 }
local BAR_LEFT_EDGE = {
    BAR_CENTER_X[1] - (BAR_WIDTH_MAX / 2),
    BAR_CENTER_X[2] - (BAR_WIDTH_MAX / 2),
}

local HIDE_Z = -9999.0

-- =========================================================
-- STATE
-- =========================================================
local initialized  = false
local textVisible  = {}
local entityShown   = {}
local entityOrigin  = {}

local slotPanels     = {}
local fishIcons      = {}
local orcIcons       = {}
local arrivingTexts  = {}
local orderTexts     = {}
local patienceFrames = {}
local patienceFills  = {}
local potionSlots    = {}
local potionIcons    = {}
local arrowRights    = {}

local barStates = { "green", "green" }

-- =========================================================
-- Z-BASED VISIBILITY HELPERS
-- =========================================================
local function StoreOrigin(entity)
    if not entity then return end
    local ok, x, y, z = pcall(GetPosition, entity)
    if ok then
        entityOrigin[entity] = { x = x, y = y, z = z }
    end
end

local function ShowEntity(entity)
    if not entity then return end
    if entityShown[entity] == true then return end
    local o = entityOrigin[entity]
    if o then
        SetTransformComponent(entity, "position", { x = o.x, y = o.y, z = o.z })
    end
    entityShown[entity] = true
end

local function HideEntity(entity)
    if not entity then return end
    if entityShown[entity] == false then return end
    local o = entityOrigin[entity]
    if o then
        SetTransformComponent(entity, "position", { x = o.x, y = o.y, z = HIDE_Z })
    end
    entityShown[entity] = false
end

-- =========================================================
-- TEXT HELPERS
-- =========================================================
local function ShowText(entity, text)
    if not entity then return end
    local t = text or ""
    if textVisible[entity] == t then return end
    SetTextComponent(entity, "text", t)
    SetTextComponent(entity, "opacity", 1.0)
    textVisible[entity] = t
end

local function HideText(entity)
    if not entity then return end
    if textVisible[entity] == "" then return end
    SetTextComponent(entity, "opacity", 0.0)
    SetTextComponent(entity, "text", "")
    textVisible[entity] = ""
end

-- =========================================================
-- NIGHT INIT
-- =========================================================
function HUDSystem:NightInit()
    -- Fix 2: Reset all state tables before populating
    entityShown   = {}
    entityOrigin  = {}
    textVisible   = {}

    for i = 1, MAX_SLOTS do
        slotPanels[i]     = FindEntityByName("SlotPanel_"    .. i)
        fishIcons[i]      = FindEntityByName("Icon_Fish_"    .. i)
        orcIcons[i]       = FindEntityByName("Icon_Orc_"     .. i)
        arrivingTexts[i]  = FindEntityByName("ArrivingText_" .. i)
        orderTexts[i]     = FindEntityByName("OrderText_"    .. i)
        patienceFrames[i] = FindEntityByName("PatienceFrame_".. i)
        patienceFills[i]  = FindEntityByName("PatienceFill_" .. i)
        potionSlots[i]    = FindEntityByName("PotionSlot_"   .. i)
        potionIcons[i]    = FindEntityByName("PotionIcon_"   .. i)
        arrowRights[i]    = FindEntityByName("arrowRight_"   .. i)

        -- print("[HUDSystem] Slot " .. i .. " refs:")
        -- print("  SlotPanel:     " .. tostring(slotPanels[i]))
        -- print("  Icon_Fish:     " .. tostring(fishIcons[i]))
        -- print("  Icon_Orc:      " .. tostring(orcIcons[i]))
        -- print("  ArrivingText:  " .. tostring(arrivingTexts[i]))
        -- print("  OrderText:     " .. tostring(orderTexts[i]))
        -- print("  PatienceFrame: " .. tostring(patienceFrames[i]))
        -- print("  PatienceFill:  " .. tostring(patienceFills[i]))
        -- print("  PotionSlot:    " .. tostring(potionSlots[i]))
        -- print("  PotionIcon:    " .. tostring(potionIcons[i]))
        -- print("  ArrowRight:    " .. tostring(arrowRights[i]))

        -- Fix 3: Store original positions BEFORE hiding
        StoreOrigin(slotPanels[i])
        StoreOrigin(fishIcons[i])
        StoreOrigin(orcIcons[i])
        StoreOrigin(patienceFrames[i])
        StoreOrigin(patienceFills[i])
        StoreOrigin(potionSlots[i])
        StoreOrigin(potionIcons[i])
        StoreOrigin(arrowRights[i])
    end

    -- Hide all slots at night start
    for i = 1, MAX_SLOTS do
        HUDSystem:HideSlot(i)
    end

    barStates = { "green", "green" }

    initialized = true
    print("[HUDSystem] NightInit complete.")
end

-- =========================================================
-- ON INIT
-- =========================================================
function HUDSystem:on_init()
    print("[HUDSystem] Script loaded.")
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function HUDSystem:on_update(dt)
    local phase = GetPhase()

    if phase ~= PHASE_NIGHT then
        if initialized then
            for i = 1, MAX_SLOTS do HUDSystem:HideSlot(i) end
            textVisible   = {}
            entityShown   = {}
            initialized   = false
        end
        return
    end

    if not initialized then
        HUDSystem:NightInit()
    end

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
end

-- =========================================================
-- SHOW SLOT
-- =========================================================
function HUDSystem:ShowSlot(slot, data)
    ShowEntity(slotPanels[slot])
    ShowEntity(arrowRights[slot])
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
    HideEntity(slotPanels[slot])
    HideEntity(arrowRights[slot])
    HideEntity(fishIcons[slot])
    HideEntity(orcIcons[slot])
    HideEntity(patienceFrames[slot])
    HideEntity(patienceFills[slot])
    HideEntity(potionSlots[slot])
    HideEntity(potionIcons[slot])
    HideText(arrivingTexts[slot])
    HideText(orderTexts[slot])
    barStates[slot] = "green"
end

-- =========================================================
-- SET ICON
-- =========================================================
function HUDSystem:SetIcon(slot, icon_type)
    if icon_type == 1 then
        ShowEntity(fishIcons[slot])
        HideEntity(orcIcons[slot])
    else
        ShowEntity(orcIcons[slot])
        HideEntity(fishIcons[slot])
    end
end

-- =========================================================
-- SET ARRIVING MODE
-- =========================================================
function HUDSystem:SetArrivingMode(slot, is_arriving)
    -- print("[HUDSystem] SetArrivingMode slot=" .. slot .. " arriving=" .. tostring(is_arriving) .. " frame=" .. tostring(patienceFills[slot]))
    if is_arriving then
        ShowText(arrivingTexts[slot], "Arriving...")
        HideText(orderTexts[slot])
        HideEntity(patienceFrames[slot])
        HideEntity(patienceFills[slot])
        HideEntity(potionSlots[slot])
        HideEntity(potionIcons[slot])
    else
        HideText(arrivingTexts[slot])
        ShowEntity(patienceFrames[slot])
        ShowEntity(potionSlots[slot])
    end
end

-- =========================================================
-- SET ORDER TEXT
-- =========================================================
function HUDSystem:SetOrderText(slot, order)
    if not orderTexts[slot] then return end
    ShowText(orderTexts[slot], order)
end

-- =========================================================
-- SET POTION ICON

-- =========================================================
function HUDSystem:SetPotionIcon(slot, order)
    if order and potionIcons[slot] then
        local texPath = "Assets/Textures/" .. order .. ".png"
        SetTexture(potionIcons[slot], texPath)
        ShowEntity(potionIcons[slot])
    end
end

-- =========================================================
-- SET PATIENCE BAR
-- =========================================================
function HUDSystem:SetPatienceBar(slot, pct)
    pct = math.max(0.0, math.min(1.0, pct))
    -- print("[HUDSystem] SetPatienceBar slot=" .. slot .. " pct=" .. pct .. " fill=" .. tostring(patienceFills[slot]) .. " shown=" .. tostring(entityShown[patienceFills[slot]]))

    if patienceFills[slot] then
        -- Make sure fill is visible first (restores Z from HIDE_Z if needed)
        ShowEntity(patienceFills[slot])

        local currentWidth = pct * BAR_WIDTH_MAX
        local centerX      = BAR_LEFT_EDGE[slot] + (currentWidth / 2)
        SetScale(patienceFills[slot], currentWidth, BAR_HEIGHT, 1.0)
        SetTransformComponent(patienceFills[slot], "position", { x = centerX, y = BAR_CENTER_Y, z = 0 })

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
            elseif targetState == "orange" then
                SetTexture(patienceFills[slot], TEX_ORANGE)
            else
                SetTexture(patienceFills[slot], TEX_GREEN)
            end
            barStates[slot] = targetState
            -- print("[HUDSystem] Slot " .. slot .. " bar -> " .. targetState)
        end
    end
end

-- =========================================================
-- FREE PLAY MODE (stub)
-- =========================================================
-- local freePlayPanel     = nil
-- local freePlayTimerText = nil
-- function HUDSystem:ShowFreePlayTimer(timeLeft)
--     if freePlayTimerText then ShowText(freePlayTimerText, string.format("%.0f", timeLeft)) end
-- end
-- function HUDSystem:HideFreePlayTimer()
--     if freePlayTimerText then HideText(freePlayTimerText) end
-- end

return HUDSystem