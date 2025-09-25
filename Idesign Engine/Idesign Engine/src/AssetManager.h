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
#include "unordered_map"

// Graphics Header Files
#include "stb_image.h"
#include "stb_image_resize2.h"

// Properties Files
#include <PropertySystem.h>
#include <AudioProperties.h>
#include <WindowConfigProperties.h>

// Asset Files
#include <AssetCompiler.h>
#include <AssetLoader.h>
#include "AssetHandler.h"

namespace Framework
{
    REGISTER_TYPE_NAME(WindowConfig);
    REGISTER_TYPE_NAME(AudioProperties);
    //REGISTER_TYPE_NAME(TextureProperties);
    //REGISTER_TYPE_NAME(FontProperties);

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
        std::string binaryPath;                         //

        AssetEntry() : pObject(nullptr, [](void*) {}), pProps(nullptr), typeName(), binaryPath() {}   // Default Constructor   
        AssetEntry(const AssetEntry&) = delete;                                         // Delete Copy Constructor
        AssetEntry& operator=(const AssetEntry&) = delete;                              // Delete Copy Assignment
        AssetEntry(AssetEntry&&) = default;                                             // Move Constructor
        AssetEntry& operator=(AssetEntry&&) = default;                                  // Move Assignment
        AssetEntry(std::unique_ptr<void, void(*)(void*)> obj,                           // Constructor 
            const prop_list* props, const std::string& type, const std::string& path)
            : pObject(std::move(obj)), pProps(props), typeName(type), binaryPath(path) {}
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

        //------------------------------------------------------------------------------------------------------------------------------------
        //  Binary Related Functions
        //  Json > Binary > Read Binary during Runtime
        //------------------------------------------------------------------------------------------------------------------------------------
        template<typename T>
        T& LoadBinary(const std::string& name, const std::string& binaryPath, const prop_list& props)
        {
            auto obj = AssetLoader<T>::Load(binaryPath, props);

            // Assign GUID if missing
            /*if (obj->guid.empty())
            {
                obj->guid = GenerateGUID();
            }

            std::string guid = obj->guid;*/

            std::unique_ptr<void, void(*)(void*)> erasedPtr
            (
                static_cast<void*>(obj.release()),
                [](void* ptr) { delete static_cast<T*>(ptr); }
            );

            m_Assets[name] = AssetEntry(std::move(erasedPtr), &props, TypeName<T>::value, binaryPath);
            return *static_cast<T*>(m_Assets[name].pObject.get());
        }

        template<typename T>
        void LoadBinaryArray(const std::string& binaryPath, const prop_list& props)
        {
            auto loadedObjects = AssetLoader<T>::LoadArray(binaryPath, props);

            for (size_t i = 0; i < loadedObjects.size(); ++i)
            {
                std::unique_ptr<T>& obj = loadedObjects[i];

                // Assign GUID if missing
                if (obj->guid.empty()) 
                {
                    obj->guid = GenerateGUID();
                }

                // Get() function nid to change parameter to guid rather then customname
                // Allow current function to generateguid for object, but use name as key
                // using name as key to retrieve the guid of the object, then do hot reload
                std::string guid = obj->guid;
                std::string name = obj->customName;
                
                std::unique_ptr<void, void(*)(void*)> erasedPtr
                (
                    static_cast<void*>(obj.release()),
                    [](void* ptr) { delete static_cast<T*>(ptr); }
                );

                // Use GUID as the key in the asset map
                m_Assets[name] = AssetEntry(std::move(erasedPtr), &props, TypeName<T>::value, binaryPath);
            }
        }

        //------------------------------------------------------------------------------------------------------------------------------------
        //  General Functions
        //------------------------------------------------------------------------------------------------------------------------------------
        
        template<typename T>
        void SaveAllAssets(const T& obj, const std::string& jsonPath, const std::string& binPath, const prop_list& props, bool IsArray)
        {
            if (!IsArray)
            {
                SaveToJson(obj, jsonPath, props);
            }
            else
            {
                SaveArrayToJson(obj, jsonPath, props);
            }
            std::ofstream out(binPath, std::ios::binary);
            SaveToBinary(out, obj, props);
            out.close();
        }

        template<typename T>
        T* Get(const std::string& key)
        {
            assert(!key.empty());

            auto it = m_Assets.find(key);
            if (it == m_Assets.end()) return nullptr;

            if (it->second.typeName != TypeName<T>::value) return nullptr;
            return static_cast<T*>(it->second.pObject.get());
        }

        // For future Hot-Reload in Imgui (Testing required)
        template<typename T>
        T& Reload(const std::string& name) 
        {
            auto it = m_Assets.find(name);
            if (it == m_Assets.end())
                throw std::runtime_error("Asset not found: " + name);

            if (it->second.typeName != TypeName<T>::value)
                throw std::runtime_error("Type mismatch during reload for asset: " + name);

            const prop_list* props = it->second.pProps;
            const std::string& path = it->second.binaryPath;

            std::ifstream in(path, std::ios::binary);
            if (!in.is_open())
                throw std::runtime_error("Failed to open binary file: " + path);

            T* obj = new T{};
            BinaryToObject(in, *obj, *props);

            std::unique_ptr<void, void(*)(void*)> erasedPtr(
                static_cast<void*>(obj),
                [](void* ptr) { delete static_cast<T*>(ptr); }
            );

            it->second.pObject = std::move(erasedPtr);
            return *obj;
        }

        // Access to Unordered Map of Assets
        const std::unordered_map<std::string, AssetEntry>& GetAssets() const
        {
            return m_Assets;
        }

        void PrintAssetsOfType(const std::string& typeName);
        // PrintAssetsOfType(TypeName<AudioProperties>::value);

        void Unload(const std::string& key);
        bool SaveToJson(void* pObject, const std::string& filePath, const prop_list& props);
        void SaveArrayToJson(const std::string& typeName, const std::string& filePath, const std::string& arrayKey);
    private:
        AssetManager() = default;                                   // Private constructor
        std::unordered_map<std::string, AssetEntry> m_Assets;       // Contains all types of Assets
    };
}
#endif // !_ASSET_MANAGER_H_