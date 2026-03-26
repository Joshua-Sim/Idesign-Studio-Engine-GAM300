-- StatsSystem.lua
-- Attach to a persistent entity (e.g. "StatsController").
-- Controls both the per-day screen (tag: "daystats") and the
-- end-of-game journey screen (tag: "endstats").
--
-- KEY BEHAVIOUR:
--   All stats text entities start with screenSpace=false (set in on_init).
--   screenSpace is only enabled when their screen is active, and disabled again
--   when it deactivates. This ensures stats text is NEVER rendered outside
--   its intended engine state.

local DayStatsScreen = {}

local TOTAL_DAYS  = 5
local TOTAL_STARS = TOTAL_DAYS * 3   -- 15 for End Stats Screen

local PREFAB_STAR = "star_Prefab"

-- ── Star sizes ────────────────────────────────────────────────
local DAY_STAR_SCALE     = 60    -- per-day screen large stars
local ROW_STAR_SCALE     = 20    -- end screen row stars (per-day column)
local GRID_STAR_SCALE    = 40    -- end screen total grid stars

-- ── Populate flags ────────────────────────────────────────────
local dayScreenPopulated = false
local endScreenPopulated = false

-- ── Spawned star tracking ─────────────────────────────────────
local spawnedDayStars   = {}
local spawnedRowStars   = {}
local spawnedTotalStars = {}

-- ── All text entity names for each screen ─────────────────────
local DAY_TEXT_NAMES = {
    "Stats_DayLabel",
    "Stats_GoldFromCustomers",
    "Stats_TimeBonusGold",
    "Stats_TotalGold",
    "Stats_CustomersServed",
    "Stats_TimeRemaining",
    "Stats_PerformanceLabel",
    "Stats_Quote",
}

local END_TEXT_NAMES = {
    "EndStats_Row1Gold",   "EndStats_Row1Served",
    "EndStats_Row2Gold",   "EndStats_Row2Served",
    "EndStats_Row3Gold",   "EndStats_Row3Served",
    "EndStats_Row4Gold",   "EndStats_Row4Served",
    "EndStats_Row5Gold",   "EndStats_Row5Served",
    "EndStats_TotalGold",
    "EndStats_TotalServed",
    "EndStats_Title",
    "EndStats_Quote",
}

-- ── Star rating labels and quotes (keyed 0-3) ─────────────────
local STAR_LABELS = {
    [0] = "No Stars",
    [1] = "Getting Started",
    [2] = "Decent Work",
    [3] = "Outstanding!"
}
local STAR_QUOTES = {
    [0] = '"Even the greatest alchemists had rough days. Tomorrow is another chance."',
    [1] = '"A steady hand learns from each brew. Keep at it."',
    [2] = '"A steady hand and a sturdy cauldron - not bad for a day\'s work."',
    [3] = '"The remedies flowed like a river today. The village is grateful."'
}

-- ── End-screen title / quote ──────────────────────────────────
local function GetEndTitle(total)
    if total >= 15 then return "Legendary Alchemist"
    elseif total >= 12 then return "Renowned Brewer"
    elseif total >= 9  then return "Skilled Potion Maker"
    elseif total >= 5  then return "Apprentice Brewer"
    else return "Novice Alchemist" end
end

local function GetEndQuote(total)
    if total >= 15 then
        return '"Every remedy was perfect. The land will sing of your craft for generations."'
    elseif total >= 12 then
        return '"Through forest paths and quarry dust, your remedies brought comfort to all who sought them. The land will remember your craft."'
    elseif total >= 9 then
        return '"A commendable journey. Many were helped by your brews."'
    elseif total >= 5 then
        return '"You learned much on this road. Return stronger next time."'
    else
        return '"The cauldron bubbled, but not always with success. There is more to learn."'
    end
end

-- ── Helpers ───────────────────────────────────────────────────
local function FormatTime(seconds)
    local s = math.floor(seconds)
    local m = math.floor(s / 60)
    return string.format("%d:%02d", m, s % 60)
end

-- Enable screenSpace + set text + set opacity
local function SafeSetText(name, value)
    local e = FindEntityByName(name)
    if not e then return end
    SetTextComponent(e, "screenSpace", true)
    SetTextComponent(e, "text",        tostring(value))
    SetTextComponent(e, "opacity",     1.0)
