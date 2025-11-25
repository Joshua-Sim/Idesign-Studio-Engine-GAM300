local M = {}

-- GLFW keycodes (same style as your other scripts)
local K = {
  G = 71,   -- press G to fake add points
}

local lastScore = nil
local firstUpdatePrinted = false

function M:on_init()
  print("[Scoreboard] on_init called")

  -- Check what self and entityId look like
  print("[Scoreboard] self = " .. tostring(self))
  print("[Scoreboard] self.entityId = " .. tostring(self and self.entity))

  lastScore = -1

  -- Try calling GetScore() safely
  local ok, s = pcall(GetScore)
  print("[Scoreboard] GetScore() ok = " .. tostring(ok) .. ", value = " .. tostring(s))

  if ok and self.entity then
    local okSet, errSet = pcall(SetText, self.entity, "Score: " .. tostring(s))
    if okSet then
      print("[Scoreboard] Initial text set for entity " .. tostring(self.entity))
    else
      print("[Scoreboard] ERROR: SetText failed in on_init: " .. tostring(errSet))
    end
  else
    print("[Scoreboard] WARNING: cannot set text in on_init (missing entityId or GetScore failed)")
  end
end

function M:on_update(dt)
  -- Print once on first update so we know on_update is being called
  if not firstUpdatePrinted then
    print("[Scoreboard] on_update first call, dt = " .. tostring(dt))
    print("[Scoreboard] self.entityId = " .. tostring(self and self.entity))
    firstUpdatePrinted = true
  end

  -- TEST BUTTON: press G to fake add points
  if IsKeyDown(K.G) then
    print("[Scoreboard] G pressed - calling AddScore(10)")
    local okAdd, errAdd = pcall(AddScore, 10)
    if not okAdd then
      print("[Scoreboard] ERROR: AddScore failed: " .. tostring(errAdd))
    end
  end

  -- Always try to read score from engine
  local ok, s = pcall(GetScore)
  if not ok then
    print("[Scoreboard] ERROR: GetScore failed: " .. tostring(s))
    return
  end

  -- Only update text if score changed
  if s ~= lastScore then
    print("[Scoreboard] Score changed: " .. tostring(lastScore) .. " -> " .. tostring(s))
    lastScore = s

    if self.entity then
      local okSet, errSet = pcall(SetText, self.entity, "Score: " .. tostring(s))
      if okSet then
        print("[Scoreboard] Text updated for entity " .. tostring(self.entity))
      else
        print("[Scoreboard] ERROR: SetText failed in on_update: " .. tostring(errSet))
      end
    else
      print("[Scoreboard] WARNING: self.entityId is nil, cannot update text")
    end
  end
end

return M
