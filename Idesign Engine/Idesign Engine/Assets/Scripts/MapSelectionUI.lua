-- MapSelectionUI.lua
-- WORKAROUND: Manually control button visibility using scale
-- This bypasses potential parent/child rendering issues

local M = {}

-- Location data with ingredients
local locations = {
    Forest = {
        name = "Forest",
        buttonEntity = "Forest Button",
        spawnPos = {x = -53.965, y = 9.0, z = 9.5},
        spawnRot = {x = 0.0, y = 90.0, z = 0.0},
        ingredients = {"Forest Herbs", "Mushrooms", "Berries"},
        ingredientImageEntity = "ForestIngredientsImage"
    },
    Quarry = {
        name = "Quarry",
        buttonEntity = "Quarry Button",
        spawnPos = {x = -36.0, y = 9.0, z = 9.5},
        spawnRot = {x = 0.0, y = 0.0, z = 0.0},
        ingredients = {"Stone Minerals", "Cave Moss", "Crystal Dust"},
        ingredientImageEntity = "QuarryIngredientsImage"
    },
    House = {
        name = "House",
        buttonEntity = "House Button",
        spawnPos = {x = -36.0, y = 8.025, z = 9.5},
        spawnRot = {x = 0.0, y = 0.0, z = 0.0},
        ingredients = {"Garden Herbs", "Kitchen Spices"},
        ingredientImageEntity = "HouseIngredientsImage"
    }
}

local currentHoveredLocation = nil

function M:on_init()
    print("[MapSelectionUI] ===== INITIALIZATION =====")
    print("[MapSelectionUI] Entity ID: " .. tostring(self.entity))
    
    -- Setup button callbacks for each location
    for locName, locData in pairs(locations) do
        local buttonId = FindEntityByName(locData.buttonEntity)
        if buttonId and IsEntityValid(buttonId) then
            print("[MapSelectionUI] Found button: " .. locData.buttonEntity .. " (ID: " .. buttonId .. ")")
            
            -- Check initial scale
            local sx, sy, sz = GetScale(buttonId)
            print("[MapSelectionUI]   Initial scale: [" .. sx .. ", " .. sy .. ", " .. sz .. "]")
            
            -- Register click callback
            RegisterButtonCallback(buttonId, function(ent)
                print("[MapSelectionUI] ===== BUTTON CLICKED: " .. locData.name .. " =====")
                self:OnLocationSelected(locData)
            end)
        else
            print("[MapSelectionUI] ERROR: Button not found: " .. locData.buttonEntity)
        end
    end
    
    -- Initialize ingredient images as hidden
    for _, locData in pairs(locations) do
        local ingredientImg = FindEntityByName(locData.ingredientImageEntity)
        if ingredientImg and IsEntityValid(ingredientImg) then
            SetEntityScale(ingredientImg, 0.0, 0.0, 0.0)
        end
    end
    
    print("[MapSelectionUI] ===== INITIALIZATION COMPLETE =====")
end

function M:on_update(dt)
    -- Placeholder for hover logic
end

function M:OnLocationHover(locData)
    print("[MapSelectionUI] Hovering over: " .. locData.name)
    
    local ingredientImg = FindEntityByName(locData.ingredientImageEntity)
    if ingredientImg and IsEntityValid(ingredientImg) then
        SetEntityScale(ingredientImg, 1.0, 1.0, 1.0)
    end
end

function M:OnLocationUnhover(locData)
    print("[MapSelectionUI] Stopped hovering over: " .. locData.name)
    
    local ingredientImg = FindEntityByName(locData.ingredientImageEntity)
    if ingredientImg and IsEntityValid(ingredientImg) then
        SetEntityScale(ingredientImg, 0.0, 0.0, 0.0)
    end
end

function M:OnLocationSelected(locData)
    print("[MapSelectionUI] ===== LOCATION SELECTED: " .. locData.name .. " =====")
    
    -- Hide all ingredient images
    for _, loc in pairs(locations) do
        self:OnLocationUnhover(loc)
    end
    
    -- Exit MapSelection state
    print("[MapSelectionUI] Popping MapSelection state")
    GameStateStart()
    SetUIFocused(false)
    
    -- Teleport player to selected location
    print("[MapSelectionUI] Teleporting player to: " .. locData.name)
    print("[MapSelectionUI]   Position: [" .. locData.spawnPos.x .. ", " .. locData.spawnPos.y .. ", " .. locData.spawnPos.z .. "]")
    print("[MapSelectionUI]   Rotation: [" .. locData.spawnRot.x .. ", " .. locData.spawnRot.y .. ", " .. locData.spawnRot.z .. "]")
    
    TeleportPlayer(
        locData.spawnPos.x, locData.spawnPos.y, locData.spawnPos.z,
        locData.spawnRot.x, locData.spawnRot.y, locData.spawnRot.z
    )
    
    print("[MapSelectionUI] ===== TELEPORT COMPLETE =====")
end

function M:ShowMap()
    print("[MapSelectionUI] ===== SHOWING MAP =====")
    
    -- Push MapSelection state - SceneInterface handles visibility via isUI flag
    PushEngineState("MapSelection")
    print("[MapSelectionUI] Pushed MapSelection state")
    
    -- Set UI focus for cursor
    SetUIFocused(true)
    
    print("[MapSelectionUI] ===== MAP VISIBLE =====")
end

function M:HideMap()
    print("[MapSelectionUI] ===== HIDING MAP =====")
    
    -- Pop MapSelection state - SceneInterface handles visibility
    PopEngineState()
    
    -- Hide all ingredient images
    for _, locData in pairs(locations) do
        self:OnLocationUnhover(locData)
    end
    
    -- Remove UI focus
    SetUIFocused(false)
    
    print("[MapSelectionUI] ===== MAP HIDDEN =====")
end

return M


















































