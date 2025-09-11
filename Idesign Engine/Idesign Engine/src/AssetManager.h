///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AssetManager.h
/// @Brief : This file implements the AssetManager class, a central component 
///          responsible for managing various asset types within the application, 
///          including audio, textures, fonts, and shaders. The AssetManager 
///          provides methods to load and retrieve these assets, allowing for 
///          efficient reuse across different systems.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _ASSET_MANAGER_H_
#define _ASSET_MANAGER_H_
#include "pch.h"
#include "unordered_map"

// Graphics Header Files
#include "stb_image.h"
#include "stb_image_resize2.h"

// Sub Asset Header Files
#include "DictionaryAsset.h"
#include "EntityAsset.h"
#include "AudioAsset.h"
#include "TextureAsset.h"
#include "FontAsset.h"
#include "FontShaderAsset.h"

#include <PropertySystem.h>

// Forward declaration of asset types here
class Dictionary;
class EntityAsset;
class AudioAsset;
class TextureAsset;
class FontAsset;
class FontShaderAsset;

namespace Framework
{

    struct AssetEntry
    {
        std::unique_ptr<void, void(*)(void*)> pObject;
        const prop_list* pProps;
    };

    class NewAssetManager
    {
    public:
        template<typename T>
        T& Load(const std::string& key, const prop_list& props)
        {
            auto it = m_Assets.find(key);
            if (it != m_Assets.end())
            {
                return *static_cast<T*>(it->second.pObject);
            }

            T* obj = new T(); // Load file here
            
            auto deleter = [](void* p) { delete static_cast<T*>(p); };
            std::unique_ptr<void, void(*)(void*)> ptr(obj, deleter);

            m_Assets[key] = AssetEntry{ std::move(ptr), &props };
            return *obj;
        }

        AssetEntry* Get(const std::string& key)
        {
            auto it = m_Assets.find(key);
            return (it != m_Assets.end()) ? &it->second : nullptr;
        }

    private:
        std::unordered_map<std::string, AssetEntry> m_Assets;
    };

