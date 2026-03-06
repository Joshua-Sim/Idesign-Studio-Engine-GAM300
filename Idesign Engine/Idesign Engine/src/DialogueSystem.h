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
#include <vector>

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
        void ClearAll();

        // Configuration
        int maxCharsPerLine = 50;  // Default to 40 characters
        float typewriterSpeed = 0.03f;  // Seconds per character

        // State queries
        bool IsActive() const { return isActive; }
        bool HasLines() const { return !dialogueQueue.empty(); }

    private:
        // Internal helpers
        Entity FindDialogueEntity(const std::string& tag);
        void DisplayCurrentLine();
        void SetDialogueUIVisible(bool visible);
        std::vector<std::string> SplitTextIntoLines(const std::string& text, size_t maxCharsPerLine);
        void UpdateVisibleText();

        // State
        std::queue<DialogueLine> dialogueQueue;
        bool isActive = false;

        // Typewriter effect state
        std::vector<std::string> fullTextLines;     // Complete text for each line (cached)
        std::vector<std::string> visibleTextLines;  // What's currently visible (grows each frame)
        size_t currentLineIndex = 0;                // Which line we're currently typing
        size_t currentCharIndex = 0;                // Character position in current line
        float textDisplayTimer = 0.0f;
        bool fullyDisplayed = true;
    };
} // namespace Framework