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
#include <chrono>               // for time related things 
#include <cmath>                //for distance calculation

using namespace std;            // For Standard Library
using namespace FMOD;           // For FMOD Library

namespace Framework 
{

    //Belinda 
    //3D Vector structure for spatial positioning 
    struct Vector3 {
        float x, y, z;
        Vector3(float x = 0, float y = 0, float z = 0): x(x), y(y), z(z) {}
    };

    //AUdio source types for different 3D behaviors
    enum class AudioSourceType {
        STEREO_2D,      //UI Sounds, BGM - no 3D processing
        WORLD_3D,       //Environmental sounds with full 3D
        AMBIENT_3D,     //Ambient sounds with subtle 3D
        HYBRID          //Can switch between 2D/3D
    };

    //3D audio settings structure 
    struct Audio3DSettings {
        float dopplerScale = 1.0;   //Doppler effect strength 
        float distanceFactor = 1.0; //Units to meters conversion
        float rolloffScale = 1.0f;  //Distance attenuation curve 
        float minDistance = 1.0f;   //full volume distance 
        float maxDistance = 100.0f; //Inaudible distance 
    };

    //Channel info for tracking 3D properties 
    struct Channel3DInfo {
        Vector3 position; 
        Vector3 velocity;
        float minDistance = 1.0f;
        float maxDistance = 100.0f;
        AudioSourceType type = AudioSourceType::STEREO_2D;
        bool isLooping = false;
        float baseVolume = 1.0;
    };
    //END Belinda

    /**
    *   @class Audio
    *   @brief Manages audio-related tasks such as loading sounds, playing, pausing and adjusting volume
    */
    class Audio : public ISystem 
    {
    public:

        /**
        *   @enum SoundType
        *   @brief Defines types of sounds that can be managed (e.g., background music or sound effects).
        */
        enum SoundType
        {
            EMPTY,              // No sound loaded
            BACKGROUND_MUSIC,   // Background music type
            SOUND_EFFECT        // Sound effect type
        };

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

        /**
         * @brief Prints out detailed information about currently playing audio.
         */
        void PrintAudioInfo() const;

        /**********************/
        //  Audio Management  //
        /**********************/
        /**
         * @brief Converts a string representing a mode (e.g., "loop") to an FMOD_MODE.
         * @param mode The mode as a string.
         * @return FMOD_MODE corresponding to the string.
         */
        FMOD_MODE UE_GetModeFromString(const std::string& mode);

        /**
         * @brief Loads a sound by its custom name using the Asset Manager.
         * @param customName The custom name used to load the sound.
         * @return Pointer to the FMOD::Sound object.
         */
        Sound* UE_LoadSound(const std::string& customName);

        /**
         * @brief Plays a sound by its custom name.
         * @param customName The custom name of the sound.
         */
        void UE_PlaySound(const std::string& customName);

        /**
         * @brief Pauses a sound that is currently playing.
         * @param customName The custom name of the sound.
         */

        void UE_PauseSound(const std::string& customName);

        /**
         * @brief Sets the volume for a specific sound.
         * @param customName The custom name of the sound.
         * @param volume The volume level to set (0.0f to 1.0f).
         */
        void UE_SetVolume(const std::string& customName, float volume);

        //Belinda
        //=========== 3D AUDIO FUNCTIONS ==========================
        /**
        * @brief Plays sound in 3D space with spatial audio 
        * @param customName The custom name of the sound 
        * @param position 3D position of the sound source
        * @param velocity 3D velocity for Doppler effect
        * @param volume Base volume (0.0f to 1.0f)
        * @param sourceType Type of 3D processing to apply 
        * @return Channel ID for later manipulation
        */

        int PlaySound3D(
            const std::string& customName,
            const Vector3& position,
            const Vector3& velocity = Vector3(0, 0, 0),
            float volume = 1.0f,
            AudioSourceType sourceType = AudioSourceType::WORLD_3D
        );

        /**
        * @brief Set the primary 3D listener position and orientation
        * @param position Listener's 3D position
        * @param velocity Listener's 3D velocity for Doppler effect
        * @param forward Forward Direction Vector 
        * @param up Up direction vector 
        */
        void Set3DListener(
            const Vector3& position,
            const Vector3& velocity,
            const Vector3& forward,
            const Vector3& up
        );

        /**
        * @brief Set multiple listeners for split-screen support 
        * @param listenerId Index of the listener (0-based)
        * @param position Listener's 3D position 
        * @param velocity Listener's 3D velocity
        * @param forward Forward Direction Vector 
        * @param up Up direction vector 
        */
        void Set3DListenerMultiple(
            int listenerId,
            const Vector3& position,
            const Vector3& velocity,
            const Vector3& forward,
            const Vector3& up
        );

        /**
        * @brief Update 3D attributes for a moving sound 
        * @param channelId The channel ID returned from PlaySound3D
        * @param position New 3D position
        * @param velocity New 3D velocity
        */
        void UpdateChannel3DAttributes(
            int channelId,
            const Vector3& position,
            const Vector3& velocity
        );

