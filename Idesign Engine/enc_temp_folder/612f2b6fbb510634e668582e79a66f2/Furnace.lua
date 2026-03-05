local M = {}

-- ============================================================
-- [CONFIGURATION]
-- ============================================================

-- ── DYNAMIC SCREEN SCALING ────────────────────────────────────
local BASE_SCALE = 800         -- The scale at 1080p
local REF_HEIGHT = 1080        -- Reference resolution height
local SCREEN_POS_X = 0.75      -- 75% across the screen (Puts it on the Right)
local SCREEN_POS_Y = 0.50      -- 50% up the screen (Centers it Vertically)

-- ── BAR DIMENSIONS (logical units, scaled by PIXEL_SCALE) ────
local BAR_HEIGHT   = 0.7     
local BAR_WIDTH    = 0.14    
local CATCH_HEIGHT = 0.14    
local FISH_SIZE    = 0.05    

local HEAT_WIDTH   = 0.055   
local BAR_GAP      = 0.05    
local FLAT_Z       = 0.008   

local PREFAB_TRACK = "ProgressBarFrame_Prefab"
local PREFAB_FILL  = "ProgressBarFill_Prefab"

-- ── TEXTURES ─────────────────────────────────────────────────
local TEX_BG           = "Assets/Textures/ui_background.png"
local TEX_FRAME        = "Assets/Textures/track_frame.png"
local TEX_CATCH_IDLE   = "Assets/Textures/catch_idle.png"
local TEX_CATCH_ACTIVE = "Assets/Textures/catch_active.png"
local TEX_CATCH_GRACE  = "Assets/Textures/catch_grace.png"
local TEX_FISH         = "Assets/Textures/fish_ember.png"
local TEX_PROG = {
    "Assets/Textures/progress_cold.png",
    "Assets/Textures/progress_warm.png",
    "Assets/Textures/progress_hot.png",
    "Assets/Textures/progress_whitehot.png",
}

-- ── PLAYER TELEPORT SETTINGS ─────────────────────────────────
-- If the player teleports to the side/back of the furnace, change 
-- this to 0, 180, or 270 until they are facing the fire opening!
local FURNACE_FACING      = 90     
local PLAYER_DIST         = 3.0    -- How far back the player stands
local PLAYER_OFFSET_RIGHT = 0.0    -- Left/right shift from the furnace center

-- ── PHYSICS ───────────────────────────────────────────────────
local CATCH_GRAVITY  = 7.0
local CATCH_LIFT     = 10.5
local CATCH_MAX_VEL  = 5.0
local CATCH_BOUNCE   = 0.20
local FISH_SPEED_MIN = 1.2
local FISH_SPEED_MAX = 6.0
local FISH_IDLE_MIN  = 0.25
local FISH_IDLE_MAX  = 1.1
local PROGRESS_GAIN  = 0.28
local PROGRESS_DRAIN = 0.22
local WIN_THRESHOLD  = 1.0
local START_PROGRESS = 0.15
local SPAWN_Y_OFFSET = 2.0

-- ============================================================
-- [STATE]
-- ============================================================
local my_entity_id = nil
local is_active    = false
local progress, catch_pos, catch_vel = 0.0, 0.0, 0.0
local fish_pos, fish_target, fish_speed, fish_timer = 0.2, 0.2, 0.0, 0.0
local grace_timer  = 0.0

local cur_catch_tex = ""
local cur_prog_tex  = ""

local e_background = nil
local e_bar_frame  = nil
local e_heat_frame = nil
local e_catch      = nil
local e_fish       = nil
local e_heat_fill  = nil

-- ============================================================
-- HELPERS
-- ============================================================
local function Clamp(v, lo, hi) return math.max(lo, math.min(v, hi)) end
local function RandRange(lo, hi) return lo + math.random()*(hi-lo) end
local function Sign(v)
    if v > 0 then return 1 elseif v < 0 then return -1 else return 0 end
end
local function SwapTex(e, new, cur)
    if new ~= cur then SetTexture(e, new) end
    return new
end
local function GetProgTex(p)
    local t = p / WIN_THRESHOLD
    if     t < 0.25 then return TEX_PROG[1]
    elseif t < 0.50 then return TEX_PROG[2]
    elseif t < 0.75 then return TEX_PROG[3]
    else                  return TEX_PROG[4] end
end

-- ============================================================
-- INIT
-- ============================================================
function M.on_init(self)
    my_entity_id = self.entity
    math.randomseed(os.time())
end

function M.OnInteract()
    if is_active then M.StopMinigame() else M.StartMinigame() end
end

