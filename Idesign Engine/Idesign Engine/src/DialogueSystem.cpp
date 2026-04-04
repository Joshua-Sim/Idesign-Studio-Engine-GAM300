    ///////////////////////////////////////////////////////////////////////////////
    ///
    /// @file DialogueSystem.cpp
    /// @brief Implementation of dialogue display and progression system
    /// @authors: Joshua Sim
    /// @copyright 2025, Digipen Institute of Technology
    ///
    ///////////////////////////////////////////////////////////////////////////////

    #include "pch.h"
    #include "DialogueSystem.h"
    #include "Coordinator.h"
    #include "EngineState.h"
    #include "TextureLoader.h"
    #include "ComponentList.h"
    #include <iostream>
    #include "TextProperties.h"
    #include <GLFW/glfw3.h>  // For glfwGetTime()
    #include <sstream>

    extern Framework::Coordinator ecsInterface;

    namespace Framework {

        // Global AudioSystem pointer (replaces GlobalAudio)
        DialogueSystem* g_DialogueSystem = nullptr;

        DialogueSystem::DialogueSystem() {
            // Constructor
        }

        DialogueSystem::~DialogueSystem() {
            // Cleanup
            Clear();
        }

        void DialogueSystem::Initialize() {
            // Register with ECS (no component signature needed - utility system)
            Signature sig; // Empty signature - doesn't need to track entities
            ecsInterface.SetSystemSignature<DialogueSystem>(sig);

            std::cout << "Signature for DialogueSystem system is: " << sig << std::endl;
            std::cout << "[DialogueSystem] Initialized as System" << std::endl;

            isActive = false;

            // Clear any leftover state
            while (!dialogueQueue.empty()) {
                dialogueQueue.pop();
            }
		    g_DialogueSystem = this;
        }

        void DialogueSystem::Update(float deltaTime) {
            (void)deltaTime; // Ignore the passed parameter since it's 0
      
            // Calculate our own deltaTime
            static float lastTime = 0.0f;
            float currentTime = (float)glfwGetTime();

            if (lastTime == 0.0f) {
                lastTime = currentTime;
                return; // Skip first frame to initialize
            }

            float actualDeltaTime = currentTime - lastTime;
            lastTime = currentTime;

            // This runs every frame when dialogue is active
            if (!isActive) return;

            // --- Cutscene camera fly ---
            if (isCutscene && isMidFlight) {   // <-- was !isMidFlight, that was the bug
                flyToTarget(actualDeltaTime);
            }

            // Typewriter effect
            if (!fullyDisplayed) {
                textDisplayTimer += actualDeltaTime;  // Use calculated deltaTime

                // Reveal one character every 0.03 seconds (adjust speed here)
                if (textDisplayTimer >= typewriterSpeed) {
                    textDisplayTimer = 0.0f;

                    // Check if current line still has characters to reveal
                    if (currentLineIndex < fullTextLines.size()) {
                        if (currentCharIndex < fullTextLines[currentLineIndex].length()) {
                            // Add one more character to visible text
                            currentCharIndex++;

                            // Update the visible version of current line
                            visibleTextLines[currentLineIndex] =
                                fullTextLines[currentLineIndex].substr(0, currentCharIndex);

                            // Update UI
                            UpdateVisibleText();
                        }
                        else {
                            // Current line is done, move to next line
                            currentLineIndex++;
                            currentCharIndex = 0;

                            // Check if we're done with all lines
                            if (currentLineIndex >= fullTextLines.size()) {
                                fullyDisplayed = true;

                                // Stop typewriter sound 
                                if (g_AudioSystem && typewriterSoundPlaying) {
                                    g_AudioSystem->StopEvent("event:/TypeWriterSounds");
                                    typewriterSoundPlaying = false;
                                }
                            }
                            else {
                                // Initialize next line as empty
                                visibleTextLines[currentLineIndex] = "";
                            }

                            // Update UI after line transition
                            UpdateVisibleText();
                        }
                    }
                }
            }
        }

        void DialogueSystem::SetCameraTargetByTag(std::string targetTag) {
            auto entities = ecsInterface.GetEntitiesByTag(targetTag);
            if (entities.empty()) {
                std::cerr << "[DialogueSystem] No entity found with tag: " << targetTag << "\n";
                return; // safe early exit
            }
            Entity target = entities[0];
            auto& transform = ecsInterface.GetComponent<TransformComponent>(target);
            LookAtTargetPosition = transform.position;
        }

        void DialogueSystem::flyToTarget(float deltaTime) {
            Camera* cam = Graphics::GetInstance()->GetCameraByIndex(0);
            if (!cam) return;

            glm::vec3 destPos = LookAtTargetPosition + flyOffset;

            cam->position = glm::mix(cam->position, destPos, deltaTime * flySpeed);

            glm::vec3 dir = glm::normalize(LookAtTargetPosition - cam->position);

            float targetYaw = glm::degrees(atan2f(dir.z, dir.x));
            float targetPitch = glm::degrees(asinf(glm::clamp(dir.y, -1.0f, 1.0f)));


            float yawDiff = targetYaw - cam->yaw;

            // Wrap difference into [-180, 180] so we always take the short way
            while (yawDiff > 180.0f) yawDiff -= 360.0f;
            while (yawDiff < -180.0f) yawDiff += 360.0f;

            cam->yaw = cam->yaw + yawDiff * (deltaTime * flySpeed);
            cam->pitch = glm::mix(cam->pitch, targetPitch, deltaTime * flySpeed);

            cam->updateVectors();

            float dist = glm::length(cam->position - destPos);
            if (dist < arrivalThreshold) {
                cam->position = destPos;
                cam->yaw = targetYaw;
                cam->pitch = targetPitch;
                cam->updateVectors();
                isMidFlight = false;

                if (!isActive && dialogueQueue.empty()) {
                    engineState.PopState();
                }
            }
        }

        std::string DialogueSystem::GetName() {
            return "DialogueSystem";
        }

        void DialogueSystem::Clear() {
            // Only clear the QUEUE of pending dialogues
            while (!dialogueQueue.empty()) {
                dialogueQueue.pop();
            }

            std::cout << "[DialogueSystem] Cleared dialogue queue" << std::endl;
        }

        // Add a NEW function to clear EVERYTHING (for EndDialogue):
        void DialogueSystem::ClearAll() {
            while (!dialogueQueue.empty()) {
                dialogueQueue.pop();
            }
            fullTextLines.clear();
            visibleTextLines.clear();
            currentLineIndex = 0;
            currentCharIndex = 0;
            m_currentVoiceEvent.clear();
            std::cout << "[DialogueSystem] Cleared ALL dialogue data" << std::endl;
        }



        void DialogueSystem::AddLine(const std::string& text, const std::string& speaker,
            const std::string& portrait, const std::string& voiceEvent) {
            DialogueLine line(text, speaker, portrait, voiceEvent);
            dialogueQueue.push(line);
            std::cout << "[DialogueSystem] Added line: \"" << text << "\" by " << speaker << std::endl;
        }

        void DialogueSystem::StartDialogue() {
            std::cout << "[DialogueSystem] ===== StartDialogue() CALLED =====" << std::endl;

            if (dialogueQueue.empty()) {
                std::cerr << "[DialogueSystem] ERROR: Cannot start - queue is empty!" << std::endl;
                return;
            }

            std::cout << "[DialogueSystem] Queue has " << dialogueQueue.size() << " lines" << std::endl;

            // Validate scene has dialogue UI
            std::cout << "[DialogueSystem] Looking for DialogueLine1 entity..." << std::endl;
            Entity mainLine = FindDialogueEntity("DialogueLine1");

            if (!ecsInterface.IsEntityValid(mainLine)) {
                std::cerr << "[DialogueSystem] ERROR: Cannot start - scene missing 'DialogueLine1' entity!" << std::endl;
                std::cerr << "[DialogueSystem] Entity ID returned: " << mainLine << std::endl;
                std::cerr << "[DialogueSystem] This means the tag 'DialogueLine1' was not found!" << std::endl;
                Clear(); // Clean up queue
                return;  // <-- THIS IS PREVENTING isActive FROM BEING SET!
            }

            std::cout << "[DialogueSystem] Found DialogueLine1! Entity ID: " << mainLine << std::endl;
            std::cout << "[DialogueSystem] Starting dialogue with " << dialogueQueue.size() << " lines" << std::endl;

            std::cout << "[DialogueSystem] Setting isActive = true..." << std::endl;
            isActive = true;  // <-- THIS IS THE KEY LINE
            std::cout << "[DialogueSystem] isActive is now: " << (isActive ? "TRUE" : "FALSE") << std::endl;

            std::cout << "[DialogueSystem] Pushing Dialogue state..." << std::endl;
            engineState.PushState(EngineMode::Dialogue);

            std::cout << "[DialogueSystem] Making UI visible..." << std::endl;
            SetDialogueUIVisible(true);

            std::cout << "[DialogueSystem] Displaying first line..." << std::endl;
            DisplayCurrentLine();

            std::cout << "[DialogueSystem] ===== StartDialogue() COMPLETE =====" << std::endl;
        }

        void DialogueSystem::Advance() {
            if (!isActive) return;

            // If text is still being typed, skip to end
            if (!fullyDisplayed) {
                fullyDisplayed = true;

				//Stop typewriter sound effect immediately

                if (g_AudioSystem && typewriterSoundPlaying) {
                    g_AudioSystem->StopEvent("event:/TypeWriterSounds");
                    typewriterSoundPlaying = false;
                }


                // Instantly show all remaining text
                for (size_t i = 0; i < fullTextLines.size(); i++) {
                    visibleTextLines[i] = fullTextLines[i];
                }

                UpdateVisibleText();
                return;
            }

            // Text is fully displayed, move to next dialogue
            if (!dialogueQueue.empty()) {
                dialogueQueue.pop(); // Remove current line
            }

            if (dialogueQueue.empty()) {
                // No more lines
                EndDialogue();
            }
            else {
                // Show next line
                DisplayCurrentLine();
            }
        }

        void DialogueSystem::EndDialogue() {
            std::cout << "[DialogueSystem] Ending dialogue" << std::endl;

            //  STOP TYPEWRITER SOUND 
            if (g_AudioSystem && typewriterSoundPlaying) {
                g_AudioSystem->StopEvent("event:/TypeWriterSounds");
                typewriterSoundPlaying = false;
            }

            // Stop voice over
            if (g_AudioSystem && !m_currentVoiceEvent.empty()) {
                g_AudioSystem->StopEvent(m_currentVoiceEvent);
                m_currentVoiceEvent.clear();
            }

            isActive = false;
            engineState.PopState();
            SetDialogueUIVisible(false);
            ClearAll();  // Use the new function that clears everything
        }

        Entity DialogueSystem::FindDialogueEntity(const std::string& tag) {
            auto entities = ecsInterface.GetEntitiesByTag(tag);
            if (entities.empty()) {
                return std::numeric_limits<Entity>::max();
            }
            return entities[0];
        }

        std::vector<std::string> DialogueSystem::SplitTextIntoLines(const std::string& text, size_t maxCharsPerLine) {
            std::vector<std::string> lines;
            std::istringstream words(text);
            std::string word;
            std::string currentLine;

            while (words >> word) {
                // Check if adding this word would exceed the limit
                if (currentLine.length() + word.length() + 1 > maxCharsPerLine) {
                    // Save current line and start new one
                    if (!currentLine.empty()) {
                        lines.push_back(currentLine);
                        currentLine.clear();
                    }
                }

                // Add word to current line
                if (!currentLine.empty()) {
                    currentLine += " ";
                }
                currentLine += word;
            }

            // Add the last line
            if (!currentLine.empty()) {
                lines.push_back(currentLine);
            }

            return lines;
        }

        void DialogueSystem::UpdateVisibleText() {
            // Find UI entities by tag
            Entity textEntity1 = FindDialogueEntity("DialogueLine1");
            Entity textEntity2 = FindDialogueEntity("DialogueLine2");
            Entity textEntity3 = FindDialogueEntity("DialogueLine3");

            std::vector<Entity> lineEntities = { textEntity1, textEntity2, textEntity3 };

            std::cout << "[Dialogue] UpdateVisibleText called - Setting text for "
                << visibleTextLines.size() << " lines" << std::endl;

            // Update each line with whatever is in visibleTextLines
            for (size_t i = 0; i < lineEntities.size(); i++) {
                Entity e = lineEntities[i];

                if (!ecsInterface.IsEntityValid(e)) {
                    std::cout << "[Dialogue] WARNING: Line " << i << " entity is INVALID!" << std::endl;
                    continue;
                }

                if (!ecsInterface.HasComponent<TextProperties>(e)) {
                    std::cout << "[Dialogue] WARNING: Line " << i << " has no TextProperties!" << std::endl;
                    continue;
                }

                auto& text = ecsInterface.GetComponent<TextProperties>(e);

                // Set text to whatever we've revealed so far (or empty if not reached yet)
                if (i < visibleTextLines.size()) {
                    text.text = visibleTextLines[i];
                    std::cout << "[Dialogue] Set Line " << i << " to: \"" << text.text << "\"" << std::endl;
                }
                else {
                    text.text = "";
                    std::cout << "[Dialogue] Set Line " << i << " to: (empty)" << std::endl;
                }
            }
        }

        void DialogueSystem::DisplayCurrentLine() {
            if (dialogueQueue.empty()) {
                EndDialogue();
                return;
            }

            auto& line = dialogueQueue.front();

            std::cout << "[Dialogue] DisplayCurrentLine - Raw text: \"" << line.text << "\"" << std::endl;

            // Split text into lines (~30 chars per line)
            fullTextLines = SplitTextIntoLines(line.text, maxCharsPerLine);

            std::cout << "[Dialogue] Split into " << fullTextLines.size() << " lines:" << std::endl;
            for (size_t i = 0; i < fullTextLines.size(); i++) {
                std::cout << "[Dialogue]   Line " << i << ": \"" << fullTextLines[i] << "\"" << std::endl;
            }

            // Initialize visible text (all empty, we'll fill them character by character)
            visibleTextLines.clear();
            visibleTextLines.resize(fullTextLines.size(), "");

            std::cout << "[Dialogue] Created " << visibleTextLines.size() << " empty visible lines" << std::endl;

            // Start at first line, first character
            currentLineIndex = 0;
            currentCharIndex = 0;
            fullyDisplayed = false;
            textDisplayTimer = 0.0f;

            // Start typewriter sound effect 
            if (g_AudioSystem && !typewriterSoundPlaying) {
                g_AudioSystem->PlayAudio("event:/TypeWriterSounds");
                typewriterSoundPlaying = true;
            }

            // Stop previous VO and play new one
            if (g_AudioSystem) {
                if (!m_currentVoiceEvent.empty()) {
                    g_AudioSystem->StopEvent(m_currentVoiceEvent);
                }
                if (!line.voiceEvent.empty()) {
                    g_AudioSystem->PlayAudio(line.voiceEvent);
                    m_currentVoiceEvent = line.voiceEvent;
                }
            }

            // Update speaker name
            Entity textSpeaker = FindDialogueEntity("Name_text");
            if (ecsInterface.IsEntityValid(textSpeaker) &&
                ecsInterface.HasComponent<TextProperties>(textSpeaker)) {
                auto& text = ecsInterface.GetComponent<TextProperties>(textSpeaker);
                text.text = line.speakerName;
                std::cout << "[Dialogue] Set speaker name to: \"" << line.speakerName << "\"" << std::endl;
            }

            // Update portrait texture (if provided)
            if (!line.portraitPath.empty()) {
                Entity portraitEntity = FindDialogueEntity("Portrait");
                if (ecsInterface.IsEntityValid(portraitEntity) &&
                    ecsInterface.HasComponent<TextureProperties>(portraitEntity)) {
                    auto& tex = ecsInterface.GetComponent<TextureProperties>(portraitEntity);
                    tex.path = line.portraitPath;
                    TextureLoader::loadTexture(tex, true);
                    std::cout << "[Dialogue] Loaded portrait: " << line.portraitPath << std::endl;
                }
            }

            // Clear all text lines initially
            std::cout << "[Dialogue] Calling initial UpdateVisibleText..." << std::endl;
            UpdateVisibleText();

            std::cout << "[Dialogue] Started dialogue by " << line.speakerName
                << " (" << fullTextLines.size() << " lines). Typewriter ready!" << std::endl;
        }

        void DialogueSystem::SetDialogueUIVisible(bool visible) {
            // Find all dialogue UI elements and toggle their active state
            std::vector<std::string> dialogueTags = {
                "DialogueBox",
                "DialogueLine1",
                "DialogueLine2",
                "DialogueLine3",
                "Name_text",
                "Portrait",
                "PortraitBox"
            };

            for (const auto& tag : dialogueTags) {
                Entity e = FindDialogueEntity(tag);
                if (ecsInterface.IsEntityValid(e)) {
                    if (ecsInterface.HasComponent<TransformComponent>(e)) {
                        auto& transform = ecsInterface.GetComponent<TransformComponent>(e);
                        transform.isActive = visible;
                    }
                    std::cout << "[DialogueSystem] Set " << tag << " visible: " << visible << std::endl;
                }
            }
        }

    } // namespace Framework