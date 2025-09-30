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
        pSystem->init(512, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, nullptr);       // Create 512 CHANNELS (FMOD_INIT_3D_RIGHTHANDED FOR 3D AUDIO SUPPORT)

        //Initialize 3D settings with defaults
        Configure3DSettings(current3DSettings);

        // Create Channel Groups on Initialization
        UE_CreateChannelGroup("BackgroundMusic");
        UE_CreateChannelGroup("SoundEffects");
        UE_CreateChannelGroup("Ambient3D");

        //Initialize DSP effects for environmental audio 
        InitializeReverb();
    }

    // Destructor
    Audio::~Audio() 
    {
        //Clean up DSP Effects
        if (reverbDSP) {
            reverbDSP->release();
            reverbDSP = nullptr;
        }
        if (lowPassFilter) {
            lowPassFilter->release();
            lowPassFilter = nullptr;
        }

        //Note: FMOD will automatically 
        //  channel when sound is released, so dont nid to stop channel
        for (auto& pair : loadedSounds)     // Loop through all loadedSounds
        {
            if (pair.second != nullptr)
            {
                pair.second->release();     // free the sound object
            }
        }
        loadedSounds.clear();               // Clear loaded sounds map
        activeChannels.clear();             // Clear active channels map
        activeChannelGroup.clear();
        activeChannelsById.clear();         //Clear additional 3D related maps 
        channel3DInfo.clear();
        audioZones.clear();
        pSystem->release();                 // Free the FMOD System Object
    }

    // Initialize the system
    void Audio::Initialize() 
    {
        //System_Create(&pSystem);                            // Create the FMOD System Object
        //pSystem->init(32, FMOD_INIT_NORMAL, nullptr);       // Create 32 Channels for 32 Audio
    
        //// Create Channel Groups on Initialization
        //UE_CreateChannelGroup("BackgroundMusic");
        //UE_CreateChannelGroup("SoundEffects");

        //Log 3D audio initialization
        std::cout << "Audio System Initialized with 3D support" << std::endl;
        std::cout << "3D Settings - Doppler: " << current3DSettings.dopplerScale
            << ", Distance Factor: " << current3DSettings.distanceFactor
            << ", Rolloff: " << current3DSettings.rolloffScale << std::endl;
    }

    // Update the system
    void Audio::Update(float deltaTime) 
    {
        //(void)deltaTime;            // Suprress unused parameter warning

        //Handle fade effects for smooth BGM transitions
        if (isFadingIn || isFadingOut) {
            fadeTimer += deltaTime;
            float progress = fadeTimer / fadeDuration;
            progress = std::min(1.0f, std::max(0.0f, progress));

            if (isFadingIn) {
                float currentVolume = fadeStartVolume + (fadeTargetVolume - fadeStartVolume) * progress;
                if (currentBGMChannel >= 0 && activeChannelsById.count(currentBGMChannel)) {
                    activeChannelsById[currentBGMChannel]->setVolume(currentVolume * bgmVolume * masterVolume);
                }

                if (progress >= 1.0f) {
                    isFadingIn = false;
                    std::cout << "Fade in complete" << std::endl;
                }
            }
            else if (isFadingOut) {
                float currentVolume = fadeStartVolume * (1.0f - progress);
                if (currentBGMChannel >= 0 && activeChannelsById.count(currentBGMChannel)) {
                    activeChannelsById[currentBGMChannel]->setVolume(currentVolume * bgmVolume * masterVolume);
                }

                if (progress >= 1.0f) {
                    isFadingOut = false;
                    if (currentBGMChannel >= 0) {
                        StopChannel(currentBGMChannel);
                    }

                    // Switch to pending BGM if crossfading
                    if (pendingBGMChannel >= 0) {
                        currentBGMChannel = pendingBGMChannel;
                        pendingBGMChannel = -1;
                        FadeBGMIn(fadeDuration);
                    }
                }
            }
        }
        pSystem->update();          // Must keep updating, else audio will not play

    }

    // Get the name of the system
    std::string Audio::GetName() 
    {
        return "Audio";
    }

    void Audio::PrintAudioInfo() const
    {
        // Note: FMOD does not store the name or path of a sound file, only addresses, thus it printed out the addresses of the audio/channels
        std::cout << "=== Audio Engine Information ===" << std::endl;

        // Print Loaded Sounds Information
        std::cout << "\nLoaded Sounds:" << std::endl;
        if (loadedSounds.empty())
        {
            std::cout << "No sounds loaded." << std::endl;
        }
        else
        {
            for (const auto& soundEntry : loadedSounds)
            {
                std::cout << "Custom Name: " << soundEntry.first << std::endl;
                std::cout << "FMOD::Sound*: " << soundEntry.second << std::endl;
            }
        }

        // Print Active Channels Information
        std::cout << "\nActive Channels:" << std::endl;
        if (activeChannels.empty())
        {
            std::cout << "No channels active." << std::endl;
        }
        else
        {
            for (const auto& channelEntry : activeChannels)
            {
                std::cout << "Custom Name: " << channelEntry.first << std::endl;

                bool isPlaying = false;
                channelEntry.second->isPlaying(&isPlaying);

                std::cout << "FMOD::Channel*: " << channelEntry.second << std::endl;
                std::cout << "Is Playing: " << (isPlaying ? "Yes" : "No") << std::endl;

                float volume = 0.0f;
                channelEntry.second->getVolume(&volume);
                std::cout << "Volume: " << volume << std::endl;

                FMOD::ChannelGroup* group = nullptr;
                channelEntry.second->getChannelGroup(&group);
                if (group)
                {
                    char groupName[256];
                    group->getName(groupName, 256);
                    std::cout << "Channel Group: " << groupName << std::endl;
                }
                else
                {
                    std::cout << "Channel Group: None" << std::endl;
                }
            }
        }

        //Print 3D Channel Information
        std::cout << "\n3D Channels:" << std::endl;
        if (channel3DInfo.empty()) {
            std::cout << "No 3D channels active." << std::endl;
        }
        else {
            for (const auto& [id, info] : channel3DInfo) {
                std::cout << "Channel ID: " << id << std::endl;
                std::cout << "  Position: (" << info.position.x << ", "
                    << info.position.y << ", " << info.position.z << ")" << std::endl;
                std::cout << "  Velocity: (" << info.velocity.x << ", "
                    << info.velocity.y << ", " << info.velocity.z << ")" << std::endl;
                std::cout << "  Min Distance: " << info.minDistance << std::endl;
                std::cout << "  Max Distance: " << info.maxDistance << std::endl;
                std::cout << "  Type: ";
                switch (info.type) {
                case AudioSourceType::STEREO_2D: std::cout << "2D Stereo"; break;
                case AudioSourceType::WORLD_3D: std::cout << "3D World"; break;
                case AudioSourceType::AMBIENT_3D: std::cout << "3D Ambient"; break;
                case AudioSourceType::HYBRID: std::cout << "Hybrid"; break;
                }
                std::cout << std::endl;
            }
        }

        // Print Active Channel Groups Information
        std::cout << "\nActive Channel Groups:" << std::endl;
        if (activeChannelGroup.empty())
        {
            std::cout << "No channel groups active." << std::endl;
        }
        else
        {
            for (const auto& groupEntry : activeChannelGroup)
            {
                std::cout << "Group Name: " << groupEntry.first << std::endl;
                std::cout << "FMOD::ChannelGroup*: " << groupEntry.second << std::endl;

                float groupVolume = 0.0f;
                groupEntry.second->getVolume(&groupVolume);
                std::cout << "Group Volume: " << groupVolume << std::endl;

                int numChannels = 0;
                groupEntry.second->getNumChannels(&numChannels);
                std::cout << "Number of Channels: " << numChannels << std::endl;
            }
        }

        std::cout << "================================" << std::endl;
    }

    Audio::SoundType Audio::ParseSoundType(const std::string& str)
    {
        if (str == "background") return Audio::SoundType::BACKGROUND_MUSIC;
        if (str == "effect") return Audio::SoundType::SOUND_EFFECT;
        throw std::invalid_argument("Unknown SoundType: " + str);
    }

    // Function to change from String to FMOD_MODE
    FMOD_MODE Audio::UE_GetModeFromString(const std::string& mode)
    {
        if (mode == "loop")         
        {   
            return FMOD_LOOP_NORMAL;        // Looping Mode
        }
        else if (mode == "oneshot")
        {
            return FMOD_DEFAULT;            // One-shot playback
        }
        else
        {
            throw std::runtime_error("Invalid sound mode : " + mode);
        }
    }

    Sound* Audio::UE_LoadSound(const std::string& customName)
    {
        Sound* pSound = nullptr;        // Initialize a Sound pointer to nullptr

        AudioProperties* audioProps = AssetManager::Instance().Get<AudioProperties>(AssetManager::Instance().FindGuid<AudioProperties>(customName));
        if (!audioProps)
        {
            std::cout << "Error: AudioProperties not found for customName: " << customName << std::endl;
            return nullptr;
        }

        const std::string& filePath = audioProps->path;
        const std::string& modeString = audioProps->mode;

        std::cout << "File path: " << filePath << std::endl;
        std::cout << "Mode: " << modeString << std::endl;

        FMOD_MODE mode = UE_GetModeFromString(modeString);
        std::cout << "FMOD Mode: " << mode << std::endl;

        FMOD_RESULT result = pSystem->createSound(filePath.c_str(), mode, nullptr, &pSound);
        if (result != FMOD_OK)
        {
            std::cout << "Error: Failed to create sound for " << customName << " with FMOD result " << result << std::endl;
            return nullptr;
        }

        unsigned int length = 0;
        pSound->getLength(&length, FMOD_TIMEUNIT_MS);
        audioProps->duration = length / 1000.0f;

        loadedSounds[customName] = pSound;
        return pSound;
    }

    void Audio::UE_PlaySound(const std::string& customName)
    {
        Sound* pSound = UE_LoadSound(customName);  // Load sound from AudioProperties
        if (!pSound)
        {
            std::cout << "Error: Sound " << customName << " could not be loaded." << std::endl;
            return;
        }

        AudioProperties* audioProps = AssetManager::Instance().Get<AudioProperties>(AssetManager::Instance().FindGuid<AudioProperties>(customName));
        if (!audioProps)
        {
            std::cout << "Error: AudioProperties not found for customName: " << customName << std::endl;
            return;
        }

        // Check for existing channel
        auto it = activeChannels.find(customName);
        if (it != activeChannels.end() && it->second != nullptr)
        {
            bool isPlaying = false;
            it->second->isPlaying(&isPlaying);

            if (isPlaying)
            {
                std::cout << "Audio " << customName << " is already playing." << std::endl;
                return;
            }

            bool isPaused = false;
            it->second->getPaused(&isPaused);

            if (isPaused)
            {
                std::cout << "Resuming audio " << customName << "." << std::endl;
                it->second->setPaused(false);
                return;
            }

            std::cout << "Audio " << customName << " has finished, replaying." << std::endl;
            activeChannels.erase(it);
        }

        // Play sound
        FMOD::Channel* pChannel = nullptr;
        FMOD_RESULT result = pSystem->playSound(pSound, nullptr, false, &pChannel);
        SoundType soundType = ParseSoundType(audioProps->soundType);
        if (result != FMOD_OK)
        {
            std::cout << "Error: Cannot play audio " << customName << ", FMOD result: " << result << std::endl;
            return;
        }

        if (pChannel)
        {
            if (soundType == BACKGROUND_MUSIC)
                pChannel->setChannelGroup(activeChannelGroup["BackgroundMusic"]);
            else if (soundType == SOUND_EFFECT)
                pChannel->setChannelGroup(activeChannelGroup["SoundEffects"]);

            activeChannels[customName] = pChannel;

            // Assign a unique channel ID and store it
            static int nextChannelId = 1;
            int assignedId = nextChannelId++;
            activeChannelsById[assignedId] = pChannel;
            audioProps->channelID = assignedId;
            audioProps->isPlaying = true;
            std::cout << "Playing audio " << customName << "." << std::endl;
        }
    }

    //========================== NEW 3D AUDIO FUNCTIONS ==============================
    
    //Play a sound in 3D space with full spatial audio support 
    int Audio::PlaySound3D(
        const std::string& customName,
        const Vector3& position,
        const Vector3& velocity,
        float volume,
        AudioSourceType sourceType)
    {
        // Load the sound if not already loaded
        Sound* pSound = nullptr;
        auto it = loadedSounds.find(customName);
        if (it != loadedSounds.end()) {
            pSound = it->second;
        }
        else {
            pSound = UE_LoadSound(customName);
            if (!pSound) {
                std::cerr << "Failed to load sound: " << customName << std::endl;
                return -1;
            }
        }

        AudioProperties* audioProps = AssetManager::Instance().Get<AudioProperties>(AssetManager::Instance().FindGuid<AudioProperties>(customName));
        if (!audioProps)
        {
            std::cout << "Error: AudioProperties not found for customName: " << customName << std::endl;
            return -1;
        }

        // Configure sound mode based on source type
        FMOD_MODE mode = FMOD_DEFAULT;
        bool is3D = (sourceType == AudioSourceType::WORLD_3D ||
            sourceType == AudioSourceType::AMBIENT_3D);

        if (is3D) {
            mode |= FMOD_3D | FMOD_3D_WORLDRELATIVE | FMOD_3D_INVERSEROLLOFF;
        }
        else {
            mode |= FMOD_2D;
        }

        // Set looping mode
        if (audioProps->mode == "loop") {
            mode |= FMOD_LOOP_NORMAL;
        }
        else {
            mode |= FMOD_LOOP_OFF;
        }

        pSound->setMode(mode);

        // Create and configure channel
        FMOD::Channel* pChannel = nullptr;
        FMOD_RESULT result = pSystem->playSound(pSound, nullptr, true, &pChannel);

        if (result != FMOD_OK || !pChannel) {
            std::cerr << "Failed to create channel for: " << customName << std::endl;
            return -1;
        }

        // Get unique channel ID
        int channelId = GetNextChannelId();

        // Set 3D attributes if applicable
        if (is3D) {
            FMOD_VECTOR pos = VectorToFmod(position);
            FMOD_VECTOR vel = VectorToFmod(velocity);
            pChannel->set3DAttributes(&pos, &vel);

            // Set distance parameters based on source type
            if (sourceType == AudioSourceType::AMBIENT_3D) {
                pChannel->set3DMinMaxDistance(5.0f, 200.0f);
                pChannel->set3DLevel(0.7f); // Partial 3D for ambient sounds
            }
            else {
                pChannel->set3DMinMaxDistance(
                    current3DSettings.minDistance,
                    current3DSettings.maxDistance);
                pChannel->set3DLevel(1.0f); // Full 3D
            }
        }
        
        SoundType soundType = ParseSoundType(audioProps->soundType);

        // Assign to appropriate channel group
        if (soundType == BACKGROUND_MUSIC) {
            pChannel->setChannelGroup(activeChannelGroup["BackgroundMusic"]);
            currentBGMChannel = channelId;
        }
        else if (sourceType == AudioSourceType::AMBIENT_3D) {
            pChannel->setChannelGroup(activeChannelGroup["Ambient3D"]);
        }
        else {
            pChannel->setChannelGroup(activeChannelGroup["SoundEffects"]);
        }

        // Set volume with master/group volume scaling
        float finalVolume = volume;
        if (soundType == BACKGROUND_MUSIC) {
            finalVolume *= bgmVolume;
        }
        else {
            finalVolume *= sfxVolume;
        }
        finalVolume *= masterVolume;
        pChannel->setVolume(finalVolume);

        // Store channel info
        activeChannelsById[channelId] = pChannel;
        activeChannels[customName] = pChannel;

        // Store 3D info for later updates
        channel3DInfo[channelId] = {
            position, velocity,
            current3DSettings.minDistance,
            current3DSettings.maxDistance,
            sourceType,
            audioProps->mode == "loop",
            volume
        };

        // Start playing
        pChannel->setPaused(false);

        std::cout << "Playing 3D sound: " << customName << " at position ("
            << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;

        return channelId;
    }

    //Set the 3D listener position and orientation
    void Audio::Set3DListener(
        const Vector3& position,
        const Vector3& velocity,
        const Vector3& forward,
        const Vector3& up)
    {
        FMOD_VECTOR pos = VectorToFmod(position);
        FMOD_VECTOR vel = VectorToFmod(velocity);
        FMOD_VECTOR fwd = VectorToFmod(forward);
        FMOD_VECTOR upVec = VectorToFmod(up);

        pSystem->set3DListenerAttributes(0, &pos, &vel, &fwd, &upVec);
    }

    //Support multiple listeners for split-screen
    void Audio::Set3DListenerMultiple(
        int listenerId,
        const Vector3& position,
        const Vector3& velocity,
        const Vector3& forward,
        const Vector3& up)
    {
        // Set number of listeners if needed
        int numListeners;
        pSystem->get3DNumListeners(&numListeners);
        if (listenerId >= numListeners) {
            pSystem->set3DNumListeners(listenerId + 1);
        }

        FMOD_VECTOR pos = VectorToFmod(position);
        FMOD_VECTOR vel = VectorToFmod(velocity);
        FMOD_VECTOR fwd = VectorToFmod(forward);
        FMOD_VECTOR upVec = VectorToFmod(up);

        pSystem->set3DListenerAttributes(listenerId, &pos, &vel, &fwd, &upVec);
    }

    //Update 3D attributes for a moving sound 
    void Audio::UpdateChannel3DAttributes(
        int channelId,
        const Vector3& position,
        const Vector3& velocity)
    {
        auto it = activeChannelsById.find(channelId);
        if
            (it != activeChannelsById.end() && it->second) {
            FMOD_VECTOR pos = VectorToFmod(position);
            FMOD_VECTOR vel = VectorToFmod(velocity);
            it->second->set3DAttributes(&pos, &vel);

            // Update cached info
            if (channel3DInfo.count(channelId)) {
                channel3DInfo[channelId].position = position;
                channel3DInfo[channelId].velocity = velocity;
            }
        }
    }

    //Update 3D attributes by sound name 
    void Audio::UpdateChannel3DAttributesByName(
        const std::string& customName,
        const Vector3& position,
        const Vector3& velocity)
    {
        auto it = activeChannels.find(customName);
        if (it != activeChannels.end() && it->second) {
            FMOD_VECTOR pos = VectorToFmod(position);
            FMOD_VECTOR vel = VectorToFmod(velocity);
            it->second->set3DAttributes(&pos, &vel);
        }
    }

    //Configure global 3D settings 
    void Audio::Configure3DSettings(const Audio3DSettings& settings)
    {
        current3DSettings = settings;
        pSystem->set3DSettings(
            settings.dopplerScale,
            settings.distanceFactor,
            settings.rolloffScale);
    }

    //Set min/max distance for a specific channel
    void Audio::SetChannelMinMaxDistance(int channelId, float minDist, float maxDist)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            it->second->set3DMinMaxDistance(minDist, maxDist);

            // Update cached info
            if (channel3DInfo.count(channelId)) {
                channel3DInfo[channelId].minDistance = minDist;
                channel3DInfo[channelId].maxDistance = maxDist;
            }
        }
    }

    //Set cone for directional sounds 
    void Audio::SetChannel3DConeSettings(
        int channelId,
        float insideConeAngle,
        float outsideConeAngle,
        float outsideVolume,
        const Vector3& orientation)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            it->second->set3DConeSettings(insideConeAngle, outsideConeAngle, outsideVolume);
            FMOD_VECTOR orient = VectorToFmod(orientation);
            it->second->set3DConeOrientation(&orient);
        }
    }

    //Set occlusion for realistic sound blocking 
    void Audio::SetChannelOcclusion(int channelId, float directOcclusion, float reverbOcclusion)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            it->second->set3DOcclusion(directOcclusion, reverbOcclusion);
        }
    }

    //Create an audio zone with environmental properties
    void Audio::CreateAudioZone(
        const std::string& zoneName,
        const Vector3& minBounds,
        const Vector3& maxBounds,
        float reverbAmount,
        float lowPassFilterValue)
    {
        audioZones[zoneName] = { minBounds, maxBounds, reverbAmount, lowPassFilterValue };
        std::cout << "Created audio zone: " << zoneName << std::endl;
    }

    //Update listener's current zone for environmental effects
    void Audio::UpdateListenerZone(const Vector3& listenerPos)
    {
        std::string newZone = "";

        // Check which zone the listener is in
        for (const auto& [name, zone] : audioZones) {
            if (listenerPos.x >= zone.minBounds.x && listenerPos.x <= zone.maxBounds.x &&
                listenerPos.y >= zone.minBounds.y && listenerPos.y <= zone.maxBounds.y &&
                listenerPos.z >= zone.minBounds.z && listenerPos.z <= zone.maxBounds.z) {
                newZone = name;
                break;
            }
        }

        // If zone changed, apply new zone settings
        if (newZone != currentZone) {
            currentZone = newZone;

            if (!newZone.empty() && audioZones.count(newZone)) {
                const AudioZone& zone = audioZones[newZone];

                // Apply reverb settings
                if (reverbDSP) {
                    reverbDSP->setBypass(false);
                    reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL,
                        -12.0f + (zone.reverbAmount * 12.0f));
                }

                // Apply low-pass filter
                if (lowPassFilter) {
                    if (zone.lowPassFilter < 1.0f) {
                        lowPassFilter->setBypass(false);
                        lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF,
                            zone.lowPassFilter * 22000.0f);
                    }
                    else {
                        lowPassFilter->setBypass(true);
                    }
                }

                std::cout << "Entered audio zone: " << newZone << std::endl;
            }
            else {
                // No zone - disable effects
                if (reverbDSP) reverbDSP->setBypass(true);
                if (lowPassFilter) lowPassFilter->setBypass(true);
            }
        }
    }

    //Check if a position is within audible range
    bool Audio::IsPositionAudible(const Vector3& position, float maxAudibleDistance)
    {
        FMOD_VECTOR listenerPos, listenerVel, listenerFwd, listenerUp;
        pSystem->get3DListenerAttributes(0, &listenerPos, &listenerVel, &listenerFwd, &listenerUp);

        float dx = position.x - listenerPos.x;
        float dy = position.y - listenerPos.y;
        float dz = position.z - listenerPos.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        return distance <= maxAudibleDistance;
    }

    //Initialize reverb and filter DSP effects
    void Audio::InitializeReverb()
    {
        // Create reverb DSP
        pSystem->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &reverbDSP);
        if (reverbDSP) {
            // Set default reverb properties (room-like)
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, 1.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, 0.02f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, 0.04f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, 5000.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HFDECAYRATIO, 0.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, 100.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, 100.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, 250.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFGAIN, 0.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_HIGHCUT, 20000.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYLATEMIX, 0.5f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, -12.0f);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, 0.0f);

            // Add to master channel group but bypass initially
            FMOD::ChannelGroup* masterGroup;
            pSystem->getMasterChannelGroup(&masterGroup);
            masterGroup->addDSP(0, reverbDSP);
            reverbDSP->setBypass(true);
        }

        // Create low-pass filter DSP
        pSystem->createDSPByType(FMOD_DSP_TYPE_LOWPASS, &lowPassFilter);
        if (lowPassFilter) {
            lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f);

            FMOD::ChannelGroup* masterGroup;
            pSystem->getMasterChannelGroup(&masterGroup);
            masterGroup->addDSP(0, lowPassFilter);
            lowPassFilter->setBypass(true);
        }
    }

    //Apply reverb to a specific channel
    void Audio::ApplyReverbToChannel(int channelId, bool enable)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second && reverbDSP) {
            if (enable) {
                it->second->addDSP(0, reverbDSP);
            }
            else {
                it->second->removeDSP(reverbDSP);
            }
        }
    }

    //Check if a channel is currently playing 
    bool Audio::IsChannelPlaying(int channelId)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            bool isPlaying = false;
            FMOD_RESULT result = it->second->isPlaying(&isPlaying);
            if (result == FMOD_OK) {
                return isPlaying;
            }
        }
        return false;
    }

    //Get the current volume of a channel 
    float Audio::GetChannelVolume(int channelId)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            float volume = 0.0f;
            FMOD_RESULT result = it->second->getVolume(&volume);
            if (result == FMOD_OK) {
                return volume;
            }
        }
        return 0.0f;
    }

    //Stop a specific channel by ID
    void Audio::StopChannel(int channelId)
    {
        auto it = activeChannelsById.find(channelId);
        if (it != activeChannelsById.end() && it->second) {
            it->second->stop();

            // Clean up the channel from our tracking maps
            activeChannelsById.erase(it);

            // Also remove from channel3DInfo
            auto infoIt = channel3DInfo.find(channelId);
            if (infoIt != channel3DInfo.end()) {
                channel3DInfo.erase(infoIt);
            }

            // If this was the BGM channel, reset it
            if (channelId == currentBGMChannel) {
                currentBGMChannel = -1;
            }
        }
    }

    //Stop all currently playing channels 
    void Audio::StopAllChannels()
    {
        // Stop all channels by name
        for (auto& channelEntry : activeChannels) {
            if (channelEntry.second != nullptr) {
                channelEntry.second->stop();
            }
        }
        activeChannels.clear();

        // Stop all channels by ID
        for (auto& channelEntry : activeChannelsById) {
            if (channelEntry.second != nullptr) {
                channelEntry.second->stop();
            }
        }
        activeChannelsById.clear();
        channel3DInfo.clear();

        // Reset BGM channel
        currentBGMChannel = -1;
        pendingBGMChannel = -1;

        std::cout << "All audio channels stopped" << std::endl;
    }

    //Set master volume level 
    void Audio::SetMasterVolume(float volume)
    {
        masterVolume = std::max(0.0f, std::min(1.0f, volume));

        // Apply to master channel group
        FMOD::ChannelGroup* masterGroup = nullptr;
        pSystem->getMasterChannelGroup(&masterGroup);
        if (masterGroup) {
            masterGroup->setVolume(masterVolume);
        }
        std::cout << "Master volume set to: " << masterVolume << std::endl;
    }

    //Set BGM Volume level 
    void Audio::SetBGMVolume(float volume)
    {
        bgmVolume = std::max(0.0f, std::min(1.0f, volume));

        // Apply to BGM channel group
        auto it = activeChannelGroup.find("BackgroundMusic");
        if (it != activeChannelGroup.end() && it->second) {
            it->second->setVolume(bgmVolume);
        }
        std::cout << "BGM volume set to: " << bgmVolume << std::endl;
    }

    //Set SFX volume level 
    void Audio::SetSFXVolume(float volume)
    {
        sfxVolume = std::max(0.0f, std::min(1.0f, volume));

        // Apply to SFX channel group
        auto it = activeChannelGroup.find("SoundEffects");
        if (it != activeChannelGroup.end() && it->second) {
            it->second->setVolume(sfxVolume);
        }

        // Also apply to Ambient3D group
        it = activeChannelGroup.find("Ambient3D");
        if (it != activeChannelGroup.end() && it->second) {
            it->second->setVolume(sfxVolume);
        }
        std::cout << "SFX volume set to: " << sfxVolume << std::endl;
    }

    //Fade in BGM over specified duration 
    void Audio::FadeBGMIn(float duration)
    {
        if (currentBGMChannel >= 0 && activeChannelsById.count(currentBGMChannel)) {
            fadeStartVolume = 0.0f;
            fadeTargetVolume = 1.0f;
            fadeDuration = duration;
            fadeTimer = 0.0f;
            isFadingIn = true;
            isFadingOut = false;

            // Start at zero volume
            activeChannelsById[currentBGMChannel]->setVolume(0.0f);

            // Enable low-pass filter for fade-in effect (optional)
            if (lowPassFilter) {
                lowPassFilter->setBypass(false);
                lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 500.0f);
            }
            std::cout << "Starting BGM fade-in over " << duration << " seconds" << std::endl;
        }
    }

    //Fade out BGM over specified duration 
    void Audio::FadeBGMOut(float duration)
    {
        if (currentBGMChannel >= 0 && activeChannelsById.count(currentBGMChannel)) {
            float currentVolume;
            activeChannelsById[currentBGMChannel]->getVolume(&currentVolume);

            fadeStartVolume = currentVolume;
            fadeTargetVolume = 0.0f;
            fadeDuration = duration;
            fadeTimer = 0.0f;
            isFadingOut = true;
            isFadingIn = false;

            // Enable low-pass filter for fade-out effect (optional)
            if (lowPassFilter) {
                lowPassFilter->setBypass(false);
                lowPassFilter->setParameterFloat(FMOD_DSP_LOWPASS_CUTOFF, 22000.0f);
            }
            std::cout << "Starting BGM fade-out over " << duration << " seconds" << std::endl;
        }
    }

    //Crossfade from current BGM to new BGM
    void Audio::CrossfadeBGM(const std::string& newBGM, float duration)
    {
        // Start new BGM at zero volume
        int newChannelId = PlaySound3D(newBGM, Vector3(0, 0, 0), Vector3(0, 0, 0),
            0.0f, AudioSourceType::STEREO_2D);

        if (newChannelId >= 0) {
            // Store as pending BGM
            pendingBGMChannel = newChannelId;

            // If there's a current BGM, fade it out
            if (currentBGMChannel >= 0) {
                FadeBGMOut(duration);
            }
            else {
                // No current BGM, just fade in the new one
                currentBGMChannel = newChannelId;
                pendingBGMChannel = -1;
                FadeBGMIn(duration);
            }
            std::cout << "Crossfading to BGM: " << newBGM << " over " << duration << " seconds" << std::endl;
        }
    }

    //Set reverb properties for environmental effects 
    void Audio::SetReverbProperties(float decayTime, float wetLevel, float dryLevel)
    {
        if (reverbDSP) {
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, decayTime);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, wetLevel);
            reverbDSP->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, dryLevel);

            std::cout << "Reverb properties updated - Decay: " << decayTime
                << "s, Wet: " << wetLevel << "dB, Dry: " << dryLevel << "dB" << std::endl;
        }
    }

    void Audio::UE_PauseSound(const std::string& customName)
    {
        auto it = activeChannels.find(customName);      // Find the Channel based on the customName
        if (it != activeChannels.end())
        {
            bool isPaused;
            it->second->getPaused(&isPaused);           // Get the current paused state
            it->second->setPaused(!isPaused);           // Toggle the paused state
            std::cout << (isPaused ? "Resuming " : "Pausing ") << customName << std::endl;
        }
        else
        {
            std::cerr << "Error: Channel for " << customName << " not found!" << std::endl;
        }
    }

    void Audio::UE_SetVolume(const std::string& customName, float volume)
    {
        auto it = activeChannels.find(customName);              // Find the channel associated with the custom sound name

        if (it != activeChannels.end() && it->second != nullptr)
        {
            FMOD::Channel* pChannel = it->second;               // Retrieve the Channel from customName 
            pChannel->setVolume(volume);                        // Set the volume on the channel (0.0f is silent, 1.0f is max volume)
            std::cout << "Volume for " << customName << " set to " << volume << std::endl;
        }
        else
        {
            std::cerr << "Error: Channel for " << customName << " not found!" << std::endl;
        }
    }

    void Audio::UE_SetGroupVolume(FMOD::ChannelGroup* group, float volume)
    {
        if (group != nullptr)
        {
            group->setVolume(volume);       // Set the volume of the group to the given volume
            std::cout << "Set volume for channel group to " << volume << std::endl;
        }
    }

    void Audio::UE_PauseGroup(FMOD::ChannelGroup* group)
    {
        if (group != nullptr)
        {
            bool isPaused;
            group->getPaused(&isPaused);    // Retrieve the pause state for the group
            group->setPaused(!isPaused);    // Toggle pause state for the group
            std::cout << (isPaused ? "Resuming group." : "Pausing group.") << std::endl;
        }
    }

    void Audio::UE_CreateChannelGroup(const std::string& groupName)
    {
        FMOD::ChannelGroup* newGroup = nullptr;                         
        pSystem->createChannelGroup(groupName.c_str(), &newGroup);      //  Create a new ChannelGroup with given groupName
        if (newGroup != nullptr)
        {
            activeChannelGroup[groupName] = newGroup;                   // Store newGroup in Map of activeChannelGroup
            //std::cout << "Channel group '" << groupName << "' created successfully." << std::endl;
        }
    }

    void Audio::IncrementGroupVol(const std::string& groupName)
    {
        auto it = activeChannelGroup.find(groupName);               // Search for GroupName
        if (it != activeChannelGroup.end())                         // Iterate through the activeChannelGroups
        {
            FMOD::ChannelGroup* group = it->second;                 // Retrieve the group
            float currentVolume;                                    // Create float to hold currentVolume
            FMOD_RESULT result = group->getVolume(&currentVolume);  // Retrieve the currentVolume of the group
            if (result == FMOD_OK) 
            {
                float newVolume = currentVolume + volChangeAmount;  // Increment the group's volume
                newVolume = (newVolume > 1.0f) ? 1.0f : newVolume;  // Ensure volume does not exceed 1.0 (max volume)
                group->setVolume(newVolume);                        // Set the group with the newVolume
                std::cout << "Increased volume of " << groupName << " to " << newVolume << std::endl;
            }
            else 
            {
                std::cout << "Error getting volume for group " << groupName << std::endl;
            }
        }
        else 
        {
            std::cout << "Channel group " << groupName << " not found." << std::endl;
        }
    }

    void Audio::DecrementGroupVol(const std::string& groupName)
    {
        auto it = activeChannelGroup.find(groupName);               // Search for groupName
        if (it != activeChannelGroup.end())                         // Iterate through the activeChannelGroups
        {
            FMOD::ChannelGroup* group = it->second;                 // Retrieve the group

            float currentVolume;                                    // Create float to hold currentVolume
            FMOD_RESULT result = group->getVolume(&currentVolume);  // Retrieve the currentVolume of the group          
            if (result == FMOD_OK) 
            {
                float newVolume = currentVolume - volChangeAmount;  // Decrement the group's volume

                newVolume = (newVolume < 0.0f) ? 0.0f : newVolume;  // Ensure volume does not fall below 0.0 (min volume)
                group->setVolume(newVolume);                        // Set the group with the newVolume

                std::cout << "Decreased volume of " << groupName << " to " << newVolume << std::endl;
            }
            else 
            {
                std::cout << "Error getting volume for group " << groupName << std::endl;
            }
        }
        else 
        {
            std::cout << "Channel group " << groupName << " not found." << std::endl;
        }
    }

    void Audio::PauseAllAudio()
    {
        for (auto& channelEntry : activeChannels)   // Iterate through all active channel
        {
            FMOD::Channel* pChannel = channelEntry.second;
            if (pChannel != nullptr)
            {
                bool isPlaying = false;
                pChannel->isPlaying(&isPlaying);    // Check if channel is playing
                if (isPlaying)
                {
                    pChannel->setPaused(true);      // Pause the channel if its playing
                }
            }
        }
    }

    void Audio::ResumeAllAudio()
    {
        for (auto& channelEntry : activeChannels)  // Iterate through all active channels
        {
            FMOD::Channel* pChannel = channelEntry.second;
            if (pChannel != nullptr)
            {
                bool isPaused = false;
                pChannel->getPaused(&isPaused);    // Check if the channel is paused
                if (isPaused)
                {
                    pChannel->setPaused(false);    // Unpause the channel if it's paused
                }
            }
        }
    }

    void Audio::Reset()
    {
        PauseAllAudio();                    // Pause any current playing audio

        // Iterate through active channels and stop/rewind each
        for (auto& pair : activeChannels)
        {
            FMOD::Channel* channel = pair.second;
            if (channel != nullptr)
            {
                channel->stop();  // Stop the sound
                channel->setPosition(0, FMOD_TIMEUNIT_MS);  // Rewind to the beginning
            }
        }
        activeChannels.clear();  // Clear active channels but keep loaded sounds
        std::cout << "Audio system has been reset by stopping and rewinding sounds." << std::endl;
    }
}