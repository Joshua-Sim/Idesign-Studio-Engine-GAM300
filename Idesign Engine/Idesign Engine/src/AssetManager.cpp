///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AssetManager.cpp
/// @Brief : Manages assets, including textures, audio, fonts, dictionaries, 
///          windows, entities, and shaders, with methods to load and retrieve 
///          these assets in the Framework namespace.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "AssetManager.h"
#include "LogicManager.h"

namespace Framework
{
    AssetManager GlobalAssetManager;        // Global instance of AssetManager.
    unsigned char* AssetManager::data{};    // Texture data buffer for loading textures.

    AssetManager::AssetManager() 
    {
        // Initialization of Assets
        //LoadWindow("src/Window.json");                                // Loaded in main.cpp
        //LoadDictionary("src/Data/DictionaryAsset.json", "src/Data/PrefixesAsset.json");
        //LoadEntities("src/Data/EntityAsset.json");
        //LoadEntities("src/entities_1.json");                          // Load in main.cpp
        LoadAudio("src/Data/AudioAsset.json");
        LoadTexture("src/Data/TextureAsset.json");                      // Load textures on initialization
        //LoadFont("../Unnamed Studios/Assets/Font/arial.ttf", 40);     // Loaded in fontsystem
        //LoadFontShader("src/UE_Font.vert");                           // Loaded in fontsystem
        //LoadFontShader("src/UE_Font.frag");                           // Loaded in fontsystem
        //LoadandStoreShader("src/UE.vert");                            // Loaded in Graphics.cpp
        //LoadandStoreShader("src/UE.frag");                            // Loaded in Graphics.cpp
    }

    AssetManager::~AssetManager() {}

    Dictionary& AssetManager::LoadDictionary(const std::string& dictionaryPath, const std::string& prefixPath)
    {
        // Check if the dictionary is already loaded
        auto it = dictionaryAssets.find(dictionaryPath);
        if (it != dictionaryAssets.end())
        {
            return *(it->second);
        }

        // If not, create a new dictionary instance
        auto dictionary = std::make_unique<Dictionary>(dictionaryPath, prefixPath);
        Dictionary& dictionaryRef = *dictionary;
        dictionaryAssets[dictionaryPath] = std::move(dictionary);

        return dictionaryRef;
    }

    void AssetManager::LoadEntities(const std::string& filePath)
    {
        // If not, load it and store it in the container
        auto entityAsset = std::make_unique<EntityAsset>(filePath);
        entityAssets[filePath] = std::move(entityAsset);
    }

    void AssetManager::LoadAudio(const std::string& filePath)
    {
        // First, load the audio using the filePath and create an AudioAsset
        auto audioAsset = std::make_shared<AudioAsset>(filePath);

        // Retrieve all the custom names associated with this AudioAsset
        std::vector<std::string> customNames = audioAsset->GetAllCustomNames();

        // Add the audio asset to the map using the custom names
        for (const auto& customName : customNames)
        {
            if (audioAssets.find(customName) == audioAssets.end())
            {
                audioAssets[customName] = audioAsset;  // Use the shared_ptr for multiple names
                std::cout << "Audio created with custom name: " << customName << "\n";
            }
            else
            {
                std::cout << "Audio already exists with custom name: " << customName << "\n";
            }
        }
    }

    AudioAsset* AssetManager::GetAudioAsset(const std::string& assetName)
    {
        auto it = audioAssets.find(assetName);
        if (it != audioAssets.end())
        {
            //std::cout << "Found " << assetName << std::endl;
            return it->second.get(); // Return the raw pointer
        }
        std::cout << "Error: AudioAsset '" << assetName << "' not found." << std::endl;
        return nullptr; // Not found
    }

