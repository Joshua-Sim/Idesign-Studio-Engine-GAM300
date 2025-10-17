local M = {}

function M:on_init()
  print("[Lua] hello.lua on_init")
end

function M:on_update(dt)
  -- print a heartbeat to prove it's running every frame
  -- comment this out if it’s too spammy
  -- print(string.format("[Lua] on_update dt=%.4f", dt))
end

function M:on_event(name)
  print("What's Up My Niglets " .. tostring(name))
end

return M
