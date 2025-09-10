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

        // Create Channel Groups on Initialization
        UE_CreateChannelGroup("BackgroundMusic");
        UE_CreateChannelGroup("SoundEffects");
    }

    // Destructor
    Audio::~Audio() 
    {
        //Note: FMOD will automatically 
        // 
        // 
        // 
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
    }

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
        AudioAsset* audioAsset = GlobalAssetManager.GetAudioAsset(customName);
        if (!audioAsset)
        {
            std::cout << "Error: AudioAsset not found for customName: " << customName << std::endl;
            return nullptr;
        }
        AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(customName);
        if (!musicAsset)
        {
            std::cout << "Error: MusicAsset not found for customName: " << customName << std::endl;
            return nullptr;
        }
        std::string filePath = musicAsset->filePath;
        std::string modeString = musicAsset->mode;
        std::cout << "File path: " << filePath << std::endl;
        std::cout << "Mode: " << modeString << std::endl;
        //std::string filePath = audioAsset->UE_GetMusicFilePath(customName);        // Retrieve the filePath from AssetManager
        //std::string modeString = audioAsset->UE_GetMusicMode(customName);          // Retrieve the modeString from AssetManager

        FMOD_MODE mode = UE_GetModeFromString(modeString);                                      // Convert the string mode to FMOD_MODE
        std::cout << "FMOD Mode: " << mode << std::endl;
        FMOD_RESULT result = pSystem->createSound(filePath.c_str(), mode, nullptr, &pSound);    // Create Sound

        if (result != FMOD_OK)
        {
            std::cout << "Error, Fail to create audio for " << customName << " " << result << std::endl;
        }

        loadedSounds[customName] = pSound;      // Store the created sound in loadedSounds Map, with customName as key

        return pSound;                          // Return the created sound
    }

    void Audio::UE_PlaySound(const std::string& customName)
    {
        Sound* pSound = UE_LoadSound(customName);                               // Load the sound with customName as key
        AudioAsset* audioAsset = GlobalAssetManager.GetAudioAsset(customName);
        AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(customName);
        SoundType soundType = musicAsset->soundType;

        auto it = activeChannels.find(customName);                      // Check if there is already an active channel for this customName
        if (it != activeChannels.end() && it->second != nullptr)        // If activeChannel for the key exists
        {
            bool isPlaying = false;
            it->second->isPlaying(&isPlaying);
            if (isPlaying)
            {
                std::cout << "Audio " << customName << " is already playing." << std::endl;
                return;     // If it's already playing, return and do nothing
            }
            else
            {
                bool isPaused = false;
                it->second->getPaused(&isPaused);

                if (isPaused)
                {
                    // The sound is paused, so resume it
                    std::cout << "Resuming audio " << customName << "." << std::endl;
                    it->second->setPaused(false);  // Unpause the channel
                    return;
                }
                else
                {
                    // If the sound is not paused, it has ended, so reset the channel
                    std::cout << "Audio " << customName << " has finished, replaying." << std::endl;
                    activeChannels.erase(it);  // Remove the channel reference to allow re-playing
                }
            }
        }

        // Note: Channel will be considered playing when System::playSound is called
        if (pSound != nullptr)          // If activeChannel for key does not exists
        {
            FMOD::Channel* pChannel = nullptr;
            FMOD_RESULT result = pSystem->playSound(pSound, nullptr, false, &pChannel);

            if (result != FMOD_OK)
            {
                std::cout << "Error, cannot play audio";
                return;
            }

            if (pChannel != nullptr)
            {
                // set the appropriate channel group based on soundtype
                if (soundType == BACKGROUND_MUSIC)
                {
                    pChannel->setChannelGroup(activeChannelGroup["BackgroundMusic"]);
                }
                else if (soundType == SOUND_EFFECT)
                {
                    pChannel->setChannelGroup(activeChannelGroup["SoundEffects"]);
                }

                activeChannels[customName] = pChannel;      // Store channel associated with the custom audio name
                std::cout << "Playing audio " << customName << "." << std::endl;
            }
        }
        else
        {
            std::cout << "Error: Sound " << customName << " could not be loaded." << std::endl;
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