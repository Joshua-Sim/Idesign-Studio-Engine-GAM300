-- MapSelectionUI.lua
--
-- DIALOG VISIBILITY: SetIsUI called directly on the 6 cached dialog entity IDs.
-- No tag dependency for dialog � we own visibility entirely from Lua.
-- Forest/Quarry/House map buttons still rely on SceneInterface via "map" tag.
--
-- HOVER SWAP: Z trick � hover entity pushed to HIDE_Z until mouse is over button.

local M = {}

local HIDE_Z = -99999.0  -- offscreen Z to hide an entity from rendering

-- =========================================================
-- LOCATION DATA
-- =========================================================
local locations = {
    Forest = {
        name = "Forest",
        buttonEntity = "Forest Button",
        checkUnlocked = IsForestUnlocked,
        spawnPos = { x = -276.0, y = 319.0, z = 620.0 },
        spawnRot = { x = 0.0,   y = 0.0,   z = 0.0 },
        ingredients = { "Forest Herbs", "Mushrooms", "Berries" },
        ingredientImageEntity = "ForestIngredientsImage",
        dialogTextures = {
            complete      = "Assets/Textures/forest_complete.png",
            button_normal = "Assets/Textures/forest_btn_travel_normal.png",
            button_hover  = "Assets/Textures/forest_btn_travel_hover.png",
            stay_normal   = "Assets/Textures/forest_btn_stay_normal.png",
            stay_hover    = "Assets/Textures/forest_btn_stay_hover.png",
        }
    },
    Quarry = {
        name = "Quarry",
        buttonEntity = "Quarry Button",
        checkUnlocked = IsQuarryUnlocked,
        spawnPos = { x = -170.856, y = 370.0, z = -170.782 },
        spawnRot = { x = 0.0,     y = 0.0,   z = 0.0 },
        ingredients = { "Stone Minerals", "Cave Moss", "Crystal Dust" },
        ingredientImageEntity = "QuarryIngredientsImage",
        dialogTextures = {
            complete      = "Assets/Textures/quarry_complete.png",
            button_normal = "Assets/Textures/quarry_btn_travel_normal.png",
            button_hover  = "Assets/Textures/quarry_btn_travel_hover.png",
            stay_normal   = "Assets/Textures/quarry_btn_stay_normal.png",
            stay_hover    = "Assets/Textures/quarry_btn_stay_hover.png",
        }
    },
    House = {
        name = "House",
        buttonEntity = "House Button",
        checkUnlocked = function() return true end,
        spawnPos = { x = 0.0, y = 0.0, z = 0.0 },
        spawnRot = { x = 0.0, y = 0.0, z = 0.0 },
        ingredients = { "Garden Herbs", "Kitchen Spices" },
        ingredientImageEntity = "HouseIngredientsImage",
        dialogTextures = nil
    }
}

-- =========================================================
-- DIALOG ENTITY NAMES
-- =========================================================
-- 6 entities. Do NOT need "map" tag � visibility fully managed here via SetIsUI.
-- dialog_btn_travel_normal / hover / stay_normal / stay_hover need ButtonComponent.

local DIALOG_OVERLAY           = "dialog_overlay"
local DIALOG_COMPLETE          = "dialog_complete"
local DIALOG_BTN_TRAVEL_NORMAL = "dialog_btn_travel_normal"
local DIALOG_BTN_TRAVEL_HOVER  = "dialog_btn_travel_hover"
local DIALOG_BTN_STAY_NORMAL   = "dialog_btn_stay_normal"
local DIALOG_BTN_STAY_HOVER    = "dialog_btn_stay_hover"

-- Cached entity IDs � populated once in on_init
local de = {}  -- dialogEntities shorthand

-- Cached original Z for all 4 button entities
local origZ = {}

local pendingLocation = nil
local travelHovered   = false
local stayHovered     = false

-- =========================================================
-- SETISUI HELPERS � only called on known dialog entity IDs
-- =========================================================
local function ShowDialog(e)
    if e and IsEntityValid(e) then
        SetIsUI(e, true)
    end
end

local function HideDialog(e)
    if e and IsEntityValid(e) then
        SetIsUI(e, false)
    end
end

-- =========================================================
-- Z TRICK HELPERS � for hover swap only
-- =========================================================
local function HideByZ(e)
    if e and IsEntityValid(e) then
        local x, y, _ = GetPosition(e)
        SetPosition(e, x, y, HIDE_Z)
    end
end

local function ShowByZ(e, z)
    if e and IsEntityValid(e) then
        local x, y, _ = GetPosition(e)
        SetPosition(e, x, y, z)
    end