    void AssetManager::AddAudio(const std::string& name, const std::string& path)
    {
        // Print input details
        std::cout << "Adding audio:" << std::endl;
        std::cout << " - Name: " << name << std::endl;
        std::cout << " - Source Path: " << path << std::endl;

        // Define the base folder for textures
        std::string targetFolder = "./assets/audio/bgm";  // Fixed folder for storing textures
        std::cout << " - Target Folder: " << targetFolder << std::endl;

        // Extract the file name from the source path
        size_t pos = path.find_last_of("/\\");
        std::string fileName = path.substr(pos + 1);  // Get only the file name
        std::string targetPath = targetFolder + "/" + fileName;  // Full path in the target folder
        std::cout << " - Extracted File Name: " << fileName << std::endl;
        std::cout << " - Target Texture Path: " << targetPath << std::endl;

        // Copy the texture to the target folder
        if (!CopyAudioToFolder(path, targetFolder))
        {
            std::cerr << "Failed to copy audio to target folder." << std::endl;
            return;
        }
        std::cout << "Audio successfully copied to target folder." << std::endl;

        // Check if the audio already exists in the map using the name
        auto it = audioAssets.find(name);
        if (it == audioAssets.end())
        {
            // Create a new AudioAsset and load it from the file
            auto audioAsset = std::make_shared<AudioAsset>();  // Create a new shared pointer for AudioAsset

            // Add the new music asset to the audio asset
            audioAsset->AddAudio(name, targetPath);  // Now calling AddAudio on the new AudioAsset

            // Move the newly created AudioAsset into the map
            audioAssets[name] = audioAsset;  // Use std::move to transfer ownership of the shared_ptr
        
            audioAsset->SerializeAudio("src/Data/TempAudioAsset.json");

            MergeSerializeAudio("src/Data/AudioAsset.json", "src/Data/TempAudioAsset.json");
        }
        else
        {
            // If the AudioAsset already exists, just add the new MusicAsset to it
            it->second->AddAudio(name, path);  // Access the existing AudioAsset and add the new MusicAsset
        }
    }

    void AssetManager::MergeSerializeAudio(const std::string& mainFilePath, const std::string& tempFilePath)
    {
        rapidjson::Document mainDocument;
        rapidjson::Document tempDocument;

        // Load the main file (or create a new one if it doesn't exist)
        std::ifstream mainFile(mainFilePath);
        if (mainFile.is_open())
        {
            std::stringstream buffer;
            buffer << mainFile.rdbuf();
            std::string jsonString = buffer.str();
            mainDocument.Parse(jsonString.c_str());
        }
        else
        {
            mainDocument.SetObject();  // Create a new document if the file doesn't exist
        }

        // Load the temporary file (or create a new one if it doesn't exist)
        std::ifstream tempFile(tempFilePath);
        if (tempFile.is_open())
        {
            std::stringstream buffer;
            buffer << tempFile.rdbuf();
            std::string jsonString = buffer.str();
            tempDocument.Parse(jsonString.c_str());
        }
        else
        {
            tempDocument.SetObject();  // Create a new document if the file doesn't exist
        }

        // Get or create the musicAssets arrays in both documents
        if (!mainDocument.HasMember("musicAssets"))
        {
            mainDocument.AddMember("musicAssets", rapidjson::Value(rapidjson::kArrayType), mainDocument.GetAllocator());
        }
        rapidjson::Value& mainMusicAssets = mainDocument["musicAssets"];

        if (!tempDocument.HasMember("musicAssets"))
        {
            tempDocument.AddMember("musicAssets", rapidjson::Value(rapidjson::kArrayType), tempDocument.GetAllocator());
        }
        rapidjson::Value& tempMusicAssets = tempDocument["musicAssets"];

        // Merge the arrays
        for (rapidjson::SizeType i = 0; i < tempMusicAssets.Size(); i++)
        {
            mainMusicAssets.PushBack(tempMusicAssets[i], mainDocument.GetAllocator());
        }

        // Serialize the merged data to the main file
        std::ofstream outFile(mainFilePath);
        if (outFile.is_open())
        {
            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            mainDocument.Accept(writer);

            outFile << buffer.GetString();  // Write the JSON string to the main file
            outFile.close();
            std::cout << "Successfully merged and serialized audio assets to " << mainFilePath << std::endl;
        }
        else
        {
            std::cerr << "Failed to open file for writing: " << mainFilePath << std::endl;
        }
    }

