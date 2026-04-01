-- MapSelectionUI.lua
--
-- DIALOG VISIBILITY: SetIsUI called directly on the 6 cached dialog entity IDs.
-- No tag dependency for dialog - visibility fully managed from Lua.
-- Forest/Quarry/House map buttons still rely on SceneInterface via "map" tag.
--
-- HOVER SWAP: Z trick - hover entity pushed to HIDE_Z until mouse is over button.

local M = {}

-- Initialise quarry tutorial flags so GetData never errors on first read
SetData("quarryFirstEntered",  0)
SetData("quarryTutorialPlayed", 0)

local HIDE_Z = -99999.0  -- offscreen Z to hide an entity from rendering

-- =========================================================
-- MAP ICON
-- =========================================================
local PLAYER_MAP_ICON = "PlayerMapIcon"
local mapIcon = nil
local currentMapLocation = 0   -- 0 = none, 1 = Forest, 2 = Quarry

-- Timer should only start on first travel of the day
local dayTimerStarted = false

local iconPositions = {
    [0] = { x = 960.0,  y = 250.0, z = 1.0 },   -- House (Middle Bottom)
    [1] = { x = 462.0,  y = 745.0, z = 1.0 },   -- Forest
    [2] = { x = 1410.0, y = 738.0, z = 1.0 }    -- Quarry
}

-- =========================================================
-- ANIMATION VARIABLES
-- =========================================================
local isMoving = false
local moveTimer = 0.0
local moveDuration = 1.0 -- How long the travel takes in seconds (adjust to your liking)
local startPos = {x = 0, y = 0, z = 0}
local targetPos = {x = 0, y = 0, z = 0}
local pendingTravelData = nil -- Remembers where we are teleporting to

-- Standard Linear Interpolation function
local function Lerp(a, b, t)
    return a + (b - a) * t
end

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
local DIALOG_OVERLAY           = "dialog_overlay"
local DIALOG_COMPLETE          = "dialog_complete"
local DIALOG_BTN_TRAVEL_NORMAL = "dialog_btn_travel_normal"
local DIALOG_BTN_TRAVEL_HOVER  = "dialog_btn_travel_hover"
local DIALOG_BTN_STAY_NORMAL   = "dialog_btn_stay_normal"
local DIALOG_BTN_STAY_HOVER    = "dialog_btn_stay_hover"

local de = {}
local origZ = {}

local pendingLocation = nil
local travelHovered   = false
local stayHovered     = false

-- =========================================================
-- SETISUI HELPERS
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
-- Z TRICK HELPERS
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
-- MAP ICON HELPERS
-- =========================================================
local function HidePlayerMapIcon()
    if mapIcon and IsEntityValid(mapIcon) then
        local x, y, _ = GetPosition(mapIcon)
        SetPosition(mapIcon, x, y, HIDE_Z)
    end
end

local function RefreshPlayerMapIcon()
    if not mapIcon or not IsEntityValid(mapIcon) then
        return
    end

    local pos = iconPositions[currentMapLocation]
    if pos then
        SetPosition(mapIcon, pos.x, pos.y, pos.z)
    else
        HidePlayerMapIcon()
    end
end

-- =========================================================
-- DIALOG SHOW / HIDE
-- =========================================================
local function SetDialogVisible(visible)
    if visible then
        ShowDialog(de.overlay)
        ShowDialog(de.complete)
        ShowDialog(de.travelNormal)
        ShowDialog(de.stayNormal)

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

    return mx >= ex - sx * 0.5 and mx <= ex + sx * 0.5 and
           my >= ey - sy * 0.5 and my <= ey + sy * 0.5
end

