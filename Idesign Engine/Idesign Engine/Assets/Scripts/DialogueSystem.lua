--- day_tutorial_sequenced.lua
--- Sequence: Sheep -> Pear -> Herb -> Gate
--- Uses Engine-side persistence to prevent re-plays

local DayTutorial = {}
SetData("dayTutorialPlayed", 0)
SetData("mapSelectionTutorialPlayed", 0)

DayTutorial.waitingForLanding = false
DayTutorial.waitingForDialogue = false
DayTutorial.timer = 0.0
DayTutorial.delayDuration = 1.35 
DayTutorial.sequenceStep = 1
DayTutorial.skipKeyHeld = false

DayTutorial.mapSelWaitingForDialogue = false
DayTutorial.mapSelTimer = 0.0
DayTutorial.mapSelDelayDuration = 1.0

function DayTutorial:on_init()
    print("[DayTutorial] Sequenced Script Initialized. Checking persistent flags...")
end

function DayTutorial:SkipSequence()
    print("[DayTutorial] Sequence skipped via F9.")

    SetData("dayTutorialPlayed", 1)

    if CutsceneIsFlying() then
        CutsceneStop()
    end
    if DialogueIsActive() then
        DialogueClear()
    end

    self.waitingForLanding = false
    self.waitingForDialogue = false
    self.sequenceStep = 5
    self.timer = 0.0

    print("[DayTutorial] Full sequence complete. Player control restored.")
end

function DayTutorial:SkipMapSelSequence()
    print("[DayTutorial] MapSelection sequence skipped via F9.")

    SetData("mapSelectionTutorialPlayed", 1)

    if DialogueIsActive() then
        DialogueClear()
    end

    self.mapSelWaitingForDialogue = false
    self.mapSelTimer = 0.0

    print("[DayTutorial] MapSelection sequence complete.")
end

function DayTutorial:on_update(dt)
    local inGameplay = IsEngineState("Play")
    local inMapSelection = IsEngineState("MapSelection")
    local isDayPhase = (GetPhase() == 1)
    local playedFlag = GetData("dayTutorialPlayed")
    local mapSelPlayedFlag = GetData("mapSelectionTutorialPlayed")

    -- F9 skip with debounce: only fires once per key press
    local skipPressed = IsKeyDown(298)
    if skipPressed and not self.skipKeyHeld then
        if inGameplay and isDayPhase and (playedFlag == 0 or self.waitingForLanding or self.waitingForDialogue) then
            self.skipKeyHeld = true
            self:SkipSequence()
            return
        end
        if inMapSelection and (mapSelPlayedFlag == 0 or self.mapSelWaitingForDialogue) then
            self.skipKeyHeld = true
            self:SkipMapSelSequence()
            return
        end
    end
    if not skipPressed then
        self.skipKeyHeld = false
    end

    -- Day tutorial
    if inGameplay and isDayPhase and playedFlag == 0 then
        self.timer = self.timer + dt
        if self.timer >= self.delayDuration then
            SetData("dayTutorialPlayed", 1)
            print("[DayTutorial] First time day detected. Starting sequence.")
            self:PlayNextStep()
        end
    end

    if self.waitingForLanding and not CutsceneIsFlying() then
        self.waitingForLanding = false
        self:StartDialogueForCurrentStep()
    end

    if self.waitingForDialogue and not DialogueIsActive() then
        self.waitingForDialogue = false
        self.sequenceStep = self.sequenceStep + 1
        
        if self.sequenceStep <= 4 then
            self:PlayNextStep()
        else
            print("[DayTutorial] Full sequence complete. Player control restored.")
        end
    end

    -- MapSelection tutorial
    if inMapSelection and mapSelPlayedFlag == 0 then
        self.mapSelTimer = self.mapSelTimer + dt
        if self.mapSelTimer >= self.mapSelDelayDuration then
            SetData("mapSelectionTutorialPlayed", 1)
            print("[DayTutorial] First time MapSelection detected. Starting dialogue.")
            self:PlayMapSelDialogue()
        end
    end

    if self.mapSelWaitingForDialogue and not DialogueIsActive() then
        self.mapSelWaitingForDialogue = false
        print("[DayTutorial] MapSelection dialogue complete.")
    end
end

function DayTutorial:PlayNextStep()
    local target = ""
    local offX, offY, offZ = 0, 5, 8
    
    if self.sequenceStep == 1 then
        target = "Sheep"
    elseif self.sequenceStep == 2 then
        target = "Pear"
        offX, offY, offZ = 0, 3, 5
    elseif self.sequenceStep == 3 then
        target = "Herb"
    elseif self.sequenceStep == 4 then
        target = "Gate"
        offX, offY, offZ = 25, 6, 12
    end

    print("[DayTutorial] Stage " .. self.sequenceStep .. " | Flying to: " .. target)

    CutsceneSetTarget(target)
    CutsceneSetOffset(offX, offY, offZ)
    CutsceneSetSpeed(2.5)
    CutsceneStart()

    self.waitingForLanding = true
end

function DayTutorial:StartDialogueForCurrentStep()
    DialogueClear()

    if self.sequenceStep == 1 then
        DialogueAdd("Look at these fluffy sheep! They aren't just for show.", "Aunt Mira", "pear")
        DialogueAdd("Gather their wool, it may prove useful in certain recipes!", "Aunt Mira", "portrait")
    
    elseif self.sequenceStep == 2 then
        DialogueAdd("Ah, a fine pear! Some recipes require more than just roots.", "Aunt Mira", "pear")
        DialogueAdd("Keep your eyes peeled for fruit hanging in the brush.", "Aunt Mira", "portrait")

    elseif self.sequenceStep == 3 then
        DialogueAdd("These are the herbs you need for your remedies.", "Aunt Mira", "pear")
        DialogueAdd("Gather them by pressing 'E' until your bag is heavy.", "Aunt Mira", "portrait")

    elseif self.sequenceStep == 4 then
        DialogueAdd("Finally, the Gate. Use it once you are finished here.", "Aunt Mira", "pear")
        DialogueAdd("The moon waits for no one, child. Now get to work!", "Aunt Mira", "portrait")
    end

    DialogueStart()
    self.waitingForDialogue = true
end

function DayTutorial:PlayMapSelDialogue()
    DialogueClear()

    -- TODO: replace with real map selection tutorial lines
    DialogueAdd("Testing! This is the map selection screen.", "Aunt Mira", "portrait")
    DialogueAdd("Pick a location to begin your journey.", "Aunt Mira", "portrait")

    DialogueStart()
    self.mapSelWaitingForDialogue = true
end

return DayTutorial