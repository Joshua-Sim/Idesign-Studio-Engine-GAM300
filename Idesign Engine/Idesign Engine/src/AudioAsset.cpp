///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AudioAsset.cpp
/// @Brief : Source file implementing the `AudioAsset` class, which manages 
///          the loading, deserialization, and retrieval of audio asset data. 
///          It includes functions for converting playback modes, retrieving 
///          sound information, and handling audio asset paths. The class 
///          interacts with external assets to organize and manage various 
///          audio data for efficient use in the audio system.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "AudioAsset.h"

AudioAsset::AudioAsset()
{
}

AudioAsset::AudioAsset(const std::string& filePath)
{
    //DeserializeAudio(filePath);
}

AudioAsset::~AudioAsset() {}

void AudioAsset::DeserializeAudio(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Error: Could not open JSON file: " << filePath << std::endl;
        throw std::runtime_error("Could not open JSON file.");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();                 // Read the entire file into the buffer
    std::string jsonString = buffer.str();  // Get the content as a string

    rapidjson::Document document;           // Parse the JSON string
    document.Parse(jsonString.c_str());

    // Check for parsing errors
    if (document.HasParseError())
    {
        std::cerr << "Error parsing JSON: " << rapidjson::GetParseError_En(document.GetParseError()) << std::endl;
        return;
    }

    // Extract music assets from JSON
    if (document.HasMember("musicAssets") && document["musicAssets"].IsArray())
    {
        const rapidjson::Value& musicArray = document["musicAssets"];
        for (rapidjson::SizeType i = 0; i < musicArray.Size(); i++)
        {
            if (musicArray[i].IsObject())
            {
                const rapidjson::Value& musicObject = musicArray[i];

                std::string customName = musicObject["customName"].GetString();
                std::string filePath = musicObject["filePath"].GetString();
                std::string mode = musicObject["mode"].GetString();
                std::string soundTypeStr = musicObject["soundType"].GetString();

                Framework::Audio::SoundType soundType = UE_GetSoundTypeFromString(soundTypeStr);

                // Add to musicAssets
                musicAssets[customName] = { filePath, mode, soundType };

                // Store the custom name in the customNames vector
                customNames.push_back(customName);

                std::cout << "Loaded asset: " << customName << " from JSON." << std::endl;
            }
        }
    }
    else
    {
        std::cerr << "Invalid JSON structure: 'musicAssets' array not found." << std::endl;
    }

    file.close();
}

void AudioAsset::SerializeAudio(const std::string& filePath)
{
    // Create a JSON document
    rapidjson::Document document;
    document.SetObject();

    // Add a JSON array for musicAssets
    rapidjson::Value musicArray(rapidjson::kArrayType);

    // Add each asset in musicAssets to the JSON array
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
    for (const auto& [customName, asset] : musicAssets)
    {
        rapidjson::Value musicObject(rapidjson::kObjectType);

        // Add fields to the musicObject
        musicObject.AddMember("customName", rapidjson::Value(customName.c_str(), allocator), allocator);
        musicObject.AddMember("filePath", rapidjson::Value(asset.filePath.c_str(), allocator), allocator);
        musicObject.AddMember("mode", rapidjson::Value(asset.mode.c_str(), allocator), allocator);

        // Convert SoundType to string
        std::string soundTypeStr = SoundTypeToString(asset.soundType);
        musicObject.AddMember("soundType", rapidjson::Value(soundTypeStr.c_str(), allocator), allocator);

        // Add the musicObject to the musicArray
        musicArray.PushBack(musicObject, allocator);
    }

    // Add the musicArray to the document
    document.AddMember("musicAssets", musicArray, allocator);

    // Write the document to a JSON file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    // Write the buffer to a file
    std::ofstream outFile(filePath);
    if (outFile.is_open())
    {
        outFile << buffer.GetString(); // Write the JSON string to the file
        outFile.close();
        std::cout << "Successfully serialized audio assets to " << filePath << std::endl;
    }
    else
    {
        std::cerr << "Error: Could not open file for writing: " << filePath << std::endl;
    }
}

Framework::Audio::SoundType AudioAsset::UE_GetSoundTypeFromString(const std::string& soundTypeStr) const
{
    if (soundTypeStr == "background")
    {
        return Framework::Audio::BACKGROUND_MUSIC;
    }
    else if (soundTypeStr == "effect")
    {
        return Framework::Audio::SOUND_EFFECT;
    }
    else
    {
        throw std::runtime_error("Invalid sound type: " + soundTypeStr);
    }
}

std::string AudioAsset::SoundTypeToString(Framework::Audio::SoundType soundType)
{
    switch (soundType)
    {
    case Framework::Audio::SoundType::BACKGROUND_MUSIC:
        return "background";
    case Framework::Audio::SoundType::EMPTY:
        return "Empty";
    case Framework::Audio::SoundType::SOUND_EFFECT:
        return "effect";
    default:
        return "Unknown";
    }
}

