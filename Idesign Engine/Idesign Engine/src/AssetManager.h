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

#include "JsonSerialize.h"
#include <PropertySystem.h>
#include <AudioProperties.h>

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
            : pObject(std::move(obj)), pProps(props), typeName(type) {}
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

        // Note: Template definition must be in headers
        template<typename T>
        inline T& Load(const std::string& key, const std::string& filePath, const prop_list& props)
        {
            assert(!props.m_Props.empty());

            // Check if asset is already loaded
            auto it = m_Assets.find(key);
            if (it != m_Assets.end())
            {
                return *static_cast<T*>(it->second.pObject.get());
            }

            // Load JSON file
            std::ifstream file(filePath);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + filePath);
            }

            rapidjson::IStreamWrapper isw(file);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            file.close();

            if (doc.HasParseError())
            {
                throw std::runtime_error("JSON parse error at offset " +
                    std::to_string(doc.GetErrorOffset()) + ": " +
                    rapidjson::GetParseError_En(doc.GetParseError()));
            }

            // Create and deserialize object safely
            auto obj = std::make_unique<T>();
            Deserialize(obj.get(), props, doc);

            // Move into type-erased unique_ptr
            auto deleter = [](void* p) { delete static_cast<T*>(p); };
            std::unique_ptr<void, void(*)(void*)> erasedPtr(obj.release(), deleter);

            // Store in asset map
            m_Assets[key] = AssetEntry{ std::move(erasedPtr), &props, key };

            return *static_cast<T*>(m_Assets[key].pObject.get());
        }


        // Generic Loader Function with Json Array
        template<typename T>
        inline void LoadArray(const std::string& arrayKey, const std::string& filePath, const prop_list& props)
        {
            assert(!props.m_Props.empty());

            std::ifstream file(filePath);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open file: " + filePath);
            }

            rapidjson::IStreamWrapper isw(file);
            rapidjson::Document doc;
            doc.ParseStream(isw);
            file.close();

            if (doc.HasParseError())
            {
                throw std::runtime_error("JSON parse error at offset " +
                    std::to_string(doc.GetErrorOffset()) + ": " +
                    rapidjson::GetParseError_En(doc.GetParseError()));
            }

            if (!doc.HasMember(arrayKey.c_str()) || !doc[arrayKey.c_str()].IsArray())
            {
                throw std::runtime_error("Missing or invalid array key: " + arrayKey);
            }

            const auto& array = doc[arrayKey.c_str()];
            for (const auto& entry : array.GetArray())
            {
                T* obj = new T();
                Deserialize(obj, props, entry);

                // Use customName as the asset key
                if (!entry.HasMember("customName") || !entry["customName"].IsString())
                {
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
            assert(!key.empty());

            auto it = m_Assets.find(key);
            if (it == m_Assets.end()) return nullptr;

            return static_cast<T*>(it->second.pObject);
        }

        void Unload(const std::string& key);

        void Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj);

        void Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator);

        bool SaveToJson(void* pObject, const std::string& filePath, const prop_list& props);

        void SaveArrayToJson(const std::string& typeName, const std::string& filePath, const std::string& arrayKey);

        // For future Hot-Reload in Imgui (Testing required)
        template<typename T>
        bool Reload(const std::string& key, const std::string& filePath, const prop_list& props)
        {
            auto newAsset = LoadFromJson<T>(filePath, props);
            if (!newAsset) return false;

            m_Assets[key] = AssetEntry{ std::move(newAsset), &props };
            return true;
        }

        // Access to Unordered Map of Assets
        const std::unordered_map<std::string, AssetEntry>& GetAssets() const 
        {
            return m_Assets;
        }

    private:
        AssetManager() = default; // Private constructor
        std::unordered_map<std::string, AssetEntry> m_Assets;
    };
}
#endif // !_ASSET_MANAGER_H_