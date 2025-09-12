///////////////////////////////////////////////////////////////////////////////
///
///	@File  : Audio.cpp
/// @Brief : This file implements the Audio system, which manages audio-related
///          tasks using FMOD. Using asset manager to retrieve sound assets to
///          provide functionality to play and control both background music and
///          sound effects.
///
///	@Authors : Edwin Leow
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "Audio.h"
#include "AssetManager.h"

namespace Framework 
{
    Audio GlobalAudio;      // Define the global Audio Instance

    // Constructor
    Audio::Audio() 
    {
        System_Create(&pSystem);                            // Create the FMOD System Object
        pSystem->init(32, FMOD_INIT_NORMAL, nullptr);       // Create 32 Channels for 32 Audio
    }

    // Destructor
    Audio::~Audio() 
    {
        pSystem->release();                 // Free the FMOD System Object
    }

    // Initialize the system
    void Audio::Initialize() {}

    // Update the system
    void Audio::Update(float deltaTime) 
    {
        (void)deltaTime;            // Suprress unused parameter warning
        pSystem->update();          // Must keep updating, else audio will not play
    }

    // Get the name of the system
    std::string Audio::GetName() 
    {
        return "Audio";
    }
}