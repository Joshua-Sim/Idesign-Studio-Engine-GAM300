--- dialogue_test_manual.lua
--- Press F8 to trigger cutscene + dialogue test

local DialogueTest = {}

DialogueTest.hasTriggered = false
DialogueTest.waitingForLanding = false

function DialogueTest:on_init()
    print("[DialogueTest] Press F8 to start cutscene dialogue test")
end

function DialogueTest:on_update(dt)
    -- Trigger on F8
    if IsKeyDown(297) and not self.hasTriggered then
        self:StartCutscene()
        self.hasTriggered = true
    end

    if not IsKeyDown(297) then
        self.hasTriggered = false
    end

    -- Poll for camera arrival, then fire dialogue
    if self.waitingForLanding and not CutsceneIsFlying() then
        self.waitingForLanding = false
        self:StartDialogueAfterFly()
    end
end

function DialogueTest:StartCutscene()
    print("[DialogueTest] Starting cutscene fly...")

    CutsceneSetTarget("exampletarget")
    CutsceneSetOffset(0, 0, 6)
    CutsceneSetSpeed(2.5)
    CutsceneStart()

    self.waitingForLanding = true
end

function DialogueTest:StartDialogueAfterFly()
    print("[DialogueTest] Camera landed - starting dialogue...")

    DialogueClear()
    DialogueAdd("Collect ingredients from all over the area!", "Aunt Mira", "")
    DialogueAdd("You have but few time, hurry now child!", "Aunt Mira", "")
    DialogueStart()
end

return DialogueTest