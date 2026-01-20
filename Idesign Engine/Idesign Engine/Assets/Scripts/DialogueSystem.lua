--- dialogue_test_manual.lua
--- Press F8 to trigger dialogue

local DialogueTest = {}

DialogueTest.hasTriggered = false

function DialogueTest:on_init()
    print("[DialogueTest] Press F8 to start dialogue test")
end

function DialogueTest:on_update(dt)
    -- Check if F8 key is pressed (GLFW_KEY_F8 = 297)
    if IsKeyDown(297) and not self.hasTriggered then
        self:StartTestDialogue()
        self.hasTriggered = true
    end
    
    -- Reset trigger when key is released (for multiple tests)
    if not IsKeyDown(297) then
        self.hasTriggered = false
    end
end

function DialogueTest:StartTestDialogue()
    print("[DialogueTest] F8 pressed - Starting dialogue...")
    
    DialogueClear()
    
    DialogueAdd("Hello! Press SPACE to continue.", "NPC", "")
    DialogueAdd("This is a test of the dialogue system.", "NPC", "")
    DialogueAdd("Goodbye!", "NPC", "")
    
    DialogueStart()
end

return DialogueTest