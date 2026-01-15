///////////////////////////////////////////////////////////////////////////////
///
/// @file DialogueSystem.h
/// @brief System to handle dialogue display and progression
/// @authors: Joshua Sim
/// @copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#include "pch.h"
#include "System.h"
#include <queue>
#include <string>

namespace Framework {

    struct DialogueLine {
        std::string text;
        std::string speakerName;
        std::string portraitPath;

        DialogueLine() = default;
        DialogueLine(const std::string& txt, const std::string& spk, const std::string& port)
            : text(txt), speakerName(spk), portraitPath(port) {
        }
    };

    class DialogueSystem : public ISystem {
    public:
        DialogueSystem();
        ~DialogueSystem();

        // Singleton pattern (for Lua bindings)
        static DialogueSystem& Instance() {
            static DialogueSystem instance;
            return instance;
        }

        // Delete copy/move constructors
        DialogueSystem(const DialogueSystem&) = delete;
        DialogueSystem& operator=(const DialogueSystem&) = delete;

        // ISystem interface
        void Initialize() override;
        void Update(float deltaTime) override;
        std::string GetName() override;

        // Core API
        void Clear();
        void AddLine(const std::string& text, const std::string& speaker = "", const std::string& portrait = "");
        void StartDialogue();
        void Advance();
        void EndDialogue();

        // State queries
        bool IsActive() const { return isActive; }
        bool HasLines() const { return !dialogueQueue.empty(); }

    private:
        // Internal helpers
        Entity FindDialogueEntity(const std::string& tag);
        void DisplayCurrentLine();
        void SetDialogueUIVisible(bool visible);

        // State
        std::queue<DialogueLine> dialogueQueue;
        bool isActive = false;

        // Optional: Add text scrolling/typewriter effect state
        float textDisplayTimer = 0.0f;
        bool fullyDisplayed = true;
    };

} // namespace Framework