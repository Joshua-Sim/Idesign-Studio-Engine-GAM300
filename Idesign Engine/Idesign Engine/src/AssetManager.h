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

// Standard Lib files
#include <unordered_map>
#include <optional>
#include <functional>

// Graphics Header Files
#include "stb_image.h"
#include "stb_image_resize2.h"

// Properties Files
#include "PropertySystem.h"
#include "AudioProperties.h"
#include "WindowConfigProperties.h"
#include "TextureProperties.h"
#include "MeshProperties.h"
#include "TextProperties.h"
#include "NavMeshAgentProperties.h"
#include "NavMeshProperties.h"
#include "PrefabProperties.h"  
#include "SceneProperties.h"
#include "MaterialProperties.h"
#include "AnimationProperties.h"
#include "LightProperties.h"

// Asset Files
#include "AssetLoader.h"
#include "AssetHandler.h"
#include "MaterialLoader.h"


namespace Framework
{
    REGISTER_TYPE_NAME(WindowConfig);
    REGISTER_TYPE_NAME(AudioProperties);
    REGISTER_TYPE_NAME(TextureProperties);
    REGISTER_TYPE_NAME(MeshProperties);
    REGISTER_TYPE_NAME(TextProperties);
    REGISTER_TYPE_NAME(NavMeshComponent);
    REGISTER_TYPE_NAME(NavMeshAgentComponent);
    REGISTER_TYPE_NAME(PrefabProperties);
    REGISTER_TYPE_NAME(SceneProperties);
    REGISTER_TYPE_NAME(MaterialProperties);
    REGISTER_TYPE_NAME(MaterialMetadata);
    REGISTER_TYPE_NAME(AnimationProperties);
    REGISTER_TYPE_NAME(LightProperties);


    struct AssetKey 
    {
        AssetHandle guid;
        std::string typeName;

        bool operator==(const AssetKey& other) const 
        {
            return guid == other.guid && typeName == other.typeName;
        }
    };

    struct AssetKeyHash 
    {
        std::size_t operator()(const AssetKey& k) const noexcept 
        {
            std::size_t h1 = std::hash<AssetHandle>()(k.guid);
            std::size_t h2 = std::hash<std::string>()(k.typeName);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
        }
    };

    /*  Struct Brief
        - Store any type of assets in a generic way
        - Track metadata for serialization and debugging
        - Allow safetly movable but not copyable 
    */
    struct AssetEntry
    {
        std::unique_ptr<void, void(*)(void*)> pObject;  // Type-erased smart pointer to hold any asset object with a custom deleter
        const prop_list* pProps;                        // Pointer to the property list used for serialization/deserialization
        std::string typeName;                           // String to store the type name of the asset
        std::string binaryPath;                         // Asset file path
        AssetHandle guid;                               // Unique identifier for the asset

        AssetEntry() : pObject(nullptr, [](void*) {}), pProps(nullptr), typeName(), binaryPath(), guid() {}   // Default Constructor   
        AssetEntry(const AssetEntry&) = delete;                                         // Delete Copy Constructor
        AssetEntry& operator=(const AssetEntry&) = delete;                              // Delete Copy Assignment
        AssetEntry(AssetEntry&&) = default;                                             // Move Constructor
        AssetEntry& operator=(AssetEntry&&) = default;                                  // Move Assignment
        AssetEntry(std::unique_ptr<void, void(*)(void*)> obj,                           // Constructor 
                   const prop_list* props, 
                   const std::string& type, 
                   const std::string& path, 
                   AssetHandle guid)
                   : pObject(std::move(obj)), pProps(props), typeName(type), binaryPath(path), guid(guid) {}
    };

    class AssetManager
    {
    public:
        static AssetManager& Instance()
        {
            static AssetManager instance;
            return instance;
        }

        // Delete copy/move constructors to enforce singleton
        AssetManager(const AssetManager&) = delete;
        AssetManager& operator=(const AssetManager&) = delete;
        
        // Save all files here (WIP)
        ~AssetManager();

        //------------------------------------------------------------------------------------------------------------------------------------
        //  Binary Related Functions
        //  Json > Binary > Read Binary during Runtime
        //------------------------------------------------------------------------------------------------------------------------------------
        template<typename T>
        T& LoadBinary(const std::string& binaryPath, const prop_list& props, std::function<void(T&)> postInit = nullptr)
        {
            auto obj = AssetLoader<T>::Load(binaryPath, props);
            AssetHandle guidHandle(obj->guid);

            if (postInit)
                postInit(*obj);

            // Register using composite key
            RegisterAsset(std::move(obj), &props, binaryPath);

            // Build composite key for lookup
            AssetKey key{ guidHandle, TypeName<T>::value };

            return *static_cast<T*>(m_Assets[key].pObject.get());
        }