end

-- =========================================================
-- DIALOG SHOW / HIDE
-- =========================================================
local function SetDialogVisible(visible)
    -- print("[MapSelectionUI] SetDialogVisible: " .. tostring(visible))
    if visible then
        ShowDialog(de.overlay)
        ShowDialog(de.complete)
        ShowDialog(de.travelNormal)
        ShowDialog(de.stayNormal)
        -- hover entities: keep isUI true (needed for button input) but push to HIDE_Z
        ShowDialog(de.travelHover)
        ShowDialog(de.stayHover)
        HideByZ(de.travelHover)
        HideByZ(de.stayHover)
    else
        HideDialog(de.overlay)
        HideDialog(de.complete)
        HideDialog(de.travelNormal)
        HideDialog(de.travelHover)
        HideDialog(de.stayNormal)
        HideDialog(de.stayHover)
        -- restore Z so positions are clean for next open
        ShowByZ(de.travelHover, origZ.travelHover or 1)
        ShowByZ(de.stayHover,   origZ.stayHover   or 1)
        travelHovered = false
        stayHovered   = false
    end
end

-- =========================================================
-- TEXTURE SWAP
-- =========================================================
local function ApplyDialogTextures(locData)
    if not locData or not locData.dialogTextures then return end
    local t = locData.dialogTextures
    if de.complete     and IsEntityValid(de.complete)     and t.complete      then SetTexture(de.complete,     t.complete)      end
    if de.travelNormal and IsEntityValid(de.travelNormal) and t.button_normal then SetTexture(de.travelNormal, t.button_normal) end
    if de.travelHover  and IsEntityValid(de.travelHover)  and t.button_hover  then SetTexture(de.travelHover,  t.button_hover)  end
    if de.stayNormal   and IsEntityValid(de.stayNormal)   and t.stay_normal   then SetTexture(de.stayNormal,   t.stay_normal)   end
    if de.stayHover    and IsEntityValid(de.stayHover)    and t.stay_hover    then SetTexture(de.stayHover,    t.stay_hover)    end
end

-- =========================================================
-- HOVER HELPER
-- =========================================================
local function IsMouseOver(e)
    if not e or not IsEntityValid(e) then return false end
    local mx, my   = GetMousePosition()
    local ex, ey,_ = GetPosition(e)
    local sx, sy,_ = GetScale(e)
    return mx >= ex - sx*0.5 and mx <= ex + sx*0.5 and
           my >= ey - sy*0.5 and my <= ey + sy*0.5
end

-- =========================================================
-- ON INIT
-- =========================================================
function M:on_init()
    -- print("[MapSelectionUI] ===== INITIALIZATION =====")

    -- Cache dialog entity IDs
    de.overlay      = FindEntityByName(DIALOG_OVERLAY)
    de.complete     = FindEntityByName(DIALOG_COMPLETE)
    de.travelNormal = FindEntityByName(DIALOG_BTN_TRAVEL_NORMAL)
    de.travelHover  = FindEntityByName(DIALOG_BTN_TRAVEL_HOVER)
    de.stayNormal   = FindEntityByName(DIALOG_BTN_STAY_NORMAL)
    de.stayHover    = FindEntityByName(DIALOG_BTN_STAY_HOVER)

    -- print("[MapSelectionUI] --- Dialog Entity Cache ---")
    -- for k, e in pairs(de) do
    --     print("[MapSelectionUI]   " .. k .. " = " .. tostring(e) .. " valid=" .. tostring(e ~= nil and IsEntityValid(e)))
    -- end

    -- Cache original Z for all 4 button entities
    if de.travelNormal and IsEntityValid(de.travelNormal) then local _,_,z = GetPosition(de.travelNormal) origZ.travelNormal = z end
    if de.travelHover  and IsEntityValid(de.travelHover)  then local _,_,z = GetPosition(de.travelHover)  origZ.travelHover  = z end
    if de.stayNormal   and IsEntityValid(de.stayNormal)   then local _,_,z = GetPosition(de.stayNormal)   origZ.stayNormal   = z end
    if de.stayHover    and IsEntityValid(de.stayHover)    then local _,_,z = GetPosition(de.stayHover)    origZ.stayHover    = z end

    -- Hide dialog on init
    SetDialogVisible(false)

    -- Register location button callbacks
    for _, locData in pairs(locations) do
        local btn = FindEntityByName(locData.buttonEntity)
        if btn and IsEntityValid(btn) then
            -- print("[MapSelectionUI] Registered: " .. locData.buttonEntity .. " (ID: " .. btn .. ")")
            RegisterButtonCallback(btn, function()
                self:OnLocationSelected(locData)
            end)
        else
            -- print("[MapSelectionUI] WARNING: not found: " .. locData.buttonEntity)
        end
    end

    -- Register TRAVEL callbacks (normal + hover both fire confirm)
    if de.travelNormal and IsEntityValid(de.travelNormal) then
        RegisterButtonCallback(de.travelNormal, function()
            if pendingLocation then self:ConfirmTravel(pendingLocation) end
        end)
    end
    if de.travelHover and IsEntityValid(de.travelHover) then
        RegisterButtonCallback(de.travelHover, function()
            if pendingLocation then self:ConfirmTravel(pendingLocation) end
        end)
    end

    -- Register STAY callbacks (normal + hover both cancel)
    if de.stayNormal and IsEntityValid(de.stayNormal) then
        RegisterButtonCallback(de.stayNormal, function()
            -- print("[MapSelectionUI] Stay clicked � cancelling.")
            pendingLocation = nil
            SetDialogVisible(false)
        end)
    end
    if de.stayHover and IsEntityValid(de.stayHover) then
        RegisterButtonCallback(de.stayHover, function()
            -- print("[MapSelectionUI] Stay clicked � cancelling.")
            pendingLocation = nil
            SetDialogVisible(false)
        end)
    end

    -- Hide ingredient images
    for _, locData in pairs(locations) do
        local img = FindEntityByName(locData.ingredientImageEntity)
        if img and IsEntityValid(img) then
            SetEntityScale(img, 0, 0, 0)
        end
    end

    -- print("[MapSelectionUI] ===== INITIALIZATION COMPLETE =====")
