local Mortar = {}

local PREFAB_CIRCLE = "TapCircle_Prefab"
local PREFAB_GOOD   = "GoodPound_Prefab"
local PREFAB_MEDIUM = "MediumPound_Prefab"
local PREFAB_POOR   = "PoorPound_Prefab"

-- --- INTERNAL STATE ---
local is_playing = false
local spawn_timer = 0.0
local active_circles = {} 
local feedback_texts = {} 
local click_was_down = false 
local my_entity_id = nil

-- [INGREDIENT STATE]
local has_ingredient = false
local ingredient_visual_id = -1

-- [POUNDING STATE]
local current_progress = 0 
local MAX_PROGRESS = 100
local POINTS_GOOD = 20
local POINTS_MEDIUM = 10
local POINTS_POOR = 5

-- [SPINNING STATE]
local is_spinning = false
local spin_progress = 0.0
local SPIN_REQUIREMENT = math.pi * 10 
local spinner_visual_id = nil
local last_mx, last_my = nil, nil
local spoon_x, spoon_y = 0.0, 100.0
local last_angle = 0.0

-- [FINISH STATE]
local is_finished = false
local finish_timer = 0.0
local FINISH_DELAY = 1.0 

-- --- DISPLAY CALIBRATION ---
local UI_WIDTH = 1920.0
local UI_HEIGHT = 1080.0       

-- --- SETTINGS ---
local CIRCLE_LIFESPAN = 3.0 
local MIN_X = 200
local MAX_X = 1720
local MIN_Y = 200
local MAX_Y = 880
local CLICK_RADIUS = 75 

function Mortar.on_init(self)
    my_entity_id = self.entity
    SetInteractionText(my_entity_id, "Place Ingredient")
    math.randomseed(os.time()) 
end

function Mortar.on_update(self, dt)
    if not is_playing then return end

    -- ==========================================
    -- PHASE 1: POUNDING
    -- ==========================================
    if not is_spinning then
        -- 1. SPAWN LOGIC
        spawn_timer = spawn_timer + dt
        if spawn_timer >= 1.0 then
            spawn_timer = spawn_timer - 1.0 

            local random_x = math.random(MIN_X, MAX_X)
            local random_y = math.random(MIN_Y, MAX_Y)
            
            local circle_id = SpawnPrefab(PREFAB_CIRCLE, random_x, random_y, 0.5)
            
            if circle_id ~= nil and circle_id ~= -1 then
                local default_sx, default_sy, default_sz = GetScale(circle_id)
                table.insert(active_circles, { 
                    id = circle_id, life = 0.0,
                    x = random_x, y = random_y, 
                    base_sx = default_sx, base_sy = default_sy, base_sz = default_sz
                })
            end
        end

        -- 2. CLICK DETECTION & PROGRESS LOGIC
        local is_clicked = IsMouseButtonDown(0)

        if is_clicked then
            if not click_was_down then
                click_was_down = true
                
                -- NEW: Use UV Coordinates for Resolution Independence!
                local uv_x, uv_y = GetMouseUV()
                
                -- Standardize to your UI Pixel Canvas
                local mouse_x = uv_x * UI_WIDTH
                local mouse_y = UI_HEIGHT - (uv_y * UI_HEIGHT) 
                
                for i = #active_circles, 1, -1 do
                    local circ = active_circles[i]
                    local dx = mouse_x - circ.x
                    local dy = mouse_y - circ.y
                    local distance = math.sqrt((dx * dx) + (dy * dy))
                    
                    if distance <= CLICK_RADIUS then
                        local popup_prefab = ""
                        local points_earned = 0
                        
                        if circ.life <= 1.0 then
                            popup_prefab = PREFAB_GOOD
                            points_earned = POINTS_GOOD
                        elseif circ.life <= 2.0 then
                            popup_prefab = PREFAB_MEDIUM
                            points_earned = POINTS_MEDIUM
                        else
                            popup_prefab = PREFAB_POOR
                            points_earned = POINTS_POOR
                        end
                        
                        current_progress = current_progress + points_earned
                        
                        local fb_id = SpawnPrefab(popup_prefab, mouse_x, mouse_y + 20, 0.6)
                        if fb_id ~= nil and fb_id ~= -1 then
                            local dsx, dsy, dsz = GetScale(fb_id)
                            table.insert(feedback_texts, { 
                                id = fb_id, life = 0.0, 
                                start_x = mouse_x, start_y = mouse_y + 20,
                                base_sx = dsx, base_sy = dsy, base_sz = dsz 
                            })
                        end
                        
                        DestroyEntity(circ.id)
                        table.remove(active_circles, i)
                        break 
                    end
                end
            end
        else
            click_was_down = false
        end

        -- 3. TRANSITION TO SPINNING PHASE
        if current_progress >= MAX_PROGRESS then
            is_spinning = true
            is_finished = false
            spin_progress = 0.0
            
            for i, circ in ipairs(active_circles) do DestroyEntity(circ.id) end
            active_circles = {}
            for i, fb in ipairs(feedback_texts) do DestroyEntity(fb.id) end
            feedback_texts = {}
            
            spinner_visual_id = SpawnPrefab(PREFAB_GOOD, UI_WIDTH / 2, UI_HEIGHT / 2, 0.5)
            if spinner_visual_id ~= nil and spinner_visual_id ~= -1 then
                local dsx, dsy, dsz = GetScale(spinner_visual_id)
                SetScale(spinner_visual_id, dsx * 2.0, dsy * 2.0, dsz * 2.0)
            end
            
            -- Set up initial mouse tracking for spinner
            local init_uv_x, init_uv_y = GetMouseUV()
            last_mx = init_uv_x * UI_WIDTH
            last_my = UI_HEIGHT - (init_uv_y * UI_HEIGHT)
            
            spoon_x = 0.0
            spoon_y = 100.0
            last_angle = math.atan2(spoon_y, spoon_x)
            return 
        end

        -- 4. CIRCLE SHRINK & DESPAWN
        for i = #active_circles, 1, -1 do
            local circ = active_circles[i]
            circ.life = circ.life + dt
            
            if circ.life >= CIRCLE_LIFESPAN then
                DestroyEntity(circ.id)
                table.remove(active_circles, i)
            else
                local scale_pct = 1.0 - (circ.life / CIRCLE_LIFESPAN)
                scale_pct = math.max(scale_pct, 0.05) 
                SetScale(circ.id, circ.base_sx * scale_pct, circ.base_sy * scale_pct, circ.base_sz * scale_pct)
            end
        end
        
        -- 5. POPUP FLOAT & FADE
        for i = #feedback_texts, 1, -1 do
            local fb = feedback_texts[i]
            fb.life = fb.life + dt
            
            if fb.life >= 1.0 then
                DestroyEntity(fb.id)
                table.remove(feedback_texts, i)
            else
                local float_y = fb.start_y + (fb.life * 100)
                SetPosition(fb.id, fb.start_x, float_y, 0.6)
                local scale_pct = 1.0 - fb.life
                scale_pct = math.max(scale_pct, 0.01) 
                SetScale(fb.id, fb.base_sx * scale_pct, fb.base_sy * scale_pct, fb.base_sz * scale_pct)
            end
        end

    -- ==========================================
    -- PHASE 2: SPINNING (Osu! Style)
    -- ==========================================
    else
        if is_finished then
            finish_timer = finish_timer - dt
            if finish_timer <= 0 then
                Mortar.StopMinigame(my_entity_id, true)
            end
        else
            -- NEW: Using UV coordinates for the spinning phase as well
            local uv_x, uv_y = GetMouseUV()
            local mx = uv_x * UI_WIDTH
            local my = UI_HEIGHT - (uv_y * UI_HEIGHT)
            
            if not last_mx then last_mx = mx; last_my = my end
            
            local dx = mx - last_mx
            local dy = my - last_my
            last_mx = mx
            last_my = my

            local is_held = IsMouseButtonDown(0)

            if is_held then
                spoon_x = spoon_x + dx
                spoon_y = spoon_y + dy
                
                local len = math.sqrt(spoon_x * spoon_x + spoon_y * spoon_y)
                if len > 0.1 then
                    spoon_x = (spoon_x / len) * 100.0
                    spoon_y = (spoon_y / len) * 100.0
                end
                
                local current_angle = math.atan2(spoon_y, spoon_x)
                local angle_delta = current_angle - last_angle
                local PI = 3.14159265359
                
                if angle_delta > PI then angle_delta = angle_delta - (2 * PI) end
                if angle_delta < -PI then angle_delta = angle_delta + (2 * PI) end
                last_angle = current_angle
                
                spin_progress = spin_progress + math.abs(angle_delta)
                
                if spinner_visual_id ~= nil and spinner_visual_id ~= -1 then
                    SetRotation(spinner_visual_id, 0.0, 0.0, math.deg(current_angle))
                end
                
                if spin_progress >= SPIN_REQUIREMENT then
                    is_finished = true
                    finish_timer = FINISH_DELAY
                end
            end
        end
    end