    inline void TestDeserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj)
    {
        for (const prop& p : PropList.m_Props)
        {
            if (!obj.HasMember(p.m_pName)) continue;

            const rapidjson::Value& val = obj[p.m_pName];

            if (p.m_Type == prop::INT && val.IsInt())
            {
                int temp = val.GetInt();
                p.m_fnSet(pObject, &temp);
            }
            else if (p.m_Type == prop::FLOAT && val.IsFloat())
            {
                float temp = val.GetFloat();
                p.m_fnSet(pObject, &temp);
            }
            else if (p.m_Type == prop::STRING && val.IsString())
            {
                std::string temp = val.GetString();
                p.m_fnSet(pObject, &temp);
            }
            // Add other types as needed
        }
    }

    template<typename T>
    std::unique_ptr<void, void(*)(void*)> LoadFromJson(const std::string& filePath, const prop_list& props)
    {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << filePath << "\n";
            return std::unique_ptr<void, void(*)(void*)>(nullptr, [](void*) {});
        }

        rapidjson::IStreamWrapper isw(file);
        rapidjson::Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            std::cerr << "JSON parse error at offset " << doc.GetErrorOffset() << ": "
                << rapidjson::GetParseError_En(doc.GetParseError()) << "\n";
            return std::unique_ptr<void, void(*)(void*)>(nullptr, [](void*) {});
        }

        T* obj = new T();
        TestDeserialize(obj, props, doc);
        return std::unique_ptr<void, void(*)(void*)>(static_cast<void*>(obj), [](void* p) {
            delete static_cast<T*>(p);
            });
    }


    /**
     * @class AssetManager
     * @brief Manages various asset types, including window configurations, dictionaries,
     *        ECS entities, audio, textures, shaders, and fonts. Provides loading and
     *        retrieval functions for these assets.
     */
    class AssetManager
    {
    public:

        /**
         * @brief Default constructor for AssetManager.
         */
        AssetManager();

        /**
         * @brief Destructor for AssetManager. Cleans up managed assets.
         */
        ~AssetManager();

        // Dictionary & Prefix

        /**
         * @brief Loads a dictionary and prefix data from specified paths.
         * @param dictionaryPath Path to the dictionary file.
         * @param prefixPath Path to the prefix file.
         * @return A reference to the loaded Dictionary object.
         */
        Dictionary& LoadDictionary(const std::string& dictionaryPath,               // Load dictionary and prefix
                                   const std::string& prefixPath);       

        // ECS Objects

        /**
         * @brief Loads ECS entities from a specified file path.
         * @param filePath Path to the file containing ECS entity data.
         * @return A reference to the loaded EntityAsset object.
         */
        void LoadEntities(const std::string& filePath);                     // Load ECS Objects

        std::unordered_map<std::string, std::unique_ptr<EntityAsset>>& GetAllEntities()
        {
            return entityAssets;
        }

        // Audio

        /**
         * @brief Loads an audio asset from a specified file path.
         * @param filePath Path to the audio file.
         * @return A shared pointer to the loaded AudioAsset object.
         */
        void LoadAudio(const std::string& filePath);         // Load Audio
        
        /**
         * @brief Retrieves a loaded audio asset by name.
         * @param assetName Name of the audio asset.
         * @return A pointer to the AudioAsset object.
         */
        AudioAsset* GetAudioAsset(const std::string& assetName);                    // Method to retrieve an audio asset

        /**
         * @brief Retrieves all loaded audio assets.
         * @return A constant reference to an unordered map of audio assets.
         */
        const std::unordered_map<std::string, std::shared_ptr<AudioAsset>>& GetAllAudioAssets() const
        {
            return audioAssets;
        }

        void AddAudio(const std::string& name, const std::string& path);

        void MergeSerializeAudio(const std::string& mainFilePath, const std::string& tempFilePath);

        bool CopyAudioToFolder(const std::string& sourceFilePath, const std::string& targetFolder);

        bool DeleteAudioFile(const std::string& filePath);

        void UpdateAudioName(const std::string& oldName, const std::string& newName);
        
        void DeleteAudio(const std::string& name);

        // Texture

        /**
         * @brief Loads a texture asset from a specified file path.
         * @param filePath Path to the texture file.
         * @return A shared pointer to the loaded TextureAsset object.
         */
        void LoadTexture(const std::string& filePath);     // Load Texture
        
        /**
         * @brief Retrieves a loaded texture asset by name.
         * @param assetName Name of the texture asset.
         * @return A pointer to the TextureAsset object.
         */
        TextureAsset* GetTexture(const std::string& assetName);                     // Get texture based on name
        
        /**
         * @brief Retrieves a texture asset as a GLuint for graphics rendering.
         * @param assetName Name of the texture asset.
         * @return The GLuint ID associated with the texture.
         */
        GLuint GetTextureForGraphics(const std::string& assetName);                 // Get texture and reutrn GLuint

        /**
         * @brief Retrieves all loaded texture assets.
         * @return A reference to an unordered map of texture assets.
         */
        std::unordered_map<std::string, std::shared_ptr<TextureAsset>>& GetAllTextureAssets()
        {
            return textureAssets;
        }

        // Function to return a vector of shared_ptr<TextureAsset>
        std::vector<std::shared_ptr<TextureAsset>> GetAllTextureAssetsPtrs() 
        {
            std::vector<std::shared_ptr<TextureAsset>> texturePtrs;
            for (const auto& pair : textureAssets) 
            {
                texturePtrs.push_back(pair.second);  // Push the shared_ptr<TextureAsset> into the vector
            }
            return texturePtrs;
        }

        bool RenameTexture(const std::string& oldName, const std::string& newName);

        void UpdateTextureName(const std::string& oldName, const std::string& newName);

        std::string UpdateTexturePath(const std::string& oldPath, const std::string& newName);

        void MergeTextureAssets(const std::string& mainFilePath, const std::string& tempFilePath);

        void DeleteTexture(const std::string& textureName);

        /**
         * @brief Adds a texture to the manager from a specified name and path.
         * @param name The name associated with the texture.
         * @param path The file path to the texture.
         */
        void AddTexture(const std::string& name, const std::string& path);

        // Graphics Shader

        /**
         * @brief Loads and stores a shader source from a specified file path.
         * @param filePath Path to the shader file.
         * @return A string containing the shader source code.
         */
        std::string LoadandStoreShader(const std::string& filePath);
        
        /**
         * @brief Retrieves a shader source code by its key.
         * @param shaderKey Key identifying the shader.
         * @return A constant reference to the shader source code string.
         */
        const std::string& GetShaderSource(const std::string& shaderKey) const;

        // Font

        /**
         * @brief Loads a font asset from a specified file path and size.
         * @param filePath Path to the font file.
         * @param fontSize Size of the font to be loaded.
         * @return A pointer to the loaded FontAsset object.
         */
        FontAsset* LoadFont(const std::string& filePath, int fontSize);             // Load Font
        
        /**
         * @brief Retrieves a loaded font asset by name.
         * @param assetName Name of the font asset.
         * @return A pointer to the FontAsset object.
         */
        FontAsset* GetFont(const std::string& assetName);                           // Get Font based on name

        // Font Shader

        /**
         * @brief Loads a font shader from a specified file path.
         * @param filePath Path to the shader file.
         * @return A string containing the shader source code.
         */
        std::string LoadFontShader(const std::string& filePath);               // Load Font Shader
        
        /**
         * @brief Retrieves a loaded font shader by name.
         * @param assetName Name of the font shader asset.
         * @return A pointer to the FontShaderAsset object.
         */
        FontShaderAsset* GetFontShader(const std::string& assetName);               // Get Font Shader based on name

        /**
         * @brief Singleton instance access method for AssetManager.
         * @return A reference to the single instance of AssetManager.
         */
        static AssetManager& Instance()
        {
            static AssetManager instance;
            return instance;
        }

    private:
        static unsigned char* data;     // Static data buffer used for image loading
        std::unordered_map<std::string, std::unique_ptr<Dictionary>> dictionaryAssets;                  // Container for Dictionary
        std::unordered_map<std::string, std::unique_ptr<EntityAsset>> entityAssets;                     // Container for EntityAsset
        std::unordered_map<std::string, std::shared_ptr<AudioAsset>> audioAssets;                       // Container for AudioAsset
        std::unordered_map<std::string, std::shared_ptr<TextureAsset>> textureAssets;                   // Container for TextureAsset
        std::unordered_map<std::string, std::unique_ptr<FontAsset>> fontAssets;                         // Container for FontAsset
        std::unordered_map<std::string, std::unique_ptr<FontShaderAsset>> fontShaderAssets;             // Container for FontShaderAsset
        std::unordered_map<std::string, std::string> shaderSources;                                     // Container for GraphicsShaderAsset
};
    extern AssetManager GlobalAssetManager;  // Global instance of AssetManager, defined in AssetManager.cpp
}
#endif // !_ASSET_MANAGER_H_