end

-- =========================================================
-- ON UPDATE � hover swap via Z trick
-- =========================================================
function M:on_update(dt)
    if not IsEngineState("MapSelection") then return end
    if not pendingLocation then return end

    -- TRAVEL hover
    local overTravel = IsMouseOver(de.travelNormal)
    if overTravel ~= travelHovered then
        travelHovered = overTravel
        if overTravel then
            HideByZ(de.travelNormal)
            ShowByZ(de.travelHover, origZ.travelHover or 1)
        else
            ShowByZ(de.travelNormal, origZ.travelNormal or 1)
            HideByZ(de.travelHover)
        end
    end

    -- STAY hover
    local overStay = IsMouseOver(de.stayNormal)
    if overStay ~= stayHovered then
        stayHovered = overStay
        if overStay then
            HideByZ(de.stayNormal)
            ShowByZ(de.stayHover, origZ.stayHover or 1)
        else
            ShowByZ(de.stayNormal, origZ.stayNormal or 1)
            HideByZ(de.stayHover)
        end
    end
end

-- =========================================================
-- LOCATION SELECTED � show dialog
-- =========================================================
function M:OnLocationSelected(locData)
    -- print("[MapSelectionUI] Location selected: " .. locData.name)

    if not IsEngineState("MapSelection") then
        -- print("[MapSelectionUI] Ignoring � not in MapSelection state")
        return
    end

    if not locData.checkUnlocked() then
        -- print("[MapSelectionUI] " .. locData.name .. " is LOCKED")
        return
    end

    pendingLocation = locData
    ApplyDialogTextures(locData)
    SetDialogVisible(true)
end

-- =========================================================
-- CONFIRM TRAVEL
-- =========================================================
function M:ConfirmTravel(locData)
    -- print("[MapSelectionUI] Travel confirmed: " .. locData.name)

    SetDialogVisible(false)
    pendingLocation = nil

    for _, loc in pairs(locations) do
        local img = FindEntityByName(loc.ingredientImageEntity)
        if img and IsEntityValid(img) then SetEntityScale(img, 0, 0, 0) end
    end

    TeleportPlayer(
        locData.spawnPos.x, locData.spawnPos.y, locData.spawnPos.z,
        locData.spawnRot.x, locData.spawnRot.y, locData.spawnRot.z
    )

    FireEvent("StartPhaseTransition")
    FireEvent("RequestHerbSpawn")
    GameStateStart()
    SetUIFocused(false)
end

-- =========================================================
-- SHOW / HIDE MAP
-- =========================================================
function M:ShowMap()
    -- print("[MapSelectionUI] ShowMap")
    PushEngineState("MapSelection")
    SetUIFocused(true)
end

function M:HideMap()
    -- print("[MapSelectionUI] HideMap")
    GameStateStart()
    SetDialogVisible(false)
    pendingLocation = nil
    SetUIFocused(false)
end

return M