    bool AssetManager::CopyAudioToFolder(const std::string& sourceFilePath, const std::string& targetFolder)
    {
        // Extract the filename from the source path
        size_t pos = sourceFilePath.find_last_of("/\\");
        std::string fileName = sourceFilePath.substr(pos + 1);

        // Construct the target file path
        std::string targetFilePath = targetFolder + "\\" + fileName;

        // Open the source file for reading in binary mode
        std::ifstream sourceFile(sourceFilePath, std::ios::binary);
        if (!sourceFile.is_open()) {
            std::cerr << "Failed to open source file: " << sourceFilePath << std::endl;
            return false;
        }

        // Open the target file for writing in binary mode
        std::ofstream targetFile(targetFilePath, std::ios::binary);
        if (!targetFile.is_open()) {
            std::cerr << "Failed to create target file: " << targetFilePath << std::endl;
            return false;
        }

        // Copy the file contents
        targetFile << sourceFile.rdbuf();

        std::cout << "Audio file copied successfully to: " << targetFilePath << std::endl;
        return true;
    }

    bool AssetManager::DeleteAudioFile(const std::string& filePath)
    {
        if (std::remove(filePath.c_str()) == 0) 
        {
            std::cout << "File deleted successfully: " << filePath << std::endl;
            return true;
        }
        else {
            std::cerr << "Failed to delete file: " << filePath << std::endl;
            return false;
        }
    }

    void AssetManager::UpdateAudioName(const std::string& oldName, const std::string& newName)
    {
        if (oldName == newName || newName.empty())
        {
            std::cerr << "Invalid or unchanged audio name." << std::endl;
            return;
        }

        auto it = audioAssets.find(oldName);
        if (it == audioAssets.end())
        {
            std::cerr << "AudioAsset not found for name: " << oldName << std::endl;
            return;
        }

        auto& audioAsset = it->second;

        // Get the MusicAsset corresponding to the oldName
        AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(oldName);
        if (!musicAsset)
        {
            std::cerr << "MusicAsset not found for audio name: " << oldName << std::endl;
            return;
        }

        // Extract folder path and file extension
        std::string currentPath = musicAsset->filePath;
        size_t lastSlash = currentPath.find_last_of("/\\");
        std::string folderPath = (lastSlash != std::string::npos) ? currentPath.substr(0, lastSlash + 1) : "";
        size_t dotPos = currentPath.find_last_of(".");
        std::string fileExtension = (dotPos != std::string::npos) ? currentPath.substr(dotPos) : "";

        // Construct new file path
        std::string oldFilePath = folderPath + oldName + fileExtension;
        std::string newFilePath = folderPath + newName + fileExtension;

        // Rename the file on disk
        if (std::rename(oldFilePath.c_str(), newFilePath.c_str()) != 0)
        {
            std::cerr << "Error renaming file from " << oldFilePath << " to " << newFilePath << std::endl;
            return;
        }
        std::cout << "File renamed successfully from " << oldFilePath << " to " << newFilePath << std::endl;

        // Update the MusicAsset with the new file path
        musicAsset->filePath = newFilePath;

        // Ensure the new name does not already exist in the map
        if (audioAssets.find(newName) != audioAssets.end())
        {
            std::cerr << "New name already exists in the map: " << newName << std::endl;
            return;
        }

        // Use nodeHandle to update the map's key efficiently
        auto nodeHandle = audioAssets.extract(it); // Extract the old entry
        nodeHandle.key() = newName;               // Update the key
        audioAssets.insert(std::move(nodeHandle)); // Reinsert with the new key

        // Update the internal audio asset data
        audioAsset->UpdateAudioNameAndPath(oldName, newName, newFilePath);

        // Serialize the updated asset
        audioAsset->SerializeAudio("src/Data/AudioAsset.json");
        std::cout << "Audio asset updated and serialized after renaming." << std::endl;
    }