std::string AudioAsset::UE_GetMusicFilePath(const std::string& customName) const
{
    auto it = musicAssets.find(customName);     // Find the customName in musicAssets
    if (it != musicAssets.end())
    {
        return it->second.filePath;             // Retrieve the filepath of the customName
    }
    else
    {
        throw std::runtime_error("Music asset not found.");
    }
}

std::string AudioAsset::UE_GetMusicMode(const std::string& customName) const
{
    auto it = musicAssets.find(customName);     // Find the customName in musicAssets
    if (it != musicAssets.end())
    {
        return it->second.mode;                 // Retrieve the mode of the customName
    }
    else
    {
        throw std::runtime_error("Music asset not found.");
    }
}

Framework::Audio::SoundType AudioAsset::UE_GetMusicSoundType(const std::string& customName) const
{
    auto it = musicAssets.find(customName);     // Find the customName in musicAssets
    if (it != musicAssets.end())
    {
        return it->second.soundType;            // Retrieve the soundType of the customName
    }
    else
    {
        throw std::runtime_error("Music asset not found.");
    }
}

std::string AudioAsset::GetAssetName(const std::string& filePath) const
{
    for (const auto& pair : musicAssets)
    {
        if (pair.second.filePath == filePath)
        {
            return pair.first;  // return custom name associated with this file path
        }
    }
    return "";  // Return empty string if not found
}

AudioAsset::MusicAsset* AudioAsset::GetMusicAssetByName(const std::string& customName)
{
    auto it = musicAssets.find(customName);
    if (it != musicAssets.end())
    {
        return &it->second; // Return a pointer to the found MusicAsset
    }
    return nullptr; // Not found
}

std::vector<std::string> AudioAsset::GetAllCustomNames() const
{
    std::vector<std::string> customNames;
    for (const auto& pair : musicAssets)
    {
        customNames.push_back(pair.first);  // Add each custom name to the list
    }
    return customNames;
}

void AudioAsset::AddAudio(const std::string& name, const std::string& path)
{
    // Check if the asset with the same name already exists
    if (musicAssets.find(name) != musicAssets.end())
    {
        std::cerr << "Audio with the name " << name << " already exists." << std::endl;
        return;  // Do nothing if the audio already exists
    }

    // If not, create a new MusicAsset and store it
    MusicAsset newAudio{ path, "loop", Framework::Audio::SoundType::BACKGROUND_MUSIC };  // Default values for mode and sound type
    musicAssets[name] = newAudio;  // Insert the new audio asset

    // Optionally, store the customName for later use (if necessary)
    customNames.push_back(name);

    std::cout << "Added audio asset: " << name << std::endl;
}

void AudioAsset::UpdateAudioNameAndPath(const std::string& oldName, const std::string& newName, const std::string& newPath)
{
    // Check if the asset with oldName exists
    auto it = musicAssets.find(oldName);
    if (it != musicAssets.end())
    {
        // Update the music asset's name and file path
        MusicAsset& musicAsset = it->second;
        musicAsset.filePath = newPath;  // Update file path

        std::cout << "Custom Name: " << newName << std::endl;
        std::cout << "File Path: " << musicAsset.filePath << std::endl;
        std::cout << "Playback Mode: " << musicAsset.mode << std::endl;

        std::cout << "Old Name Length: " << oldName.size() << std::endl;
        std::cout << "New Name Length: " << newName.size() << std::endl;

        // Must handle key, if not will cause issues
        customNames.erase(std::remove(customNames.begin(), customNames.end(), oldName), customNames.end());
        customNames.push_back(newName);

        //// Insert a new key-value pair with the updated name
        auto nodeHandle = musicAssets.extract(it); // Detach the node
        nodeHandle.key() = newName;                // Update the key
        musicAssets.insert(std::move(nodeHandle)); // Reinsert with the new key

        std::cout << "Audio asset name and path updated from " << oldName << " to " << newName << std::endl;
    }
    else
    {
        std::cerr << "Error: Audio asset not found with the name " << oldName << std::endl;
    }
}

void AudioAsset::RemoveAudio(const std::string& name)
{
    // Step 1: Locate the audio asset
    auto it = customNames.begin();
    while (it != customNames.end())
    {
        if (*it == name) // Check if the name matches
        {
            // Step 2: Remove from the customNames list
            it = customNames.erase(it); // Erase and update iterator
            std::cout << "Audio '" << name << "' removed from custom names." << std::endl;
        }
        else
        {
            ++it; // Move to the next item
        }
    }

    // Step 3: Remove from any other relevant data structures
    auto musicIt = musicAssets.find(name);
    if (musicIt != musicAssets.end())
    {
        musicAssets.erase(musicIt); // Erase from the musicAssets map
        std::cout << "Audio '" << name << "' removed from music assets." << std::endl;
    }
}