local function CheckAndApplyMapReset()
    local resetFlag = 0
    local ok, result = pcall(function()
        return GetData("ResetMapIconNextOpen")
    end)

    if ok and result then
        resetFlag = result
    end

    if resetFlag == 1 then
        currentMapLocation = 0
        dayTimerStarted = false
        
        -- INSTEAD OF HIDING THE ICON, SNAP IT TO THE HOUSE
        if mapIcon and IsEntityValid(mapIcon) then
            local pos = iconPositions[0]
            SetPosition(mapIcon, pos.x, pos.y, pos.z)
        end
        
        SetData("ResetMapIconNextOpen", 0)
        print("[MapSelectionUI] Map icon reset to House for new day")
        print("[MapSelectionUI] Day travel timer reset")
    end
end

-- =========================================================
-- ON INIT
-- =========================================================
function M:on_init()
    de.overlay      = FindEntityByName(DIALOG_OVERLAY)
    de.complete     = FindEntityByName(DIALOG_COMPLETE)
    de.travelNormal = FindEntityByName(DIALOG_BTN_TRAVEL_NORMAL)
    de.travelHover  = FindEntityByName(DIALOG_BTN_TRAVEL_HOVER)
    de.stayNormal   = FindEntityByName(DIALOG_BTN_STAY_NORMAL)
    de.stayHover    = FindEntityByName(DIALOG_BTN_STAY_HOVER)

    mapIcon = FindEntityByName(PLAYER_MAP_ICON)

    if de.travelNormal and IsEntityValid(de.travelNormal) then
        local _, _, z = GetPosition(de.travelNormal)
        origZ.travelNormal = z
    end
    if de.travelHover and IsEntityValid(de.travelHover) then
        local _, _, z = GetPosition(de.travelHover)
        origZ.travelHover = z
    end
    if de.stayNormal and IsEntityValid(de.stayNormal) then
        local _, _, z = GetPosition(de.stayNormal)
        origZ.stayNormal = z
    end
    if de.stayHover and IsEntityValid(de.stayHover) then
        local _, _, z = GetPosition(de.stayHover)
        origZ.stayHover = z
    end

    SetDialogVisible(false)
    HidePlayerMapIcon()

    for _, locData in pairs(locations) do
        local btn = FindEntityByName(locData.buttonEntity)
        if btn and IsEntityValid(btn) then
            RegisterButtonCallback(btn, function()
                self:OnLocationSelected(locData)
            end)
        end
    end

    if de.travelNormal and IsEntityValid(de.travelNormal) then
        RegisterButtonCallback(de.travelNormal, function()
            if pendingLocation then
                self:ConfirmTravel(pendingLocation)
            end
        end)
    end

    if de.travelHover and IsEntityValid(de.travelHover) then
        RegisterButtonCallback(de.travelHover, function()
            if pendingLocation then
                self:ConfirmTravel(pendingLocation)
            end
        end)
    end

    if de.stayNormal and IsEntityValid(de.stayNormal) then
        RegisterButtonCallback(de.stayNormal, function()
            pendingLocation = nil
            SetDialogVisible(false)
        end)
    end

    if de.stayHover and IsEntityValid(de.stayHover) then
        RegisterButtonCallback(de.stayHover, function()
            pendingLocation = nil
            SetDialogVisible(false)
        end)
    end

    for _, locData in pairs(locations) do
        local img = FindEntityByName(locData.ingredientImageEntity)
        if img and IsEntityValid(img) then
            SetEntityScale(img, 0, 0, 0)
        end
    end
end

