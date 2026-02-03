///////////////////////////////////////////////////////////////////////////////
///
///@file Enginestate.h
/// 
/// @brief This file contains the definition of the Entity State. It handles state flags
///       and is utilized to manage engine state between editor and simulation / debug.
///  
/// @Authors: Joshua Sim
/// Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#include "pch.h"

namespace Framework
{
    // Current enum represents each logical mode of the engine/UI
    enum EngineMode
    {
        Editor,
        Play,
        Paused,
        Debug,
        Menu,
        HowToPlay,
        Upgrade,
        Settings,
        Dialogue
    };

    class EngineState
    {
    public:

        // Adds a new active state on top of the stack
        void PushState(EngineMode state)
        {
            if (!stateStack.empty() && stateStack.back() == state) return;
            stateStack.push_back(state);
        }

        // Remove the current state, returning to the prev one
        void PopState()
        {
            if (stateStack.size() > 1) stateStack.pop_back();
        }

        // Hard Reset, remove all UI/nav states and return to gameplay/editor
        void ClearToBase()
        {
            while (stateStack.size() > 1) stateStack.pop_back();
        }

        // Returns the current active state aka top of stack
        EngineMode Current() const
        {
            return stateStack.back();
        }

        // Check for current state at the top
        bool Is(EngineMode state) const
        {
            return Current() == state;
        }

        // Check if state is within stack
        bool HasState(EngineMode state) const
        {
            for (EngineMode s : stateStack)
                if (s == state) return true;
            return false;
        }

        // Check if any MENU/UI overlay is open
        bool IsAnyMenuOpen() const
        {
            return Is(EngineMode::Menu)      ||
                   Is(EngineMode::Paused)    ||
                   Is(EngineMode::HowToPlay) ||
                   Is(EngineMode::Settings)  ||
                   Is(EngineMode::Dialogue)  ||
                   Is(EngineMode::Upgrade);
        }

        // Check if current scene is actively in gameplay (Play Mode, with no overlays)
        bool IsInGameplay() const
        {
            return Is(EngineMode::Play) && !IsAnyMenuOpen();
        }

    private:
        std::vector<EngineMode> stateStack{ EngineMode::Editor };
    };
    extern EngineState engineState;
}