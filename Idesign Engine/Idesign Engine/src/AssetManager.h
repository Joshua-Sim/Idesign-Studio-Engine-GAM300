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

// Rapid Json Header Files
#include "JsonSerialize.h"

// Properties Files
#include <PropertySystem.h>
#include <AudioProperties.h>
#include <WindowConfigProperties.h>

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

        AssetEntry() : pObject(nullptr, [](void*) {}), pProps(nullptr), typeName() {}   // Default Constructor   
        AssetEntry(const AssetEntry&) = delete;                                         // Delete Copy Constructor
        AssetEntry& operator=(const AssetEntry&) = delete;                              // Delete Copy Assignment
        AssetEntry(AssetEntry&&) = default;                                             // Move Constructor
        AssetEntry& operator=(AssetEntry&&) = default;                                  // Move Assignment
        AssetEntry(std::unique_ptr<void, void(*)(void*)> obj,                           // Constructor 
            const prop_list* props, const std::string& type)
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

        //------------------------------------------------------------------------------------------------------------------------------------
        //  Binary Related Functions
        //  Json > Binary > Read Binary during Runtime
        //------------------------------------------------------------------------------------------------------------------------------------
        template<typename T>
        T& LoadBinary(const std::string& name, const std::string& binaryPath, const prop_list& props)
        {
            std::ifstream in(binaryPath, std::ios::binary);
            if (!in.is_open())
                throw std::runtime_error("Failed to open binary file: " + binaryPath);

            T* obj = new T{};
            BinaryToObject(in, *obj, props);

            std::unique_ptr<void, void(*)(void*)> erasedPtr(
                static_cast<void*>(obj),
                [](void* ptr) { delete static_cast<T*>(ptr); }
            );

            m_Assets[name] = AssetEntry(std::move(erasedPtr), &props, TypeName<T>::value);
            return *static_cast<T*>(m_Assets[name].pObject.get());
        }

        template<typename T>
        void BinaryToObject(std::istream& in, T& obj, const prop_list& props)
        {
            constexpr uint32_t MAX_STRING_LENGTH = 1024 * 1024;

            for (const auto& p : props.m_Props)
            {
                if (!p.m_fnSet) throw std::runtime_error("Function pointer m_fnSet is null for property: " + std::string(p.m_pName));
                switch (p.m_Type)
                {
                case prop::INT:     { int value;     in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::FLOAT:   { float value;   in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::DOUBLE:  { double value;  in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::LONG:    { long value;    in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::SHORT:   { short value;   in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::CHAR:    { char value;    in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::BOOL:    { bool value;    in.read(reinterpret_cast<char*>(&value), sizeof(value)); if (!in) throw; p.m_fnSet(&obj, &value); break; }
                case prop::STRING:
                {
                    uint32_t length;
                    in.read(reinterpret_cast<char*>(&length), sizeof(length));

                    if (!in || length > MAX_STRING_LENGTH) throw std::runtime_error("Invalid string length for property: " + std::string(p.m_pName));

                    std::string value(length, '\0');
                    in.read(&value[0], length);

                    if (!in) throw std::runtime_error("Failed to read string data for property: " + std::string(p.m_pName));
                    p.m_fnSet(&obj, &value);
                    break;
                }
                default:
                    throw std::runtime_error("Unsupported property type: " + std::string(p.m_pName));
                }
            }
        }

        template<typename T>
        void LoadBinaryArray(const std::string& binaryPath, const prop_list& props)
        {
            std::ifstream in(binaryPath, std::ios::binary);
            if (!in.is_open())
                throw std::runtime_error("Failed to open binary file: " + binaryPath);

            uint32_t count = 0;
            in.read(reinterpret_cast<char*>(&count), sizeof(count));
            if (!in || count == 0)
                throw std::runtime_error("Invalid or empty binary array: " + binaryPath);

            for (uint32_t i = 0; i < count; ++i)
            {
                auto* obj = new T{};
                BinaryToObject(in, *obj, props);

                std::string arrayName = obj->customName.empty() ? "unnamed_" + std::to_string(i) : obj->customName;

                std::unique_ptr<void, void(*)(void*)> erasedPtr(
                    static_cast<void*>(obj),
                    [](void* ptr) { delete static_cast<T*>(ptr); }
                );

                m_Assets[arrayName] = AssetEntry(std::move(erasedPtr), &props, TypeName<T>::value);
            }
        }

        // Note
        /*
            Step 1: Read Json and populate object
            Deserialize(&obj, props, doc);
            Step 2: Serialize object to binary
            JsonToBinary(out, obj, props);
            Step 3: Load binary and reconsutruct object
            BinaryToObject(in, obj, props);

            Convert this function to scripting ?
        */
        template<typename T>
        bool CompileSingleObject(const std::string& jsonPath, const std::string& outPath, const prop_list& props)
        {
            // Parse JSON
            FILE* fp = fopen(jsonPath.c_str(), "r");
            char buffer[65536];
            rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
            rapidjson::Document doc;
            doc.ParseStream(is);
            fclose(fp);

            if (doc.HasParseError()) 
            {
                std::cerr << "JSON parse error\n";
                return false;
            }

            // Deserialize into object
            T obj{};
            Deserialize(&obj, props, doc);

            // Serialize to binary
            std::ofstream out(outPath, std::ios::binary);
            if (!out.is_open()) return false;

            JsonToBinary(out, obj, props);
            out.close();
            return true;
        }

        template<typename T>
        bool CompileArray(const std::string& jsonPath, const std::string& outPath, const prop_list& props, const std::string& arrayKey)
        {
            FILE* fp = fopen(jsonPath.c_str(), "r");
            char buffer[65536];
            rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
            rapidjson::Document doc;
            doc.ParseStream(is);
            fclose(fp);

            if (doc.HasParseError() || !doc.HasMember(arrayKey.c_str()) || !doc[arrayKey.c_str()].IsArray())
            {
                std::cerr << "Invalid array format\n";
                return false;
            }

            const auto& arr = doc[arrayKey.c_str()].GetArray();
            std::ofstream out(outPath, std::ios::binary);
            if (!out.is_open()) return false;

            uint32_t count = static_cast<uint32_t>(arr.Size());
            out.write(reinterpret_cast<char*>(&count), sizeof(count));

            for (const auto& item : arr)
            {
                T obj{};
                Deserialize(&obj, props, item);
                JsonToBinary(out, obj, props);
            }
            out.close();
            return true;
        }

        template<typename T>
        void JsonToBinary(std::ofstream& out, const T& obj, const Framework::prop_list& props)
        {
            for (const auto& p : props.m_Props)
            {
                switch (p.m_Type)
                {
                case prop::INT:     { int    value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::FLOAT:   { float  value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::DOUBLE:  { double value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::LONG:    { long   value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::SHORT:   { short  value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::CHAR:    { char   value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::BOOL:    { bool   value; p.m_fnGet(&obj, &value); out.write(reinterpret_cast<const char*>(&value), sizeof(value)); break; }
                case prop::STRING:
                {
                    std::string value;
                    p.m_fnGet(&obj, &value);
                    uint32_t length = static_cast<uint32_t>(value.size());
                    out.write(reinterpret_cast<const char*>(&length), sizeof(length));
                    out.write(value.data(), length);
                    break;
                }
                default:
                    throw std::runtime_error(std::string("Unsupported property type: ") + p.m_pName);
                }
            }
        }

        template<typename T>
        void SaveToBinary(std::ofstream& out, const T& obj, const prop_list& props)
        {
            for (const auto& p : props.m_Props)
            {
                switch (p.m_Type)
                {
                case prop::INT:     { int    v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::FLOAT:   { float  v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::DOUBLE:  { double v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::LONG:    { long   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::SHORT:   { short  v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::CHAR:    { char   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::BOOL:    { bool   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
                case prop::STRING:
                {
                    std::string v;
                    p.m_fnGet(&obj, &v);
                    uint32_t len = static_cast<uint32_t>(v.size());
                    out.write((char*)&len, sizeof(len));
                    out.write(v.data(), len);
                    break;
                }
                default:
                    throw std::runtime_error(std::string("Unsupported property type: ") + p.m_pName);
                }
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

            return static_cast<T*>(it->second.pObject);
        }

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

        // For Array Files
        void PrintAssetsOfType(const std::string& typeName);
        // How to use
        // PrintAssetsOfType(TypeName<AudioProperties>::value);

        void Unload(const std::string& key);
        void Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj);
        void Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator);
        bool SaveToJson(void* pObject, const std::string& filePath, const prop_list& props);
        void SaveArrayToJson(const std::string& typeName, const std::string& filePath, const std::string& arrayKey);
    private:
        AssetManager() = default;                                   // Private constructor
        std::unordered_map<std::string, AssetEntry> m_Assets;       // Contains all types of Assets
    };
}
#endif // !_ASSET_MANAGER_H_