-- =========================================================
-- ON UPDATE
-- =========================================================
function M:on_update(dt)
    if not IsEngineState("MapSelection") then return end

    if type(dt) ~= "number" or dt <= 0 then
        dt = 0.016 
    end

    CheckAndApplyMapReset()

    -- ANIMATION LOGIC
    if isMoving then
        moveTimer = moveTimer + dt
        local t = moveTimer / moveDuration
        
        -- Watch the console for this! If it counts from 0.0 to 1.0, it's working.
        print(string.format("[DEBUG-ANIMATION] Timer: %.2f | Progress: %.2f%%", moveTimer, t * 100))
        
        if t >= 1.0 then
            t = 1.0
            isMoving = false
            SetPosition(mapIcon, targetPos.x, targetPos.y, targetPos.z)
            self:FinishTravel(pendingTravelData)
        else
            -- If Lerp is missing, the script will silently die on this exact line
            local newX = Lerp(startPos.x, targetPos.x, t)
            local newY = Lerp(startPos.y, targetPos.y, t)
            local newZ = Lerp(startPos.z, targetPos.z, t)
            SetPosition(mapIcon, newX, newY, newZ)
        end
    else
        -- Only run the instant snap if we are NOT animating
        RefreshPlayerMapIcon()
    end

    if not pendingLocation then return end

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
-- LOCATION SELECTED
-- =========================================================
function M:OnLocationSelected(locData)
    if not IsEngineState("MapSelection") then
        return
    end

    if not locData.checkUnlocked() then
        return
    end

    pendingLocation = locData
    ApplyDialogTextures(locData)
    SetDialogVisible(true)
end

-- =========================================================
-- CONFIRM TRAVEL (Starts the Animation)
-- =========================================================
function M:ConfirmTravel(locData)
    print("\n[DEBUG] --- ConfirmTravel TRIGGERED ---")
    print("[DEBUG] Destination requested: " .. tostring(locData.name))

    SetDialogVisible(false)
    pendingLocation = nil

    -- Determine the target location index
    local targetIdx = 0
    if locData.name == "Forest" then
        targetIdx = 1
    elseif locData.name == "Quarry" then
        targetIdx = 2
    elseif locData.name == "House" then
        targetIdx = 0
    end
    print("[DEBUG] Target index resolved to: " .. tostring(targetIdx))

    -- Check if the icon exists and is valid
    if mapIcon and IsEntityValid(mapIcon) then
        local cx, cy, cz = GetPosition(mapIcon)
        startPos = {x = cx, y = cy, z = cz}
        targetPos = iconPositions[targetIdx]
        
        print(string.format("[DEBUG] Start Pos: X=%.1f, Y=%.1f", startPos.x, startPos.y))
        print(string.format("[DEBUG] Target Pos: X=%.1f, Y=%.1f", targetPos.x, targetPos.y))
        
        currentMapLocation = targetIdx
        pendingTravelData = locData
        isMoving = true
        moveTimer = 0.0
        print("[DEBUG] isMoving set to TRUE. Waiting for on_update...")
    else
        print("[DEBUG] ERROR: mapIcon is nil or invalid! Skipping animation.")
        currentMapLocation = targetIdx
        self:FinishTravel(locData)
    end
end
-- =========================================================
-- FINISH TRAVEL (Happens after animation completes)
-- =========================================================
function M:FinishTravel(locData)
    for _, loc in pairs(locations) do
        local img = FindEntityByName(loc.ingredientImageEntity)
        if img and IsEntityValid(img) then
            SetEntityScale(img, 0, 0, 0)
        end
    end

    if locData.name == "Quarry" and GetData("quarryTutorialPlayed") == 0 then
        SetData("quarryFirstEntered", 1)
    end

    TeleportPlayer(
        locData.spawnPos.x, locData.spawnPos.y, locData.spawnPos.z,
        locData.spawnRot.x, locData.spawnRot.y, locData.spawnRot.z
    )

    FireEvent("StartPhaseTransition")
    FireEvent("RequestHerbSpawn")

    -- Start timer ONLY on first travel of the day
    if not dayTimerStarted then
        GameStateStart()
        dayTimerStarted = true
        print("[MapSelectionUI] First travel of day -> timer started")
    else
        print("[MapSelectionUI] Travel during same day -> timer continues")
    end

    PopEngineState()
    SetUIFocused(false)
end

-- =========================================================
-- SHOW / HIDE MAP
-- =========================================================
function M:ShowMap()
    PushEngineState("MapSelection")
    SetUIFocused(true)

    CheckAndApplyMapReset()
    RefreshPlayerMapIcon()
end

function M:HideMap()
    SetDialogVisible(false)
    pendingLocation = nil
    PopEngineState()
    SetUIFocused(false)
end

return M