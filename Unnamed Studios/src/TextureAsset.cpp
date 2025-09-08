///////////////////////////////////////////////////////////////////////////////
///
///	@File  : TextureAsset.cpp
/// @Brief : Defines the TextureAsset class, responsible for managing texture
///          assets in the application. This includes loading textures from files,
///          serializing/deserializing texture data, and providing access to texture
///          properties such as names, paths, and OpenGL texture IDs.
///          The class supports functionalities like loading textures from a JSON
///          file, serializing them back to a file, and managing texture-related
///          information dynamically.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "TextureAsset.h"
#include "AssetManager.h"

TextureAsset::TextureAsset() {}

TextureAsset::TextureAsset(const std::string& filePath)
{
    Deserialize(filePath);
}

TextureAsset::~TextureAsset() {}

void TextureAsset::Deserialize(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();

    rapidjson::Document document;
    document.Parse(buffer.str().c_str());

    if (!document.IsObject())
    {
        std::cerr << "Invalid JSON format." << std::endl;
        return;
    }

    const auto& textures = document["textures"];
    if (!textures.IsArray())
    {
        std::cerr << "Textures field is not an array." << std::endl;
        return;
    }

    for (const auto& texture : textures.GetArray())
    {
        if (texture.IsObject())
        {
            Texture newTexture;

            // Deserialize texture name
            if (texture.HasMember("name") && texture["name"].IsString())
            {
                newTexture.name = texture["name"].GetString();
                //std::cout << "This is the texture name: " << newTexture.name << std::endl;
            }

            // Deserialize texture path
            if (texture.HasMember("path") && texture["path"].IsString())
            {
                newTexture.path = texture["path"].GetString();
                //std::cout << "This is the texture path: " << newTexture.path << std::endl;
            }

            // Store the texture in a map, using the texture name as the key
            texturesMap[newTexture.name] = newTexture;  // Store texture by name
        }
    }
}

