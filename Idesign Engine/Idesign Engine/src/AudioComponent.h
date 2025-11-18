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

namespace Framework 
{
    struct AudioMetadata
    {
        AssetHandle guid;          // Unique asset identifier
        std::string name;          // Human-readable name
        std::string path;          // File path or FMOD bank path
        std::string mode;          // Streaming, static, etc.
        std::string soundType;     // Music, SFX, Ambient, UI
        float duration = 0.0f;     // Length in seconds
    };

    // Property registration for AudioMetadata
    // In AudioComponent.h, update the property registration:
    inline const std::vector g_AudioMetadataPropsArray =
    {
        PROP_GUID_HIDDEN(guid,          AudioMetadata),
        PROP_STRING(name,               AudioMetadata),
        PROP_STRING(path,               AudioMetadata),  // FMOD event path (e.g., "event:/piano")
        PROP_STRING_READONLY(mode,      AudioMetadata),
        PROP_STRING_READONLY(soundType, AudioMetadata),
        PROP_FLOAT_READONLY(duration,   AudioMetadata)
    };

    inline const prop_list g_AudioMetadataProps{ "AudioMetadata", g_AudioMetadataPropsArray };
    inline global_registration g_AudioMetadataReg(&g_AudioMetadataProps);

    //------------------------------------------------------------------------------------------------------------------//

    // Player's "Ears"
    struct ListenerComponent
    {
        glm::vec3 position = glm::vec3(0.0f);               // Listener position
        glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);   // Orientation forward
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);         // Orientation up
        glm::vec3 velocity = glm::vec3(0.0f);               // Velocity for Doppler

        bool active = true;                                 // Check if this listener is active
        int listenerIndex = 0;                              // FMOD listener index
    };

    // Property registration for ListenerComponent
    inline const std::vector g_ListenerComponentPropsArray =
    {
        PROP_VEC3(position,     ListenerComponent),
        PROP_VEC3(forward,      ListenerComponent),
        PROP_VEC3(up,           ListenerComponent),
        PROP_VEC3(velocity,     ListenerComponent),
        PROP_BOOL(active,       ListenerComponent),
        PROP_INT(listenerIndex, ListenerComponent)
    };

    inline const prop_list g_ListenerComponentProps{ "ListenerComponent", g_ListenerComponentPropsArray };
    inline global_registration g_ListenerComponentReg(&g_ListenerComponentProps);

    //------------------------------------------------------------------------------------------------------------------//

    // Audio Source || Sound Emitter
    struct AudioComponent 
    {
        AssetHandle guid;                                       // Asset reference to audio event/clip from AudioMetadata
        std::string eventName;                                  // Name of the FMOD event to play
        FMOD::Studio::EventInstance* eventInstance = nullptr;   // Instance of the playing sound

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

        // Optional: internal ID or handle for audio system
        int channelID = -1;
    };

    // Property registration for AudioComponent
    inline const std::vector g_AudioComponentPropsArray =
    {
        PROP_GUID_HIDDEN(guid,      AudioComponent),
        PROP_STRING(eventName,      AudioComponent),
        PROP_BOOL(is3D,             AudioComponent),
        PROP_FLOAT(minDistance,     AudioComponent),
        PROP_FLOAT(maxDistance,     AudioComponent),
        PROP_SLIDER_FLOAT(volume,   AudioComponent, 0.0f, 1.0f),
        PROP_BOOL(loop,             AudioComponent),
        PROP_BOOL(playOnStart,      AudioComponent),
        PROP_BOOL(isPlaying,        AudioComponent),
        PROP_VEC3(lastPosition,     AudioComponent)
    };

    inline const prop_list g_AudioComponentProps{ "AudioComponent", g_AudioComponentPropsArray };
    inline global_registration g_AudioComponentReg(&g_AudioComponentProps);
}
#endif // AUDIO_COMPONENT_H