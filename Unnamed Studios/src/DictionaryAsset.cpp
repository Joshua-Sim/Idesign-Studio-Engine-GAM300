///////////////////////////////////////////////////////////////////////////////
///
///	@File  : DictionaryAsset.cpp
/// @Brief : Defines the `Dictionary` class, which manages a collection of words 
///          and prefixes loaded from files. Provides functionality for loading 
///          and accessing these words and prefixes for application usage. This
///          class supports loading dictionary and prefix data from JSON files,
///          and provides functionality to retrieve random prefixes.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "DictionaryAsset.h"

/**
 * @brief Constructs a Dictionary object and loads dictionary and prefix data from the specified files.
 * @param dictionaryPath Path to the JSON file containing the dictionary words.
 * @param prefixPath Path to the JSON file containing the prefixes.
 */
Dictionary::Dictionary(const std::string& dictionaryPath, const std::string& prefixPath)
{
    DeserializeDictionary(dictionaryPath);
    DeserializePrefixes(prefixPath);
}

/**
 * @brief Destructor for the Dictionary class. Currently, no dynamic memory is allocated,
 *        so the destructor is empty.
 */
Dictionary::~Dictionary() {}

/**
 * @brief Deserializes the dictionary words from a JSON file.
 *        This function reads the file, parses the JSON structure, and stores the words
 *        into an unordered set.
 * @param dictionaryPath Path to the JSON file containing dictionary words.
 */
void Dictionary::DeserializeDictionary(const std::string& dictionaryPath)
{
    std::ifstream file(dictionaryPath);
    if (!file.is_open()) 
    {
        std::cerr << "Failed to open file: " << dictionaryPath << std::endl;
        return;
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

    // Check if the JSON structure is correct and extract words
    if (document.HasMember("words") && document["words"].IsArray()) 
    {
        const rapidjson::Value& wordsArray = document["words"];
        for (rapidjson::SizeType i = 0; i < wordsArray.Size(); i++) 
        {
            if (wordsArray[i].IsString()) 
            {
                dictionary.insert(wordsArray[i].GetString());
            }
        }
        std::cout << "Loaded dictionary successfully. Total words: " << dictionary.size() << std::endl;
    }
    else 
    {
        std::cerr << "Invalid JSON structure: 'words' array not found." << std::endl;
    }
}

/**
 * @brief Deserializes the prefixes from a JSON file.
 *        This function reads the file, parses the JSON structure, and stores the prefixes
 *        into a vector after removing any spaces.
 * @param prefixPath Path to the JSON file containing prefixes.
 */
void Dictionary::DeserializePrefixes(const std::string& prefixPath)
{
    std::ifstream file(prefixPath);
    if (!file.is_open()) 
    {
        std::cerr << "Failed to open file: " << prefixPath << std::endl;
        return;
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

    // Check if the JSON structure is correct and extract prefixes
    if (document.HasMember("prefixes") && document["prefixes"].IsArray()) 
    {
        const rapidjson::Value& prefixesArray = document["prefixes"];
        for (rapidjson::SizeType i = 0; i < prefixesArray.Size(); i++) 
        {
            if (prefixesArray[i].IsString()) 
            {
                std::string prefix = prefixesArray[i].GetString();
                prefix.erase(remove_if(prefix.begin(), prefix.end(), isspace), prefix.end());       // Optional: Remove spaces
                prefixes.push_back(prefix);
            }
        }
        std::cout << "Loaded prefixes successfully. Total prefixes: " << prefixes.size() << std::endl;
    }
    else 
    {
        std::cerr << "Invalid JSON structure: 'prefixes' array not found." << std::endl;
    }
}

/**
 * @brief Retrieves a random prefix from the list of prefixes.
 *        This method generates a pseudo-random number based on the current time and selects
 *        a prefix from the loaded prefixes.
 * @return A randomly selected prefix from the loaded prefixes.
 * @throws std::runtime_error If no prefixes are loaded.
 */
std::string Dictionary::UE_GetRandomPrefix()
{
    if (prefixes.empty())
    {
        throw std::runtime_error("No prefixes loaded.");
    }

    time_t currentTime = std::time(nullptr);                            // Get current time
    unsigned int timeValue = static_cast<unsigned int>(currentTime);    // Explicit cast

    //std::srand(static_cast<unsigned int>(std::time(nullptr)));        // Seed for random number generation
    int randomIndex = timeValue % prefixes.size();                      // Get a random index within the size of the prefixes
    return prefixes[randomIndex];                                       // Return random prefix
}