void TextureAsset::Serialize(const std::string& filePath)
{
    rapidjson::Document document;

    // Open file for reading (if it exists) and parse it
    std::ifstream inFile(filePath);
    if (inFile.is_open())
    {
        std::string fileContents((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        document.Parse(fileContents.c_str());

        // Check for any parse errors
        if (document.HasParseError()) {
            std::cerr << "Failed to parse JSON file." << std::endl;
            return;
        }

        // Check if the 'textures' key exists and is an array, if not, initialize it as an array
        if (!document.HasMember("textures") || !document["textures"].IsArray()) {
            rapidjson::Value texturesArray(rapidjson::kArrayType);
            document.AddMember("textures", texturesArray, document.GetAllocator());  // Ensure we add an empty array if it's missing
        }
    }
    else
    {
        // If file does not exist, initialize document and add empty textures array
        document.SetObject();
        rapidjson::Value texturesArray(rapidjson::kArrayType);
        document.AddMember("textures", texturesArray, document.GetAllocator());
    }

    // Now we ensure that texturesArray is accessible
    rapidjson::Value& texturesArray = document["textures"];

    // Clear the textures array first, so we don't retain deleted textures
    texturesArray.Clear();

    // Access all texture assets from the global manager
    auto allTextureAssets = Framework::GlobalAssetManager.GetAllTextureAssets();

    // Iterate over texturesMap and add each texture to the texturesArray
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    // Iterate over each texture in the current TextureAsset
    for (const auto& texturePair : texturesMap)
    {
        const Texture& texture = texturePair.second;

        // Create a JSON object for this texture
        rapidjson::Value textureObject(rapidjson::kObjectType);
        textureObject.AddMember("name", rapidjson::Value(texture.name.c_str(), allocator), allocator);
        textureObject.AddMember("path", rapidjson::Value(texture.path.c_str(), allocator), allocator);

        // Add this texture to the textures array
        texturesArray.PushBack(textureObject, allocator);
    }

    // Write the updated document to the file
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    std::ofstream outFile(filePath);
    if (outFile.is_open())
    {
        outFile << buffer.GetString();  // Write the JSON string to the file
        outFile.close();
        std::cout << "Successfully serialized textures to " << filePath << std::endl;
    }
    else
    {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
    }
}

std::vector<std::string> TextureAsset::GetNames() const
{
    std::vector<std::string> names;

    // Iterate over the map, which stores textures by their names
    for (const auto& entry : texturesMap) // entry.first is the name of the texture
    {
        names.push_back(entry.first); // Add the texture name (the key in the map) to the vector
    }

    return names;
}

std::string TextureAsset::GetPath(const std::string& textureType) const
{
    // Find the texture in the map by its name (key)
    auto it = texturesMap.find(textureType);
    if (it != texturesMap.end()) // If texture found
    {
        return it->second.path;  // Return the path of the texture
    }
    else
    {
        std::cerr << "Texture name not found: " << textureType << std::endl;
        return ""; // Return an empty string if the texture is not found
    }
}

GLuint TextureAsset::GetTextureID(const std::string& textureName) const
{
    auto it = texturesMap.find(textureName);
    if (it != texturesMap.end())
    {
        return it->second.textureID;  // Access the textureID from the Texture struct
    }

    std::cerr << "Texture name not found: " << textureName << std::endl;
    return 0;  // Return 0 if the texture is not found
}

void TextureAsset::SetTextureID(const std::string& textureName, GLuint id)
{
    // Find the texture by its name
    auto it = texturesMap.find(textureName);
    if (it != texturesMap.end())
    {
        // Set the textureID for the found texture
        it->second.textureID = id;
        //std::cout << "Texture ID for " << textureName << " set to: " << id << std::endl;
    }
    else
    {
        std::cerr << "Texture name not found: " << textureName << std::endl;
    }
}

void TextureAsset::SetName(const std::string& oldName, const std::string& newName)
{
    auto it = texturesMap.find(oldName);
    if (it != texturesMap.end())
    {
        it->second.name = newName;
        std::cout << "Texture name updated from " << oldName << " to " << newName << std::endl;
    }
    else
    {
        std::cerr << "Texture name not found: " << oldName << std::endl;
    }
}

void TextureAsset::AddTexture(const std::string& name, const std::string& path)
{
    if (texturesMap.find(name) != texturesMap.end())
    {
        std::cerr << "Texture with the name " << name << " already exists." << std::endl;
        return; // Do nothing if the texture already exists
    }

    Texture newTexture{ name, path };
    texturesMap[name] = newTexture;  // Insert new texture
}

TextureAsset::Texture* TextureAsset::GetTexture(const std::string& textureName)
{
    auto it = texturesMap.find(textureName);
    if (it != texturesMap.end())
    {
        return &it->second;  // Return a pointer to the Texture
    }
    return nullptr;  // Return nullptr if the texture does not exist
}

void TextureAsset::UpdateTextureNameAndPath(const std::string& oldName, const std::string& newName, const std::string& newPath)
{
    // Update the name
    auto it = texturesMap.find(oldName);
    if (it != texturesMap.end())
    {
        it->second.name = newName;  // Update name

        // Update the path
        it->second.path = newPath;  // Update path

        // Erase the old name and insert the updated texture with the new name
        Texture texture = it->second;  // Copy the texture data
        texturesMap.erase(it);        // Remove the old entry
        texturesMap[newName] = texture;  // Insert the texture with the new name

        std::cout << "Texture updated: " << oldName << " -> " << newName << std::endl;
    }
    else
    {
        std::cerr << "Texture not found: " << oldName << std::endl;
    }
}

void TextureAsset::RemoveTexture(const std::string& textureName)
{
    auto it = texturesMap.find(textureName);
    if (it != texturesMap.end())
    {
        texturesMap.erase(it);
        std::cout << "Texture " << textureName << " removed from TextureAsset." << std::endl;
    }
    else
    {
        std::cerr << "Texture " << textureName << " not found in TextureAsset." << std::endl;
    }
}