end

-- ==========================================
-- HELPER: STOP MINIGAME
-- ==========================================
function Mortar.StopMinigame(targetID, success)
    is_playing = false
    is_spinning = false
    is_finished = false
    
    SetUIFocused(false)
    SetCameraLock(false)
    
    for i, circ in ipairs(active_circles) do DestroyEntity(circ.id) end
    active_circles = {}
    
    for i, fb in ipairs(feedback_texts) do DestroyEntity(fb.id) end
    feedback_texts = {}
    
    if spinner_visual_id ~= nil and spinner_visual_id ~= -1 then
        DestroyEntity(spinner_visual_id)
        spinner_visual_id = nil
    end

    if success then
        has_ingredient = false 
        SetInteractionText(targetID, "Place Ingredient")
        
        if ingredient_visual_id ~= nil and ingredient_visual_id ~= -1 then
            DestroyEntity(ingredient_visual_id)
            ingredient_visual_id = -1
        end

        AddToInventory("chopped_herb") 
        Sound.Play("event:/RecipeComplete") 
    else
        SetInteractionText(targetID, "Start Minigame")
    end
end

-- ==========================================
-- INTERACTION LOGIC
-- ==========================================
function Mortar.OnInteract(senderID, targetID, actionType)
    if is_playing then 
        print("[Mortar] Minigame cancelled by player.")
        Mortar.StopMinigame(targetID, false)
        return 
    end
    
    if not has_ingredient then
        local held_item_id = GetSelectedItem()
        
        if held_item_id ~= -1 then
            ConsumeSelectedItem()
            has_ingredient = true
            
            if SpawnCauldronItem then
                ingredient_visual_id = SpawnCauldronItem(targetID, held_item_id)
            end
            
            Sound.Play("event:/PlaceDownItem")
            SetInteractionText(targetID, "Start Minigame")
        end
    else
        is_playing = true
        is_spinning = false
        is_finished = false
        spawn_timer = 1.0 
        current_progress = 0 
        click_was_down = false 
        
        SetInteractionText(targetID, "Stop Grinding") 
        SetUIFocused(true)
        SetCameraLock(true)
    end
end

return Mortar