        template<typename T>
        void LoadBinaryArray(const std::string& binaryPath, const prop_list& props, std::function<void(T&)> postInit = nullptr)
        {
            auto loadedObjects = AssetLoader<T>::LoadArray(binaryPath, props);
            for (auto& obj : loadedObjects)
            {
                if (postInit)
                    postInit(*obj); 

                RegisterAsset(std::move(obj), &props, binaryPath); // Move after init
            }
        }

        template<typename T>
        void LoadBinaryArrayFromFolder(const std::string& folderPath, const prop_list& props, std::function<void(T&)> postInit = nullptr)
        {
            for (const auto& entry : std::filesystem::directory_iterator(folderPath))
            {
                if (!entry.is_regular_file()) continue;

                const auto& path = entry.path();
                if (path.extension() == ".bin")
                {
                    LoadBinaryArray<T>(path.string(), props, postInit);
                }
            }
        }

        //------------------------------------------------------------------------------------------------------------------------------------
        //  General Functions
        //------------------------------------------------------------------------------------------------------------------------------------        
        template<typename T>
        T* Get(const AssetHandle& guid)
        {
            AssetKey key{ guid, TypeName<T>::value };
            auto it = m_Assets.find(key);
            if (it == m_Assets.end()) return nullptr;
            return static_cast<T*>(it->second.pObject.get());
        }

        template<typename T>
        void RegisterAsset(std::unique_ptr<T> obj, const prop_list* props, const std::string& binaryPath)
        {
            if (obj->guid.empty())
                obj->guid = GenerateGUID();

            AssetHandle guidHandle(obj->guid);

            std::unique_ptr<void, void(*)(void*)> erasedPtr(
                static_cast<void*>(obj.release()),
                [](void* ptr) { delete static_cast<T*>(ptr); }
            );

            AssetKey key{ guidHandle, TypeName<T>::value };
            m_Assets[key] = AssetEntry(std::move(erasedPtr), props, TypeName<T>::value, binaryPath, guidHandle);
        }

        // Template function to find the GUID of an asset of type T.
        // Optionally filters by name if provided.
        template<typename T>
        AssetHandle FindGuid(const std::optional<std::string>& name = std::nullopt)
        {
            // Loop through all registered assets in the AssetManager
            for (const auto& [key, entry] : AssetManager::Instance().GetAssets())
            {
                // Skip entries that don't match the requested type
                if (entry.typeName != TypeName<T>::value)
                    continue;

                // Safely cast the generic asset pointer to the expected type T
                T* obj = static_cast<T*>(entry.pObject.get());
                if (!obj) continue; // Skip if the cast fails or the object is null

                // If a name was provided, try to match it against the asset's name
                if constexpr (requires(T t) { t.name; })
                {
                    if (name.has_value())
                    {
                        if (obj->name == name.value())
                            return key.guid;   // <-- use key.guid instead of guid
                    }
                    else
                    {
                        return key.guid;       // <-- use key.guid instead of guid
                    }
                }
                else
                {
                    if (!name.has_value())
                        return key.guid;       // <-- use key.guid instead of guid
                }
            }
            // No matching asset found — return an empty GUID
            return AssetHandle();
        }

        void MarkDirty(const AssetHandle& guid)
        {
            m_DirtyAssets.insert(guid);
        }

        bool IsDirty(const AssetHandle& guid) const
        {
            return m_DirtyAssets.contains(guid);
        }

        // Access to Unordered Map of Assets
        const std::unordered_map<AssetKey, AssetEntry, AssetKeyHash>& GetAssets() const
        {
            return m_Assets;
        }

        void SaveAllAssets();
        void CompileAllAssets();
        void LoadAllAssets();
        bool DeleteAssetFile(const std::string& filePath);
        void Unload(const std::string& guid);
        bool SaveToJson(void* pObject, const std::string& filePath, const prop_list& props);
        void SaveArrayToJson(const std::string& typeName, const std::string& filePath, const std::string& arrayKey);
        bool HasTextureByGUID(const std::string& guid) const;

    private:
        AssetManager() = default;                                   // Private constructor
        std::unordered_map<AssetKey, AssetEntry, AssetKeyHash> m_Assets;
        std::unordered_set<AssetHandle> m_DirtyAssets;              // Contains changed Assets
    };
}
#endif // !_ASSET_MANAGER_H_