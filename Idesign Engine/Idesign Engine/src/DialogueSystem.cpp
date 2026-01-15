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
#include "TextProperties.h"
#include <iostream>

extern Framework::Coordinator ecsInterface;

namespace Framework {

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
    }

    void DialogueSystem::Update(float deltaTime) {
        // This runs every frame when dialogue is active
        if (!isActive) return;

        // Optional: Add typewriter effect timing here
        // textDisplayTimer += deltaTime;
        // if (textDisplayTimer > 0.05f) {
        //     textDisplayTimer = 0.0f;
        //     // Reveal one more character
        // }

        // Optional: Add auto-advance timing
        // if (fullyDisplayed && autoAdvanceDelay > 0) {
        //     autoAdvanceTimer += deltaTime;
        //     if (autoAdvanceTimer >= autoAdvanceDelay) {
        //         Advance();
        //     }
        // }
    }

    std::string DialogueSystem::GetName() {
        return "DialogueSystem";
    }

    void DialogueSystem::Clear() {
        // Empty the queue
        while (!dialogueQueue.empty()) {
            dialogueQueue.pop();
        }
        std::cout << "[DialogueSystem] Cleared dialogue queue" << std::endl;
    }

    void DialogueSystem::AddLine(const std::string& text, const std::string& speaker, const std::string& portrait) {
        DialogueLine line(text, speaker, portrait);
        dialogueQueue.push(line);
        std::cout << "[DialogueSystem] Added line: \"" << text << "\" by " << speaker << std::endl;
    }

    void DialogueSystem::StartDialogue() {
        if (dialogueQueue.empty()) {
            std::cerr << "[DialogueSystem] Cannot start - queue is empty!" << std::endl;
            return;
        }

        // Validate scene has dialogue UI
        Entity mainLine = FindDialogueEntity("DialogueLine_1");
        if (!ecsInterface.IsEntityValid(mainLine)) {
            std::cerr << "[DialogueSystem] Cannot start - scene missing 'DialogueLine_1' entity!" << std::endl;
            Clear(); // Clean up queue
            return;
        }

        std::cout << "[DialogueSystem] Starting dialogue with " << dialogueQueue.size() << " lines" << std::endl;

        isActive = true;
        engineState.SetDialogue(true);

        SetDialogueUIVisible(true);
        DisplayCurrentLine();
    }

    void DialogueSystem::Advance() {
        if (!isActive) return;

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

        isActive = false;
        engineState.SetDialogue(false);

        SetDialogueUIVisible(false);

        // Clean up any remaining lines
        Clear();
    }

    Entity DialogueSystem::FindDialogueEntity(const std::string& tag) {
        auto entities = ecsInterface.GetEntitiesByTag(tag);
        if (entities.empty()) {
            return std::numeric_limits<Entity>::max();
        }
        return entities[0];
    }

    void DialogueSystem::DisplayCurrentLine() {
        if (dialogueQueue.empty()) {
            EndDialogue();
            return;
        }

        auto& line = dialogueQueue.front();

        // Find UI entities by tag
        Entity textEntity = FindDialogueEntity("DialogueLine_1");
        Entity speakerEntity = FindDialogueEntity("DialogueSpeaker");
        Entity portraitEntity = FindDialogueEntity("DialoguePortrait");

        // Update main dialogue text
        if (ecsInterface.IsEntityValid(textEntity) &&
            ecsInterface.HasComponent<TextProperties>(textEntity)) {
            auto& text = ecsInterface.GetComponent<TextProperties>(textEntity);
            text.text = line.text;
            std::cout << "[DialogueSystem] Displayed: \"" << line.text << "\"" << std::endl;
        }
        else {
            std::cerr << "[DialogueSystem] Warning: DialogueLine_1 entity missing or no TextProperties!" << std::endl;
        }

        // Update speaker name
        if (ecsInterface.IsEntityValid(speakerEntity) &&
            ecsInterface.HasComponent<TextProperties>(speakerEntity)) {
            auto& text = ecsInterface.GetComponent<TextProperties>(speakerEntity);
            text.text = line.speakerName;
        }

        // Update portrait texture
        if (!line.portraitPath.empty() &&
            ecsInterface.IsEntityValid(portraitEntity) &&
            ecsInterface.HasComponent<TextureProperties>(portraitEntity)) {
            auto& tex = ecsInterface.GetComponent<TextureProperties>(portraitEntity);
            tex.path = line.portraitPath;
            TextureLoader::loadTexture(tex, true);
        }

        // Reset typewriter state if you implement it
        textDisplayTimer = 0.0f;
        fullyDisplayed = true;
    }

    void DialogueSystem::SetDialogueUIVisible(bool visible) {
        // Find all dialogue UI elements and toggle their active state
        std::vector<std::string> dialogueTags = {
            "DialogueBox",
            "DialogueLine_1",
            "DialogueSpeaker",
            "DialoguePortrait"
        };

        for (const auto& tag : dialogueTags) {
            Entity e = FindDialogueEntity(tag);
            if (ecsInterface.IsEntityValid(e)) {
                // TODO: Implement your active/inactive toggle mechanism
                // If you have an IsActive component:
                // if (ecsInterface.HasComponent<IsActiveComponent>(e)) {
                //     auto& active = ecsInterface.GetComponent<IsActiveComponent>(e);
                //     active.isActive = visible;
                // }

                std::cout << "[DialogueSystem] Set " << tag << " visible: " << visible << std::endl;
            }
        }
    }

} // namespace Framework