        /**
        * @brief Update 3D attributes by sound name 
        * @param customName The custom name of the sound 
        * @param position New 3D position
        * @param velocity New 3D velocity
        */
        void UpdateChannel3DAttributesByName(
            const::string& customName,
            const Vector3& position,
            const Vector3& velocity
        );

        /**
        * @brief Configure global 3D audio settings 
        * @param settings 3D audio configuration parameters
        */
        void Configure3DSettings(const Audio3DSettings& settings);

        /**
        * @brief Setmin/max distance for volume attenuation
        * @param channelId The channel ID
        * @param minDist Distance at which sound is at full volume 
        * @param maxDist Distance at which sound is inaudible 
        */
        void SetChannelMinMaxDistance(int channelId, float minDist, float maxDist);

        /**
        * @brief Set directional cone for a sound source 
        * @param channelId The channel ID
        * @param insideConeAngle Angle of full volume cone 
        * @param outsideConeAngle Angle of outer cone 
        * @param outsideVolume Volume outside the cone (0.0f to 1.0f)
        * @param orientation Direction the cone is facing 
        */
        void SetChannel3DConeSettings(
            int channelId,
            float insideConeAngle,
            float outsideConeAngle,
            float outsideVolume,
            const Vector3& orientation
        );

        /**
        * @brief Set occlusionfor sound blocking by obstacles 
        * @param channelId The channel ID
        * @param directOcclusion Direct path occlusion (0.0f to 1.0f)
        * @param reverbOcclusion Reverb occlusion (0.0f to 1.0f)
        */
        void SetChannelOcclusion(int channelId, float directOcclusion, float reverbOcclusion);

        /**
        * @brief Create an Audio zone with environmental properties 
        * @param zoneName Unique name for the zone 
        * @param minBounds Minimum bounds of the zone 
        * @param maxBounds Maximum bounds of the zone 
        * @param reverbAmount Amount of revern (0.0f to 1.0f)
        * @param lowPassFilter Low-pass filter amount (9.0f to 1.0f)
        */
        void CreateAudioZone(
            const::string& zoneName,
            const Vector3& minBounds,
            const Vector3& maxBounds,
            float reverbAmount = 0.5,
            float lowPassFilter = 1.0f
        );

        /**
        * @brief Update which zone the listener is in 
        * @param listenerPos Current Listener position
        */
        void UpdateListenerZone(const Vector3& listenerPos);

        /**
        * @brief Check if a position would be audible
        * @param position Position to check 
        * @param maxAudibleDistance Maximum audible distance
        * @return true if position is within audible range 
        */
        bool IsPositionAudible(const Vector3& position, float maxAudibleDistance = 100.0);

        /**
        * @brief Check if a channel is currently playing 
        * @param channelId The channel ID
        * @return true if channel is playing 
        */
        bool IsChannelPlaying(int channelId);

        /**
        * @brief Get the current volume of channel
        * @param channelId The channel ID
        * @return Current volume (0.0f to 1.0f)
        */
        float GetChannelVolume(int channelId);

        /**
        * @brief Stop a specific channel
        * @param channelId The channel ID
        */
        void StopChannel(int channelId);

        /**
        * @brief Stop allcurrently playing channels
        */
        void StopAllChannels();

        // ===================== END NEW 3D AUDIO FUNCTIONS ================
        //END Belinda 

        /*******************************/
        //  Channel Group Management  //
        /******************************/
        /**
         * @brief Sets the volume for a specific channel group.
         * @param group Pointer to the channel group.
         * @param volume The volume level to set (0.0f to 1.0f).
         */
        void UE_SetGroupVolume(FMOD::ChannelGroup* group, float volume);

        /**
         * @brief Pauses a specific channel group.
         * @param group Pointer to the channel group to pause.
         */
        void UE_PauseGroup(FMOD::ChannelGroup* group);

        /**
         * @brief Creates a new channel group for organizing sounds.
         * @param groupName The name of the new channel group.
         */
        void UE_CreateChannelGroup(const std::string& groupName);

        /**********************/
        //  Volume Control    //
        /**********************/
        /**
         * @brief Increases the volume of a specific channel group.
         * @param groupName The name of the channel group.
         */
        void IncrementGroupVol(const std::string& groupName);

        /**
         * @brief Decreases the volume of a specific channel group.
         * @param groupName The name of the channel group.
         */
        void DecrementGroupVol(const std::string& groupName);

        /**
         * @brief Set the master volume
         * @param volume Volume level (0.0f to 1.0f)
         */
        void SetMasterVolume(float volume);

        /**
         * @brief Set the BGM volume
         * @param volume Volume level (0.0f to 1.0f)
         */
        void SetBGMVolume(float volume);

        /**
         * @brief Set the SFX volume
         * @param volume Volume level (0.0f to 1.0f)
         */
        void SetSFXVolume(float volume);

        /**
         * @brief Get current master volume
         * @return Current Master volume (0.0f to 1.0f)
         */
        float GetMasterVolume() const { return masterVolume; }

