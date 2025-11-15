-- Pathfinding lua script, this script calls pathfinds function for an entity via nav-mesh to target entity via their name

local M = {}

local didPath = false  -- local flag to ensure we only run once

function M:on_init()
    print("[Lua] Pathfinding test on_init")
end

function M:on_update(dt)
    if not didPath then
        PathfindTo("Frog", "Target")
        print("[Lua] Pathfinding triggered once.")
        didPath = true
    end
end

return M
