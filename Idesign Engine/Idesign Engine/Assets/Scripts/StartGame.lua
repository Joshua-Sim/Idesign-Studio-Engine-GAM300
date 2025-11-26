local M = {}

function M:on_init()
    RegisterButtonCallback(self.entity, function(ent)
        print("[Lua] Button for entity", ent, "clicked loading GameScene.json")
        LoadScene("Scenes/GameScene.json")  -- calls AssetCompiler-backed binding
    end)
end

return M
