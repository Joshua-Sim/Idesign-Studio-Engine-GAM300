--- night_tutorial_sequenced.lua
--- Uses Engine-side persistence to prevent re-plays on soft resets

local NightTutorial = {}

-- We keep these for the internal state of the active sequence
NightTutorial.waitingForLanding = false
NightTutorial.waitingForDialogue = false
NightTutorial.timer = 0.0
NightTutorial.delayDuration = 1.35 
NightTutorial.sequenceStep = 1

function NightTutorial:on_init()
    print("[NightTutorial] Night sequence initialized. Checking persistent flags...")
end

function NightTutorial:on_update(dt)
    local inGameplay = IsEngineState("Play")
    local isNightPhase = (GetPhase() == 2) 

    -- 1. Persistent Check: Refer to the C++ map instead of a local bool
    -- GetData returns 0 if "nightTutorialPlayed" doesn't exist yet.
    local playedFlag = GetData("nightTutorialPlayed")

    if inGameplay and isNightPhase and playedFlag == 0 then
        self.timer = self.timer + dt
        if self.timer >= self.delayDuration then
            -- 2. Lock it in the Engine Map immediately so a reset can't catch it
            SetData("nightTutorialPlayed", 1)
            
            print("[NightTutorial] First time night detected. Starting sequence.")
            self:PlayNextStep()
        end
    end

    -- 3. Sequence Logic (Camera & Dialogue)
    if self.waitingForLanding and not CutsceneIsFlying() then
        self.waitingForLanding = false
        self:StartDialogueForCurrentStep()
    end

    if self.waitingForDialogue and not DialogueIsActive() then
        self.waitingForDialogue = false
        self.sequenceStep = self.sequenceStep + 1
        
        if self.sequenceStep <= 3 then
            self:PlayNextStep()
        else
            print("[NightTutorial] Night sequence complete. control restored.")
        end
    end
end

function NightTutorial:PlayNextStep()
    local target = ""
    local offX, offY, offZ = 0, 0, 0
    
    if self.sequenceStep == 1 then
        target = "Frog"
        offX, offY, offZ = 0, 2, 4 
    elseif self.sequenceStep == 2 then
        target = "Cutting_Board"
        offX, offY, offZ = 2.0, 4.0, 4.0 
    elseif self.sequenceStep == 3 then
        target = "Cauldron"
        offX, offY, offZ = -2.0, 4.5, -4.0 
    end

    print("[NightTutorial] Flying to: " .. target)
    CutsceneSetTarget(target)
    CutsceneSetOffset(offX, offY, offZ)
    CutsceneSetSpeed(2.5)
    CutsceneStart()

    self.waitingForLanding = true
end

function NightTutorial:StartDialogueForCurrentStep()
    DialogueClear()
    if self.sequenceStep == 1 then
        DialogueAdd("Look alive, child! The moon is high; 'tis the Night Phase now.", "Aunt Mira", "pear")
        DialogueAdd("Don't just stand, customers awaits!", "Aunt Mira", "portrait")
    elseif self.sequenceStep == 2 then
        DialogueAdd("First, preparation! Use the knife!", "Aunt Mira", "pear")
        DialogueAdd("Hold the knife and click the mouse in time to dice em up!", "Aunt Mira", "pear")
    elseif self.sequenceStep == 3 then
        DialogueAdd("Then, the cauldron. Toss your ingredients in and stir!", "Aunt Mira", "pear")
        DialogueAdd("Press 'R' if you've forgotten your potions and need the Recipe Book.", "Aunt Mira", "portrait")
    end
    DialogueStart()
    self.waitingForDialogue = true
end

return NightTutorial