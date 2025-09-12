///////////////////////////////////////////////////////////////////////////////
///
///	@file  : Audio.h
/// @Brief : Header file for Audio system that manages sound loading, playback,
///          channel groups, and volume control using FMOD
///	
///	@Authors: Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "pch.h"                // Contains all required libraries
#include "System.h"             // For Framework
#include "fmod.hpp"				// FMOD_CORE_API
#include "fmod_studio.hpp"		// FMOD_STUDIO_API
#include <unordered_map>        // std::unordered_map

using namespace std;            // For Standard Library
using namespace FMOD;           // For FMOD Library

namespace Framework 
{
    /**
    *   @class Audio
    *   @brief Manages audio-related tasks such as loading sounds, playing, pausing and adjusting volume
    */
    class Audio : public ISystem 
    {
    public:

        /**
        *   @brief Constructor for Audio system.
        *   @param AssetManager Pointer to the asset manager to load sound assets.
        */
        Audio();

        /**
         * @brief Destructor for Audio system.
         */
        ~Audio();

        /***********************/
        //  General Functions  //
        /***********************/
        /**
         * @brief Initializes the Audio system.
         */
        void Initialize() override;

        /**
         * @brief Updates the Audio system.
         * @param deltaTime Time since the last frame.
         */
        void Update(float deltaTime) override;

        /**
         * @brief Retrieves the system's name.
         * @return The name of the system as a string.
         */
        std::string GetName() override;

    private:
        FMOD::System* pSystem = nullptr;                                            // Create System API, System Object is now a member of the class
    };
    extern Audio GlobalAudio;   // Declare a global Audio Instance
}
#endif // _AUDIO_H_