        /**
        * @brief Get current BGM volume
        * @return Current BGM volume (0.0f to 1.0f)
        */
        float GetBGMVolume() const { return bgmVolume; }

        /**
        * @brief Get current SFX volume
        * @return Current SFX volume (0.0f to 1.0f)
        */
        float GetSFXVolume() const { return sfxVolume; }

        /**
         * @brief Pauses all currently playing audio.
         */
        void PauseAllAudio();

        /**
         * @brief Resumes all paused audio.
         */
        void ResumeAllAudio();

        /**
         * @brief Resets the audio system, clearing loaded sounds and active channels.
         */
        void Reset();

        /*
        * @brief Fade in BGM over specified duration
        * @param duration Fade duration in seconds
        */
        void FadeBGMIn(float duration = 2.0f);

        /*
        * @brief Fade out BGM over specified duration 
        * @param duration fade duration in seconds 
        */
        void FadeBGMOut(float duration = 2.0f);

        /*
        * @brief Crossfade to new BGM
        * @param newBGM Name of the new BGM to fade to 
        * @param duration Crossfade duration in seconds 
        */
        void CrossfadeBGM(const std::string& newBGM, float duration = 1.0f);

        /**
        * @brief Initialize reverb DSP effect 
        */
        void InitializeReverb();

        /*
        * @brief Apply reverb to a specific channel
        * @param channelId The channel ID
        * @param enable true to enable to reverb, false to disable 
        */
        void ApplyReverbToChannel(int channelId, bool enable);

        /*
        * @brief Set reverb properties
        * @param decayTime Reverb decay time in seconds 
        * @param wetLevel Wet signal level in dB
        * @param dryLevel Dry signal level in dB
        */
        void SetReverbProperties(float decayTime, float wetLevel, float dryLevel);
        

    private:

        //Helper functions for 3D Audio
        /*
        * @brief Convert Vector3 to FMOD_VECTOR
        * @param v Vector3 to convert
        * @return FMOD_VECTOR equivalent
        */
        FMOD_VECTOR VectorToFmod(const Vector3& v) {
            FMOD_VECTOR fv;
            fv.x = v.x;
            fv.y = v.y;
            fv.z = v.z;
            return fv;
        }

        /*
        * @brief Convert FMOD_VECTOR to Vector3
        * @param v FMOD_VECTOR to convert
        * @return Vector3 equivalent 
        */
        Vector3 FmodToVector(const FMOD_VECTOR& v) {
            return Vector3(v.x, v.y, v.x);
        }

        /*
        * @brief Get next available channel ID
        * @return Unique channel ID
        */
        int GetNextChannelId() { return nextChannelId++; }

        FMOD::System* pSystem = nullptr;                                            // Create System API, System Object is now a member of the class
        std::unordered_map<std::string, FMOD::ChannelGroup*> activeChannelGroup;    // Map of active channel groups
        std::unordered_map<std::string, FMOD::Channel*> activeChannels;             // Map of active channels
        std::unordered_map<std::string, FMOD::Sound*> loadedSounds;                 // Map for storing loaded sounds
        const float volChangeAmount = 0.1f;                                         // Fixed amount to change volume

        //Additional maps for 3D audio management
        std::unordered_map<int, FMOD::Channel*> activeChannelsById;
        std::unordered_map<int, Channel3DInfo> channel3DInfo;

        //Audio zones for environmental effects 
        struct AudioZone {
            Vector3 minBounds;
            Vector3 maxBounds;
            float reverbAmount;
            float lowPassFilter;
        };
        std::unordered_map<std::string, AudioZone> audioZones;
        std::string currentZone;

        ///3D Settings 
        Audio3DSettings current3DSettings;

        //Volume Settings 
        float masterVolume = 1.0f;
        float bgmVolume = 1.0f;
        float sfxVolume = 1.0f;

        //Fade state tracking
        bool isFadingIn = false;                                                    // NEW: Is BGM fading in
        bool isFadingOut = false;                                                   // NEW: Is BGM fading out
        float fadeTimer = 0.0f;                                                     // NEW: Current fade timer
        float fadeDuration = 2.0f;                                                  // NEW: Fade duration
        float fadeStartVolume = 0.0f;                                               // NEW: Starting fade volume
        float fadeTargetVolume = 1.0f;                                              // NEW: Target fade volume
        int currentBGMChannel = -1;                                                 // NEW: Current BGM channel ID
        int pendingBGMChannel = -1;

        //DSP Effects
        FMOD::DSP * reverbDSP = nullptr;                                             // NEW: Reverb DSP effect
        FMOD::DSP* lowPassFilter = nullptr;                                         // NEW: Low-pass filter DSP

        //Channel ID counter
        int nextChannelId = 1;

        //Debounce for volume controls
        std::chrono::steady_clock::time_point lastVolumeChangeTime;                 // NEW: Last volume change time
        const std::chrono::milliseconds volumeDebounceTime{ 100 };
    };

    extern Audio GlobalAudio;   // Declare a global Audio Instance
}
#endif // _AUDIO_H_