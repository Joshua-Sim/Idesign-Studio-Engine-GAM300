--- dialogue_test_manual.lua
--- Press T to trigger dialogue

local DialogueTest = {}

DialogueTest.hasTriggered = false

function DialogueTest:on_init()
    print("[DialogueTest] Press T to start dialogue test")
end

function DialogueTest:on_update(dt)
    -- Check if T key is pressed (GLFW_KEY_T = 84)
    if IsKeyDown(84) and not self.hasTriggered then
        self:StartTestDialogue()
        self.hasTriggered = true
    end
    
    -- Reset trigger when key is released (for multiple tests)
    if not IsKeyDown(84) then
        self.hasTriggered = false
    end
end

function DialogueTest:StartTestDialogue()
    print("[DialogueTest] T pressed - Starting dialogue...")
    
    DialogueClear()
    
    DialogueAdd("Hello! Press SPACE to continue.", "NPC", "")
    DialogueAdd("This is a test of the dialogue system.", "NPC", "")
    DialogueAdd("Goodbye!", "NPC", "")
    
    DialogueStart()
end

return DialogueTest