-- ============================================================
-- START
-- ============================================================
function M.StartMinigame()
    is_active   = true
    progress    = START_PROGRESS
    catch_pos   = BAR_HEIGHT * 0.5 - CATCH_HEIGHT * 0.5
    catch_vel   = 0.0
    fish_pos    = 0.2
    fish_target = RandRange(0.1, BAR_HEIGHT - 0.1)
    fish_speed  = RandRange(FISH_SPEED_MIN, FISH_SPEED_MAX)
    fish_timer  = RandRange(FISH_IDLE_MIN,  FISH_IDLE_MAX)
    grace_timer = 1.2
    cur_catch_tex = ""
    cur_prog_tex  = ""

    -- Lock camera and stop player
    SetCameraLock(true)
    local pid = FindEntityByName("Player")
    if pid and SetLinearVelocity then SetLinearVelocity(pid, 0, 0, 0) end

    -- ── TELEPORT PLAYER TO FURNACE ───────────────────────────
    local cx, cy, cz = GetPosition(my_entity_id)
    local rx, ry, rz = GetRotation(my_entity_id)

    if pid and cx then
        local fr  = math.rad(ry + FURNACE_FACING)
        local fwd_x = math.cos(fr)
        local fwd_z = math.sin(fr)
        local rr    = fr + math.pi * 0.5
        local right_x = math.cos(rr)
        local right_z = math.sin(rr)

        local tx  = cx + fwd_x * PLAYER_DIST + right_x * PLAYER_OFFSET_RIGHT
        local tz  = cz + fwd_z * PLAYER_DIST + right_z * PLAYER_OFFSET_RIGHT
        local px, py, pz = GetPosition(pid)
        local yaw = math.deg(math.atan2(-fwd_z, -fwd_x))
        if TeleportPlayer then TeleportPlayer(tx, py, tz, 0, yaw, 0) end
        if SetCameraRot   then SetCameraRot(0.0, yaw) end
    end
    -- ─────────────────────────────────────────────────────────

    -- Spawn entities
    e_background = SpawnPrefab(PREFAB_FILL, my_entity_id)
    e_bar_frame  = SpawnPrefab(PREFAB_TRACK, my_entity_id)
    e_heat_frame = SpawnPrefab(PREFAB_TRACK, my_entity_id)
    e_catch      = SpawnPrefab(PREFAB_FILL,  my_entity_id)
    e_fish       = SpawnPrefab(PREFAB_FILL,  my_entity_id)
    e_heat_fill  = SpawnPrefab(PREFAB_FILL,  my_entity_id)

    SetIsUI(e_background, true)
    SetIsUI(e_bar_frame,  true)
    SetIsUI(e_heat_frame, true)
    SetIsUI(e_catch,      true)
    SetIsUI(e_fish,       true)
    SetIsUI(e_heat_fill,  true)

    SetTexture(e_background, TEX_BG)
    SetTexture(e_bar_frame,  TEX_FRAME)
    SetTexture(e_heat_frame, TEX_FRAME)
    SetTexture(e_catch,      TEX_CATCH_GRACE)
    SetTexture(e_fish,       TEX_FISH)
    SetTexture(e_heat_fill,  TEX_PROG[1])
end

-- ============================================================
-- STOP
-- ============================================================
function M.StopMinigame()
    is_active = false
    SetCameraLock(false)

    local pid = FindEntityByName("Player")
    if pid and SetLinearVelocity then SetLinearVelocity(pid, 0, 0, 0) end

    local function Safe(e) if e then DestroyEntity(e) end end
    Safe(e_background)
    Safe(e_bar_frame); Safe(e_heat_frame)
    Safe(e_catch); Safe(e_fish); Safe(e_heat_fill)
    e_background = nil
    e_bar_frame, e_heat_frame     = nil, nil
    e_catch, e_fish, e_heat_fill  = nil, nil, nil
end

