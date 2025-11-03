///////////////////////////////////////////////////////////////////////////////
///
/// @file AudioComponent.h
/// @brief Component for attaching 3D audio to entities
/// 
/// @Authors: Belinda Chua Jia Xuan 
/// @Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef AUDIO_COMPONENT_H
#define AUDIO_COMPONENT_H

#include <string>
#include <fmod_studio.hpp>
#include <glm.hpp>

namespace Framework {

    struct AudioComponent {
        // Audio event data
        std::string eventName;                          // Name of the FMOD event to play
        FMOD::Studio::EventInstance* eventInstance = nullptr;  // Instance of the playing sound

        // 3D audio properties
        bool is3D = true;                              // Whether this is a 3D sound
        float minDistance = 1.0f;                      // Distance where sound is at full volume
        float maxDistance = 100.0f;                    // Distance where sound is inaudible

        // Playback properties
        float volume = 1.0f;                           // Volume multiplier (0.0 to 1.0)
        bool isPlaying = false;                        // Current playback state
        bool loop = false;                             // Whether sound should loop
        bool playOnStart = true;                       // Auto-play when entity is created

        // Runtime data
        glm::vec3 lastPosition = glm::vec3(0.0f);     // Last known position for velocity calculation
    };

}

#endif // AUDIO_COMPONENT_H