end

-- Disable screenSpace + zero opacity (completely removes from UI text pass)
local function HideText(name)
    local e = FindEntityByName(name)
    if not e then return end
    SetTextComponent(e, "opacity",     0.0)
    SetTextComponent(e, "screenSpace", false)
end

-- Disable screenSpace on a list of entity names
local function HideTextList(names)
    for _, name in ipairs(names) do
        HideText(name)
    end
end

local function ClearEntities(list)
    for _, e in ipairs(list) do
        if e then DestroyEntity(e) end
    end
end

-- ═════════════════════════════════════════════════════════════
-- DAY SCREEN STARS
-- 3 large stars under "Today's Performance"
-- PLACEHOLDER positions — tune X/Y once visible in game
-- ═════════════════════════════════════════════════════════════
local DAY_STAR_POSITIONS = {
    { x = 880,  y = 383, z = 0.6 },   -- Star 1 (left)
    { x = 960,  y = 383, z = 0.6 },   -- Star 2 (center)
    { x = 1040, y = 383, z = 0.6 },   -- Star 3 (right)
}

local function SpawnDayStars(stars)
    ClearEntities(spawnedDayStars)
    spawnedDayStars = {}
    for i = 1, stars do
        local pos = DAY_STAR_POSITIONS[i]
        local e = SpawnPrefab(PREFAB_STAR, pos.x, pos.y, pos.z)
        if e then
            SetScale(e, DAY_STAR_SCALE, DAY_STAR_SCALE, 1)
            table.insert(spawnedDayStars, e)
        end
    end
end

-- ═════════════════════════════════════════════════════════════
-- END SCREEN — ROW STARS
-- 3 small stars in STARS column per day row
-- PLACEHOLDER positions — tune once visible in game
-- ═════════════════════════════════════════════════════════════
local ROW_STAR_START_X   = 1213
local ROW_STAR_SPACING_X = 22
local ROW_STAR_START_Y   = 825
local ROW_STAR_SPACING_Y = -57   -- negative = moves up each row
local ROW_STAR_Z         = 1.0

local function SpawnRowStars(history)
    ClearEntities(spawnedRowStars)
    spawnedRowStars = {}
    for i, d in ipairs(history) do
        for s = 1, d.stars do
            local x = ROW_STAR_START_X + (s - 1) * ROW_STAR_SPACING_X
            local y = ROW_STAR_START_Y + (i - 1) * ROW_STAR_SPACING_Y
            local e = SpawnPrefab(PREFAB_STAR, x, y, ROW_STAR_Z)
            if e then
                SetScale(e, ROW_STAR_SCALE, ROW_STAR_SCALE, 1)
                table.insert(spawnedRowStars, e)
            end
        end
    end
end

-- ═════════════════════════════════════════════════════════════
-- END SCREEN — TOTAL STAR GRID
-- Up to 15 small stars in a 5×3 grid
-- PLACEHOLDER positions — tune once visible in game
-- ═════════════════════════════════════════════════════════════
local GRID_COLS      = 5
local GRID_START_X   = 866
local GRID_SPACING_X = 50
local GRID_START_Y   = 378
local GRID_SPACING_Y = -45   -- negative = moves up each row
local GRID_Z         = 1.0

local function SpawnTotalStars(totalStars)
    ClearEntities(spawnedTotalStars)
    spawnedTotalStars = {}
    for i = 1, totalStars do
        local col = (i - 1) % GRID_COLS
        local row = math.floor((i - 1) / GRID_COLS)
        local x = GRID_START_X + col * GRID_SPACING_X
        local y = GRID_START_Y + row * GRID_SPACING_Y
        local e = SpawnPrefab(PREFAB_STAR, x, y, GRID_Z)
        if e then
            SetScale(e, GRID_STAR_SCALE, GRID_STAR_SCALE, 1)
            table.insert(spawnedTotalStars, e)
        end
    end
end

