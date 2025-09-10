///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AudioAsset.h
/// @Brief : Header file defining the `AudioAsset` class, which manages audio
///          asset data. This class includes functionalities for loading, 
///          serializing, and retrieving audio asset information, including 
///          file paths, playback modes, and sound types. It is used to 
///          handle and organize audio assets within the application.
///          The class stores audio information in a structured format for 
///          easy retrieval and management.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _AUDIO_ASSET_H_
#define _AUDIO_ASSET_H_
#include "pch.h"
#include "JsonSerialize.h"
#include <Audio.h>

class AudioAsset
{
public:

    /**
        *   @struct MusicAsset
        *   @brief Struct to store music asset data including file path, mode, and
        *          sound type.
        */
    struct MusicAsset
    {
        std::string filePath;                                                                   // Path to music file
        std::string mode = "oneshot";                                                           // Playback mode
        Framework::Audio::SoundType soundType = Framework::Audio::SoundType::SOUND_EFFECT;      // Type of sound
    };

    AudioAsset();

    /**
     * @brief Constructor that initializes the AudioAsset with the given file path.
     * @param filePath Path to the audio file.
     */
    AudioAsset(const std::string& filePath);
    
    /**
     * @brief Destructor for AudioAsset. Cleans up any resources used by the audio assets.
     */
    ~AudioAsset();

    /**
     * @brief Deserializes the audio asset data from a specified file path.
     * @param filePath Path to the audio asset file containing JSON or other serialized data.
     */
    void DeserializeAudio(const std::string& filePath);

    void SerializeAudio(const std::string& filePath);

    /**
     * @brief Converts a string representing a sound type to its corresponding enumeration value.
     * @param soundTypeStr The string representing the sound type (e.g., "music", "sfx").
     * @return The corresponding SoundType enumeration value.
     */
    Framework::Audio::SoundType UE_GetSoundTypeFromString(const std::string& soundTypeStr) const;
    
    std::string SoundTypeToString(Framework::Audio::SoundType soundType);

    /**
     * @brief Retrieves the file path for a music asset identified by its custom name.
     * @param customName The custom name associated with the music asset.
     * @return The file path of the music asset.
     */
    std::string UE_GetMusicFilePath(const std::string& customName) const;
    
    /**
     * @brief Retrieves the playback mode for a music asset identified by its custom name.
     * @param customName The custom name associated with the music asset.
     * @return The playback mode of the music asset (e.g., loop, once).
     */
    std::string UE_GetMusicMode(const std::string& customName) const;
    
    /**
     * @brief Retrieves the sound type for a music asset identified by its custom name.
     * @param customName The custom name associated with the music asset.
     * @return The SoundType enumeration value of the music asset.
     */
    Framework::Audio::SoundType UE_GetMusicSoundType(const std::string& customName) const;
    
    /**
     * @brief Generates a unique asset name based on the given file path.
     * @param filePath The path to the audio file.
     * @return The generated asset name.
     */
    std::string GetAssetName(const std::string& filePath) const;
    
    /**
     * @brief Retrieves the music asset data by custom name.
     * @param customName The custom name of the music asset.
     * @return A pointer to the corresponding MusicAsset object.
     */
    MusicAsset* GetMusicAssetByName(const std::string& customName);

    /**
     * @brief Retrieves a list of all custom names for the stored music assets.
     * @return A vector of strings containing all custom names for music assets.
     */
    std::vector<std::string> GetAllCustomNames() const;

    void AddAudio(const std::string& name, const std::string& path);
    
    void UpdateAudioNameAndPath(const std::string& oldName, const std::string& newName, const std::string& newPath);

    void RemoveAudio(const std::string& name);

    std::vector<std::string> customNames;  // Added to store custom names
private:
    std::unordered_map<std::string, MusicAsset> musicAssets;    // To store all the music assets(key:custom name, value: filepath etc)
};
#endif // !_AUDIO_ASSET_H_