    void AssetManager::DeleteAudio(const std::string& name)
    {
        // Check if the audio asset exists
        auto it = audioAssets.find(name);
        if (it == audioAssets.end())
        {
            std::cerr << "Error: Cannot delete audio. Asset with name '" << name << "' not found." << std::endl;
            return;
        }

        // Get the file path of the audio asset (optional, if you need to delete the file too)
        AudioAsset* audioAsset = it->second.get();
        AudioAsset::MusicAsset* musicAsset = audioAsset->GetMusicAssetByName(name);
        if (musicAsset)
        {
            std::string filePath = musicAsset->filePath;

            // (Optional) Remove the file from disk
            if (std::remove(filePath.c_str()) != 0)
            {
                std::cerr << "Error: Failed to remove file '" << filePath << "' from disk." << std::endl;
            }
            else
            {
                std::cout << "File '" << filePath << "' removed from disk successfully." << std::endl;
            }
        }

        // remove audio
        audioAsset->RemoveAudio(name);

        // Remove the asset from the map
        audioAssets.erase(it);

        // Serialize the updated state to persist changes
        audioAsset->SerializeAudio("src/Data/AudioAsset.json");

        std::cout << "Audio asset '" << name << "' deleted successfully." << std::endl;
    }

    void AssetManager::LoadTexture(const std::string& filePath)
    {
        // Create a single TextureAsset from the file path using a shared pointer
        auto textureAsset = std::make_shared<TextureAsset>();

        textureAsset->Deserialize("src/Data/TextureAsset.json");

        // Retrieve all texture names in this asset
        std::vector<std::string> textureNames = textureAsset->GetNames();

        // Insert the texture asset into the map for each texture name
        for (const auto& textureName : textureNames)
        {
            // If a texture with the same name doesn't already exist, add it
            if (textureAssets.find(textureName) == textureAssets.end())
            {
                textureAssets[textureName] = textureAsset;  // Shared ownership
                //std::cout << "Texture created with name: " << textureName << "\n";
            }
            else
            {
                std::cout << "Texture already exists with name: " << textureName << "\n";
            }
        }
    }

    TextureAsset* AssetManager::GetTexture(const std::string& assetName)
    {
        // Check if the texture exists in the map
        auto it = textureAssets.find(assetName);
        if (it != textureAssets.end()) {
            std::cout << "Texture found: " << assetName << std::endl;
            return it->second.get(); // Return pointer to the existing TextureAsset
        }

        // If not found, you can throw an exception or handle the error
        throw std::runtime_error("TextureAsset '" + assetName + "' not found.");
    }

    GLuint AssetManager::GetTextureForGraphics(const std::string& assetName)
    {
        // Find the texture asset by name
        auto it = textureAssets.find(assetName);
        if (it == textureAssets.end())
        {
            std::cerr << "Texture not found: " << assetName << std::endl;
            return 0; // Return 0 if texture is not found
        }

        TextureAsset* textureAsset = it->second.get();  // Retrieve the texture asset

        // Check if the texture ID is already set
        GLuint existingTextureID = textureAsset->GetTextureID(assetName);
        if (existingTextureID != 0)
        {
            // If the texture ID is already set, return it directly
            return existingTextureID;
        }

        // If not loaded, load texture from file and generate a new ID
        std::string filepath = textureAsset->GetPath(assetName);
        int width, height, nrChannels;
        unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &nrChannels, 0);
        if (!data)
        {
            std::cerr << "Failed to load texture from path: " << filepath << std::endl;
            return 0; // Return 0 on failure
        }