-- ═════════════════════════════════════════════════════════════
-- PER-DAY SCREEN POPULATE
-- ═════════════════════════════════════════════════════════════
function DayStatsScreen.PopulateDayScreen()
    local d = GetLastDayStats()
    if not d then
        print("[StatsSystem] ERROR: GetLastDayStats returned nil")
        return
    end

    SafeSetText("Stats_DayLabel",          "Day " .. d.dayIndex .. " of " .. TOTAL_DAYS)
    SafeSetText("Stats_GoldFromCustomers", tostring(d.goldFromCustomers))
    SafeSetText("Stats_TimeBonusGold",     "+ " .. d.timeBonusGold .. " G day time bonus")
    SafeSetText("Stats_TotalGold",         d.totalGold .. " G")
    SafeSetText("Stats_CustomersServed",   tostring(d.customersServed))
    SafeSetText("Stats_TimeRemaining",     FormatTime(d.timeRemaining))
    SafeSetText("Stats_PerformanceLabel",  STAR_LABELS[d.stars] or "Decent Work")
    SafeSetText("Stats_Quote",             STAR_QUOTES[d.stars] or "")

    SpawnDayStars(d.stars)

    print("[StatsSystem] Day " .. d.dayIndex .. " screen populated. Stars: " .. d.stars)
end

-- ═════════════════════════════════════════════════════════════
-- END-OF-GAME SCREEN POPULATE
-- ═════════════════════════════════════════════════════════════
function DayStatsScreen.PopulateEndScreen()
    local history = GetAllDayStats()
    if not history then
        print("[StatsSystem] ERROR: GetAllDayStats returned nil")
        return
    end

    local totalGold, totalServed, totalStars = 0, 0, 0

    for i = 1, TOTAL_DAYS do
        local d = history[i]
        if d then
            totalGold   = totalGold   + d.totalGold
            totalServed = totalServed + d.customersServed
            totalStars  = totalStars  + d.stars
            SafeSetText("EndStats_Row" .. i .. "Gold",   tostring(d.totalGold))
            SafeSetText("EndStats_Row" .. i .. "Served", tostring(d.customersServed))
        else
            SafeSetText("EndStats_Row" .. i .. "Gold",   "0")
            SafeSetText("EndStats_Row" .. i .. "Served", "0")
        end
    end

    SafeSetText("EndStats_TotalGold",   string.format("%d G", totalGold))
    SafeSetText("EndStats_TotalServed", tostring(totalServed))
    SafeSetText("EndStats_Title",       GetEndTitle(totalStars))
    SafeSetText("EndStats_Quote",       GetEndQuote(totalStars))

    SpawnRowStars(history)
    SpawnTotalStars(totalStars)

    print("[StatsSystem] End screen populated. Total stars: " .. totalStars .. "/" .. TOTAL_STARS)
end

-- ═════════════════════════════════════════════════════════════
-- ECS HOOKS
-- ═════════════════════════════════════════════════════════════
function DayStatsScreen:on_init()
    print("[StatsSystem] Initialized.")

    -- Disable screenSpace on ALL stats text at startup.
    -- This prevents them from entering the UI text render pass
    -- until their respective screen is actually active.
    HideTextList(DAY_TEXT_NAMES)
    HideTextList(END_TEXT_NAMES)

    dayScreenPopulated = false
    endScreenPopulated = false
    spawnedDayStars    = {}
    spawnedRowStars    = {}
    spawnedTotalStars  = {}
end

function DayStatsScreen:on_update(dt)
    -- ── Per-day screen ─────────────────────────────────────────
    if HasEngineState("DayStats") then
        if not dayScreenPopulated then
            DayStatsScreen.PopulateDayScreen()  -- enables screenSpace on day text
            dayScreenPopulated = true
        end
    else
        if dayScreenPopulated then
            -- Screen deactivated — disable screenSpace on all day text
            HideTextList(DAY_TEXT_NAMES)
            ClearEntities(spawnedDayStars)
            spawnedDayStars = {}
        end
        dayScreenPopulated = false
    end

    -- ── End screen ─────────────────────────────────────────────
    if HasEngineState("EndStats") then
        if not endScreenPopulated then
            DayStatsScreen.PopulateEndScreen()  -- enables screenSpace on end text
            endScreenPopulated = true
        end
    else
        if endScreenPopulated then
            -- Screen deactivated — disable screenSpace on all end text
            HideTextList(END_TEXT_NAMES)
            ClearEntities(spawnedRowStars)
            ClearEntities(spawnedTotalStars)
            spawnedRowStars   = {}
            spawnedTotalStars = {}
        end
        endScreenPopulated = false
    end
end

return DayStatsScreen