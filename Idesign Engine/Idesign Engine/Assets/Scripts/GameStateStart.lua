local M = {}

function M:on_init()
    print("[MenuScript] on_init called for entity:", self.entity)

    RegisterButtonCallback(self.entity, function(ent)
        print("[MenuScript] Button clicked! Entity:", ent)

        -- Start the game state
        -- ATM only got audio
        GameStateStart()
    end)
end
return M