        // Generate and bind the texture
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);  // Free the texture data

        // Store the texture ID in the texture asset
        textureAsset->SetTextureID(assetName, textureID);  // Set the texture ID using the name

        return textureID;
    }

    bool AssetManager::RenameTexture(const std::string& oldName, const std::string& newName)
    {
        // Check if the new name already exists in the map
        if (textureAssets.find(newName) != textureAssets.end())
        {
            std::cerr << "Texture with the name " << newName << " already exists!" << std::endl;
            return false;  // Return false if the new name already exists
        }

        auto it = textureAssets.find(oldName);
        if (it != textureAssets.end())
        {
            // Extract the texture asset and remove the old name
            auto textureAsset = it->second;
            textureAssets.erase(it);

            // Insert the texture with the new name
            textureAssets[newName] = textureAsset;

            std::cout << "Texture renamed from " << oldName << " to " << newName << std::endl;
            return true;  // Successfully renamed
        }
        else
        {
            std::cerr << "Texture with the name " << oldName << " not found!" << std::endl;
            return false;  // Return false if the texture was not found
        }
    }

    void AssetManager::UpdateTextureName(const std::string& oldName, const std::string& newName)
    {
        // Check if the texture exists under the old name
        auto it = textureAssets.find(oldName);
        if (it != textureAssets.end())
        {
            // Access the TextureAsset via the shared_ptr
            auto textureAsset = it->second;

            // Extract the old path and extension
            std::string oldPath = textureAsset->GetPath(oldName);  // Get the old path from texture asset
            size_t pos = oldPath.find_last_of("/\\");  // Find the last directory separator
            std::string pathWithoutName = oldPath.substr(0, pos + 1);  // Get everything before the filename
            std::string fileExtension = oldPath.substr(oldPath.find_last_of("."));  // Get the file extension

            // Construct the new path using the old directory and the new name
            std::string newPath = pathWithoutName + newName + fileExtension;  // Combine new name with extension

            // Rename the actual file on disk using the standard C rename function (Linux/Unix)
            if (rename(oldPath.c_str(), newPath.c_str()) == 0)
            {
                std::cout << "File renamed from " << oldPath << " to " << newPath << std::endl;
            }
            else
            {
                std::perror("Failed to rename file");  // Print the error message
                return;
            }

            // Update the internal texture name and path
            textureAsset->UpdateTextureNameAndPath(oldName, newName, newPath);

            textureAssets.erase(it);
            textureAssets[newName] = textureAsset;

            std::cout << "Texture name updated from " << oldName << " to " << newName << std::endl;

            // Optionally, serialize the updated texture asset back to the JSON file
            textureAsset->Serialize("src/Data/TextureAsset.json");
        }
        else
        {
            std::cerr << "Error: Texture not found for name " << oldName << std::endl;
        }
    }

    std::string AssetManager::UpdateTexturePath(const std::string& oldPath, const std::string& newName)
    {
        // Find the last directory separator
        size_t pos = oldPath.find_last_of("/\\");

        // If we found a valid separator, update the filename part
        if (pos != std::string::npos)
        {
            // Everything before the filename
            std::string pathWithoutName = oldPath.substr(0, pos + 1);

            // The file name (including extension)
            std::string fileName = oldPath.substr(pos + 1);

            // Find the last period in the fileName to separate the name and extension
            size_t extPos = fileName.find_last_of(".");
            std::string extension = (extPos != std::string::npos) ? fileName.substr(extPos) : "";  // Get extension if exists

            // Replace the file name with the new name, keeping the same extension
            std::string newFileName = newName + extension;  // Append the extension to the new name

            return pathWithoutName + newFileName;  // Return the new path
        }

        // If no directory part is found, return the original path (shouldn't happen if the path is well-formed)
        return oldPath;
    }

    bool CopyTextureToFolder(const std::string& sourceFilePath, const std::string& targetFolder)
    {
        // Extract the filename from the source path
        size_t pos = sourceFilePath.find_last_of("/\\");
        std::string fileName = sourceFilePath.substr(pos + 1);

        // Construct the target file path
        std::string targetFilePath = targetFolder + "\\" + fileName;

        // Open the source file for reading in binary mode
        std::ifstream sourceFile(sourceFilePath, std::ios::binary);
        if (!sourceFile.is_open()) {
            std::cerr << "Failed to open source file: " << sourceFilePath << std::endl;
            return false;
        }

        // Open the target file for writing in binary mode
        std::ofstream targetFile(targetFilePath, std::ios::binary);
        if (!targetFile.is_open()) {
            std::cerr << "Failed to create target file: " << targetFilePath << std::endl;
            return false;
        }

        // Copy the file contents
        targetFile << sourceFile.rdbuf();

        std::cout << "File copied successfully to: " << targetFilePath << std::endl;
        return true;
    }

    void AssetManager::AddTexture(const std::string& name, const std::string& path)
    {
        // Define the base folder for textures
        std::string targetFolder = "./images";  // Fixed folder for storing textures
        std::cout << " - Target Folder: " << targetFolder << std::endl;

        // Extract the file name from the source path
        size_t pos = path.find_last_of("/\\");
        std::string fileName = path.substr(pos + 1);  // Get only the file name
        std::string targetPath = targetFolder + "/" + fileName;  // Full path in the target folder

        // Copy the texture to the target folder
        if (!CopyTextureToFolder(path, targetFolder))
        {
            std::cerr << "Failed to copy texture to target folder." << std::endl;
            return;
        }
        std::cout << "Texture successfully copied to target folder." << std::endl;

        // Check if a TextureAsset already exists for this name
        auto it = textureAssets.find(name);
        if (it == textureAssets.end())
        {
            std::cout << "No existing TextureAsset found for name: " << name << ". Creating a new one." << std::endl;

            // Create a new TextureAsset
            auto textureAsset = std::make_shared<TextureAsset>();
            textureAsset->AddTexture(name, targetPath);  // Add the texture to the new TextureAsset

            // Insert into the map
            textureAssets[name] = textureAsset;
            std::cout << "Texture added successfully." << std::endl;

            // Serialize this TextureAsset into a temporary JSON file
            textureAsset->Serialize("src/Data/TempTextureAsset.json");

            // Merge the temporary JSON file with the main JSON
            MergeTextureAssets("src/Data/TextureAsset.json", "src/Data/TempTextureAsset.json");
        }
        else
        {
            std::cout << "Found existing TextureAsset for name: " << name << ". Adding texture to it." << std::endl;

            // If the TextureAsset exists, add the texture to the existing asset
            it->second->AddTexture(name, targetPath);
            it->second->Serialize("src/Data/TextureAsset.json");

            // Merge the temporary JSON file with the main JSON
            MergeTextureAssets("src/Data/TextureAsset.json", "src/Data/TempTextureAsset.json");
        }
    }

    // Function to merge the temporary JSON with the main JSON file
    void AssetManager::MergeTextureAssets(const std::string& mainFilePath, const std::string& tempFilePath)
    {
        rapidjson::Document mainDoc;
        rapidjson::Document tempDoc;

        // Load the main JSON file
        std::ifstream mainFile(mainFilePath);
        if (mainFile.is_open())
        {
            std::string mainFileContents((std::istreambuf_iterator<char>(mainFile)), std::istreambuf_iterator<char>());
            mainDoc.Parse(mainFileContents.c_str());
        }
        else
        {
            std::cerr << "Failed to open main JSON file." << std::endl;
            return;
        }

        // Load the temporary JSON file
        std::ifstream tempFile(tempFilePath);
        if (tempFile.is_open())
        {
            std::string tempFileContents((std::istreambuf_iterator<char>(tempFile)), std::istreambuf_iterator<char>());
            tempDoc.Parse(tempFileContents.c_str());
        }
        else
        {
            std::cerr << "Failed to open temporary JSON file." << std::endl;
            return;
        }

        // Check for 'textures' array in both documents
        if (!mainDoc.HasMember("textures") || !mainDoc["textures"].IsArray())
        {
            std::cerr << "Main JSON does not contain a valid 'textures' array." << std::endl;
            return;
        }

        if (!tempDoc.HasMember("textures") || !tempDoc["textures"].IsArray())
        {
            std::cerr << "Temporary JSON does not contain a valid 'textures' array." << std::endl;
            return;
        }

        // Merge the 'textures' arrays
        rapidjson::Value& mainTextures = mainDoc["textures"];
        rapidjson::Value& tempTextures = tempDoc["textures"];

        // Push back the elements from the temporary array to the main array
        for (rapidjson::SizeType i = 0; i < tempTextures.Size(); ++i)
        {
            mainTextures.PushBack(tempTextures[i], mainDoc.GetAllocator());
        }

        // Write the merged content back to the main JSON file
        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        mainDoc.Accept(writer);

        std::ofstream outFile(mainFilePath);
        if (outFile.is_open())
        {
            outFile << buffer.GetString();
            outFile.close();
            std::cout << "Successfully merged textures and updated " << mainFilePath << std::endl;
        }
        else
        {
            std::cerr << "Failed to open main file for writing." << std::endl;
        }
    }

    void AssetManager::DeleteTexture(const std::string& textureName)
    {
        auto it = textureAssets.find(textureName);
        if (it != textureAssets.end())
        {
            auto textureAsset = it->second;

            // Remove the texture from the map
            textureAssets.erase(it);

            // Also remove it from the internal texturesMap of the TextureAsset class
            textureAsset->RemoveTexture(textureName);

            textureAsset->Serialize("src/Data/TextureAsset.json");

            std::cout << "Texture " << textureName << " deleted." << std::endl;
        }
        else
        {
            std::cerr << "Texture " << textureName << " not found!" << std::endl;
        }
    }

    std::string AssetManager::LoadandStoreShader(const std::string& filePath)
    {
        // Check if the shader is already loaded
        if (shaderSources.find(filePath) != shaderSources.end())
        {
            return shaderSources[filePath]; // Shader already loaded, no need to load again
        }

        std::ifstream shaderFile(filePath);
        if (!shaderFile.is_open())
        {
            throw std::runtime_error("Failed to open shader file: " + filePath);
        }

        std::ostringstream shaderStream;
        shaderStream << shaderFile.rdbuf(); // Read the file buffer into the stream
        shaderSources[filePath] = shaderStream.str(); // Store the shader code in the container
        return shaderStream.str();
    }

    const std::string& AssetManager::GetShaderSource(const std::string& shaderKey) const
    {
        auto it = shaderSources.find(shaderKey);
        if (it != shaderSources.end())
        {
            return it->second; // Return the shader source string
        }
        else
        {
            // Handle case where the key is not found
            throw std::runtime_error("Shader key not found: " + shaderKey);
        }
    }

    std::string AssetManager::LoadFontShader(const std::string& filePath)
    {
        auto shader = std::make_unique<FontShaderAsset>(filePath);
        std::string shaderSource = shader->DeserializeShader(filePath); // Retrieve shader source as string

        if (!shaderSource.empty()) {
            fontShaderAssets[filePath] = std::move(shader); // Store shader asset for future use
        }

        return shaderSource; // Return shader source code as string
    }

    FontShaderAsset* AssetManager::GetFontShader(const std::string& assetName)
    {
        auto it = fontShaderAssets.find(assetName);
        if (it != fontShaderAssets.end())
        {
            return it->second.get();
        }
        return nullptr;
    }

    FontAsset* AssetManager::LoadFont(const std::string& filePath, int fontSize)
    {
        // Check if the file exists 
        std::ifstream file(filePath); 
        if (!file.is_open()) 
        { 
            std::cerr << "Font file does not exist: " << filePath << std::endl; 
            return nullptr;     // Return early if the file doesn't exist 
        } 
        file.close();

        std::cout << "Attempting to load font: " << filePath << " of size: " << fontSize << std::endl;

        // Create a new FontAsset
        auto fontAsset = std::make_unique<FontAsset>(filePath, fontSize);

        // For this example, let's use the file name as the asset name
        std::string assetName = filePath.substr(filePath.find_last_of("/\\") + 1); // Extract file name from path

        // Store the font asset in the map
        fontAssets[assetName] = std::move(fontAsset);
        std::cout << "Font asset stored with name: " << assetName << std::endl;

        // Return the pointer to the newly created font asset
        return fontAssets[assetName].get();
    }

    FontAsset* AssetManager::GetFont(const std::string& assetName)
    {
        std::cout << "Retrieving font asset with name: " << assetName << std::endl;

        auto it = fontAssets.find(assetName);
        if (it != fontAssets.end())
        {
            std::cout << "Font asset found: " << assetName << std::endl;
            return it->second.get();
        }
        std::cout << "Font asset not found: " << assetName << std::endl;
        return nullptr;
    }
}