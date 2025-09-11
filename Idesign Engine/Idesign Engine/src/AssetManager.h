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
#include "EntityAsset.h"
#include "AudioAsset.h"
#include "TextureAsset.h"
#include "FontAsset.h"
#include "FontShaderAsset.h"

#include <PropertySystem.h>

// Forward declaration of asset types here
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
        std::string typeName;

        AssetEntry() : pObject(nullptr, [](void*) {}), pProps(nullptr), typeName() {}
        
        AssetEntry(const AssetEntry&) = delete;
        AssetEntry& operator=(const AssetEntry&) = delete;
        AssetEntry(AssetEntry&&) = default;
        AssetEntry& operator=(AssetEntry&&) = default;

        AssetEntry(std::unique_ptr<void, void(*)(void*)> obj, const prop_list* props, const std::string& type)
            : pObject(std::move(obj)), pProps(props), typeName(type) {
        }
    };

    class NewAssetManager
    {
    public:

        static NewAssetManager& Instance() {
            static NewAssetManager instance;
            return instance;
        }

        // Delete copy/move constructors to enforce singleton
        NewAssetManager(const NewAssetManager&) = delete;
        NewAssetManager& operator=(const NewAssetManager&) = delete;

        template<typename T>
        T& Load(const std::string& key, const std::string& filePath, const prop_list& props)
        {
            auto it = m_Assets.find(key);
            if (it != m_Assets.end())
            {
                return *static_cast<T*>(it->second.pObject.get());
            }

            // Load from JSON
            std::ifstream file(filePath);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + filePath);
            }

            rapidjson::IStreamWrapper isw(file);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            file.close();

            if (doc.HasParseError()) {
                throw std::runtime_error("JSON parse error at offset " +
                    std::to_string(doc.GetErrorOffset()) + ": " +
                    rapidjson::GetParseError_En(doc.GetParseError()));
            }

            T* obj = new T();
            Deserialize(obj, props, doc);

            auto deleter = [](void* p) { delete static_cast<T*>(p); };
            std::unique_ptr<void, void(*)(void*)> ptr(obj, deleter);

            m_Assets[key] = AssetEntry{ std::move(ptr), &props, key };
            return *obj;
        }

        template<typename T>
        void LoadArray(const std::string& arrayKey, const std::string& filePath, const prop_list& props)
        {
            std::ifstream file(filePath);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + filePath);
            }

            rapidjson::IStreamWrapper isw(file);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            file.close();

            if (doc.HasParseError()) {
                throw std::runtime_error("JSON parse error at offset " +
                    std::to_string(doc.GetErrorOffset()) + ": " +
                    rapidjson::GetParseError_En(doc.GetParseError()));
            }

            if (!doc.HasMember(arrayKey.c_str()) || !doc[arrayKey.c_str()].IsArray()) {
                throw std::runtime_error("Missing or invalid array key: " + arrayKey);
            }

            const auto& array = doc[arrayKey.c_str()];
            for (const auto& entry : array.GetArray()) {
                T* obj = new T();
                Deserialize(obj, props, entry);

                // Use customName as the asset key
                if (!entry.HasMember("customName") || !entry["customName"].IsString()) {
                    delete obj;
                    throw std::runtime_error("Missing or invalid 'customName' field in array entry");
                }

                std::string key = entry["customName"].GetString();

                auto deleter = [](void* p) { delete static_cast<T*>(p); };
                std::unique_ptr<void, void(*)(void*)> ptr(obj, deleter);

                m_Assets[key] = AssetEntry{ std::move(ptr), &props, arrayKey };
            }
        }


        template<typename T>
        T* Get(const std::string& key)
        {
            auto it = m_Assets.find(key);
            if (it == m_Assets.end()) return nullptr;

            return static_cast<T*>(it->second.pObject);
        }


        void Unload(const std::string& key)
        {
            auto it = m_Assets.find(key);
            if (it != m_Assets.end())
            {
                m_Assets.erase(it);     
            }
        }

        void Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator)
        {
            for (const prop& p : PropList.m_Props)
            {
                rapidjson::Value key;
                key.SetString(p.m_pName, allocator); // Properly construct key

                if (p.m_Type == prop::INT)
                {
                    int value;
                    p.m_fnGet(pObject, &value);
                    outObj.AddMember(key, value, allocator);
                }
                else if (p.m_Type == prop::FLOAT)
                {
                    float value;
                    p.m_fnGet(pObject, &value);
                    outObj.AddMember(key, value, allocator);
                }
                else if (p.m_Type == prop::STRING)
                {
                    std::string value;
                    p.m_fnGet(pObject, &value);

                    rapidjson::Value jsonValue;
                    jsonValue.SetString(value.c_str(), static_cast<rapidjson::SizeType>(value.length()), allocator);

                    outObj.AddMember(key, jsonValue, allocator);
                }
                // Add other types as needed
            }
        }


        bool SaveToJson(const std::string& filePath, void* pObject, const prop_list& props)
        {
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            Serialize(pObject, props, doc, allocator);

            std::ofstream ofs(filePath);
            if (!ofs.is_open()) return false;

            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::Writer<rapidjson::OStreamWrapper> writer(osw);
            doc.Accept(writer);

            return true;
        }

        template<typename T>
        bool Reload(const std::string& key, const std::string& filePath, const prop_list& props)
        {
            auto newAsset = LoadFromJson<T>(filePath, props);
            if (!newAsset) return false;

            m_Assets[key] = AssetEntry{ std::move(newAsset), &props };
            return true;
        }

        const std::unordered_map<std::string, AssetEntry>& GetAssets() const 
        {
            return m_Assets;
        }

    private:
        NewAssetManager() = default; // Private constructor
        std::unordered_map<std::string, AssetEntry> m_Assets;

        inline void Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj)
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
    };

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