-- ============================================================
-- UPDATE
-- ============================================================
function M:on_update(dt)
    if not is_active then return end
    if dt > 0.05 then dt = 0.05 end
    if grace_timer > 0 then grace_timer = grace_timer - dt end

    -- 1. CATCH PHYSICS
    if IsMouseButtonDown(0) then catch_vel = catch_vel + CATCH_LIFT * dt
    else                         catch_vel = catch_vel - CATCH_GRAVITY * dt end
    catch_vel = Clamp(catch_vel, -CATCH_MAX_VEL, CATCH_MAX_VEL)
    local np = catch_pos + catch_vel * dt
    if     np <= 0                       then np = 0;                       catch_vel =  math.abs(catch_vel) * CATCH_BOUNCE
    elseif np >= BAR_HEIGHT-CATCH_HEIGHT then np = BAR_HEIGHT-CATCH_HEIGHT; catch_vel = -math.abs(catch_vel) * CATCH_BOUNCE end
    catch_pos = np

    -- 2. FISH AI
    fish_timer = fish_timer - dt
    if fish_timer <= 0 then
        fish_target = RandRange(0.05, BAR_HEIGHT - 0.05)
        fish_speed  = RandRange(FISH_SPEED_MIN, FISH_SPEED_MAX)
        fish_timer  = RandRange(FISH_IDLE_MIN,  FISH_IDLE_MAX)
    end
    local df = fish_target - fish_pos
    if math.abs(df) > 0.01 then
        local step = Sign(df) * fish_speed * dt
        if math.abs(step) > math.abs(df) then step = df end
        fish_pos = fish_pos + step
    end
    fish_pos = Clamp(fish_pos, 0, BAR_HEIGHT)

    -- 3. PROGRESS
    local inside = fish_pos >= catch_pos and fish_pos <= catch_pos + CATCH_HEIGHT
    if inside             then progress = progress + PROGRESS_GAIN  * dt
    elseif grace_timer<=0 then progress = progress - PROGRESS_DRAIN * dt end
    progress = Clamp(progress, 0.0, WIN_THRESHOLD)

    -- 4. TEXTURE SWAPS
    local ct
    if   grace_timer > 0 then ct = TEX_CATCH_GRACE
    elseif inside        then ct = TEX_CATCH_ACTIVE
    else                      ct = TEX_CATCH_IDLE end
    cur_catch_tex = SwapTex(e_catch,     ct,                   cur_catch_tex)
    cur_prog_tex  = SwapTex(e_heat_fill, GetProgTex(progress), cur_prog_tex)

    -- 5. DYNAMIC UI POSITIONING & SCALING
    local screen_w, screen_h = 1920, 1080
    if GetScreenSize then 
        screen_w, screen_h = GetScreenSize() 
    end

    -- Calculate current scale and anchor positions
    local PS = (screen_h / REF_HEIGHT) * BASE_SCALE
    local UI_BASE_X = screen_w * SCREEN_POS_X
    local UI_BASE_Y = screen_h * SCREEN_POS_Y

    -- Offsets and sizes
    local left_cx  = -(BAR_WIDTH * 0.5 + BAR_GAP * 0.5) * PS
    local right_cx =  (HEAT_WIDTH * 0.5 + BAR_GAP * 0.5) * PS
    local bar_cy   = 0  -- Since we anchor at 0.5 (middle), this needs to just be 0 so it centers!

    -- Depths
    local D_FRAME = 0.0
    local D_FILL  = 1.0   
    local D_FISH  = 2.0

    -- Apply Dynamic Scales
    local BG_W = (BAR_WIDTH + BAR_GAP + HEAT_WIDTH + 0.16) * PS
    local BG_H = (BAR_HEIGHT + 0.18) * PS
    SetScale(e_background, BG_W, BG_H, FLAT_Z)
    SetScale(e_bar_frame,  BAR_WIDTH * PS, BAR_HEIGHT * PS, FLAT_Z)
    SetScale(e_heat_frame, HEAT_WIDTH * PS, BAR_HEIGHT * PS, FLAT_Z)
    SetScale(e_catch,      BAR_WIDTH * 0.72 * PS, CATCH_HEIGHT * PS, FLAT_Z)
    SetScale(e_fish,       FISH_SIZE * PS, FISH_SIZE * PS, FLAT_Z)
    
    local ph = math.max(0.01, (progress / WIN_THRESHOLD) * BAR_HEIGHT)
    local ph_px = ph * PS
    SetScale(e_heat_fill,  HEAT_WIDTH * 0.72 * PS, ph_px, FLAT_Z)

    -- Apply Dynamic Positions (Subtracting half the bar height to center the Y anchor)
    local bg_cx = UI_BASE_X + (left_cx + right_cx) * 0.5
    local center_y_offset = -(BAR_HEIGHT * 0.5) * PS
    SetPosition(e_background, bg_cx, UI_BASE_Y, D_FRAME - 1.0)
    
    SetPosition(e_bar_frame,  UI_BASE_X + left_cx,  UI_BASE_Y, D_FRAME)
    SetPosition(e_heat_frame, UI_BASE_X + right_cx, UI_BASE_Y, D_FRAME)

    local catch_cy_px = ((catch_pos + CATCH_HEIGHT * 0.5) * PS) + center_y_offset
    SetPosition(e_catch, UI_BASE_X + left_cx, UI_BASE_Y + catch_cy_px, D_FILL)

    local fish_cy_px = ((fish_pos + FISH_SIZE * 0.5) * PS) + center_y_offset
    SetPosition(e_fish, UI_BASE_X + left_cx, UI_BASE_Y + fish_cy_px, D_FISH)

    SetPosition(e_heat_fill, UI_BASE_X + right_cx, UI_BASE_Y + (ph_px * 0.5) + center_y_offset, D_FILL)

    -- 6. WIN / LOSE
    if progress >= WIN_THRESHOLD then M.CompleteSmelting()
    elseif progress <= 0.0 and grace_timer <= 0 then M.FailSmelting() end
end

function M.CompleteSmelting()
    M.StopMinigame()
    local cx, cy, cz = GetPosition(my_entity_id)
    if cx then SpawnPrefab("iron_bar_Prefab", cx, cy + SPAWN_Y_OFFSET, cz) end
end

function M.FailSmelting() M.StopMinigame() end

return M