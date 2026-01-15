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

#include "pch.h"

namespace Framework {

#pragma once

    class EngineState {
    public:

        // Accessors and mutators for flags
        bool IsInEditor() const { return inEditor; }
        void SetInEditor(bool value) { inEditor = value; }

        bool IsInDebugMode() const { return inDebugMode; }
        void SetInDebugMode(bool value) { inDebugMode = value; }

        bool IsPaused() const { return isPaused; }
        void SetPaused(bool value) { isPaused = value; }

        bool IsPlay() const { return IsPlayMode; }
        void SetPlay(bool value) { IsPlayMode = value; }

        // Scenes
        bool IsInMenu() const { return mInMenu; }
        void SetMenu(bool value) { mInMenu = value; }

        bool InHowToPlay() const { return mInHowToPlay; }
        void SetHowToPlay(bool value) { mInHowToPlay = value; }

        bool IsInUpgrade() const { return mInUpgrade; }
        void SetUpgrade(bool value) { mInUpgrade = value; }

        // Dialogue toggles
        bool InDialogue() const { return inDialogue; }
        void SetDialogue(bool state) { inDialogue = state; }

    private:
        // State flags
        bool inEditor = true;
        bool inDebugMode = false;  
        bool isPaused = false;
        bool IsPlayMode = false;
        bool inDialogue = false;

        // Scenes
        bool mInMenu = false;
        bool mInHowToPlay = false;
        bool mInUpgrade = false;
    };

    // Declare the global instance of EngineState
    extern EngineState engineState;
}