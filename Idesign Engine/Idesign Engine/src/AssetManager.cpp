///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AssetManager.cpp
/// @Brief : Manages assets, including textures, audio, fonts, dictionaries, 
///          windows, entities, and shaders, with methods to load and retrieve 
///          these assets in the Framework namespace.
///
///	@Authors : Edwin
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "AssetManager.h"

namespace Framework
{
    void AssetManager::PrintAssetsOfType(const std::string& typeName)
    {
        for (const auto& [key, entry] : m_Assets)
        {
            if (entry.typeName != typeName) continue;

            std::cout << "Asset: " << key << "\n";

            for (const auto& prop : entry.pProps->m_Props)
            {
                if (!prop.m_fnGet) continue;

                std::cout << "  " << prop.m_pName << ": ";

                switch (prop.m_Type)
                {
                case prop::STRING:
                {
                    std::string value;
                    prop.m_fnGet(entry.pObject.get(), &value);
                    std::cout << value;
                    break;
                }
                case prop::INT:
                {
                    int value;
                    prop.m_fnGet(entry.pObject.get(), &value);
                    std::cout << value;
                    break;
                }
                case prop::BOOL:
                {
                    bool value;
                    prop.m_fnGet(entry.pObject.get(), &value);
                    std::cout << (value ? "true" : "false");
                    break;
                }
                // Add other types as needed
                default:
                    std::cout << "[unsupported type]";
                    break;
                }

                std::cout << "\n";
            }

            std::cout << "------------------------\n";
        }
    }

    void AssetManager::Unload(const std::string& key)
    {
        auto it = m_Assets.find(key);
        if (it != m_Assets.end())
        {
            m_Assets.erase(it);
        }
    }

    void AssetManager::Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj)
    {
        assert(pObject != nullptr);
        assert(obj.IsObject());

        for (const prop& p : PropList.m_Props)
        {
            if (!obj.HasMember(p.m_pName)) continue;

            const rapidjson::Value& val = obj[p.m_pName];

            switch (p.m_Type)
            {
            case prop::INT:
                if (val.IsInt())
                {
                    int temp = val.GetInt();
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::FLOAT:
                if (val.IsNumber()) // Accepts both float and double
                {
                    float temp = static_cast<float>(val.GetDouble());
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::DOUBLE:
                if (val.IsNumber())
                {
                    double temp = val.GetDouble();
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::LONG:
                if (val.IsInt64())
                {
                    long temp = static_cast<long>(val.GetInt64());
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::SHORT:
                if (val.IsInt())
                {
                    short temp = static_cast<short>(val.GetInt());
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::CHAR:
                if (val.IsString() && val.GetStringLength() == 1)
                {
                    char temp = val.GetString()[0];
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::BOOL:
                if (val.IsBool())
                {
                    bool temp = val.GetBool();
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::STRING:
                if (val.IsString())
                {
                    std::string temp(val.GetString(), val.GetStringLength()); // Safer string copy
                    p.m_fnSet(pObject, &temp);
                }
                break;

            case prop::STRING_ARRAY:
                if (val.IsArray())
                {
                    std::vector<std::string> temp;
                    for (const auto& item : val.GetArray())
                    {
                        if (item.IsString())
                        {
                            temp.emplace_back(item.GetString(), item.GetStringLength());
                        }
                        else
                        {
                            std::cerr << "Non-string element found in array for property: " << p.m_pName << std::endl;
                        }
                    }
                    p.m_fnSet(pObject, &temp);
                }
                else
                {
                    std::cerr << "Expected array for property: " << p.m_pName << std::endl;
                }
                break;

            default:
                // Optional: log unsupported or mismatched types
                std::cerr << "Unsupported or mismatched type for property: " << p.m_pName << std::endl;
                break;
            }
        }
    }

    void AssetManager::Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator)
    {
        assert(pObject != nullptr);
        assert(outObj.IsObject());

        for (const prop& p : PropList.m_Props)
        {
            rapidjson::Value key;
            key.SetString(p.m_pName, allocator);

            switch (p.m_Type)
            {
            case prop::INT:
            {
                int value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, value, allocator);
                break;
            }
            case prop::FLOAT:
            {
                float value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, value, allocator);
                break;
            }
            case prop::DOUBLE:
            {
                double value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, value, allocator);
                break;
            }
            case prop::LONG:
            {
                long value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, static_cast<int64_t>(value), allocator); // RapidJSON prefers int64_t
                break;
            }
            case prop::SHORT:
            {
                short value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, static_cast<int>(value), allocator); // Promote to int for JSON
                break;
            }
            case prop::CHAR:
            {
                char value;
                p.m_fnGet(pObject, &value);
                rapidjson::Value jsonValue;
                jsonValue.SetString(&value, 1, allocator); // Single char as string
                outObj.AddMember(key, jsonValue, allocator);
                break;
            }
            case prop::BOOL:
            {
                bool value;
                p.m_fnGet(pObject, &value);
                outObj.AddMember(key, value, allocator);
                break;
            }
            case prop::STRING:
            {
                std::string value;
                p.m_fnGet(pObject, &value);
                rapidjson::Value jsonValue;
                jsonValue.SetString(value.c_str(), allocator);
                outObj.AddMember(key, jsonValue, allocator);
                break;
            }
            default:
                std::cerr << "Unsupported type during serialization: " << p.m_pName << std::endl;
                break;
            }
        }
    }

    bool AssetManager::SaveToJson(void* pObject, const std::string& filePath, const prop_list& props)
    {
        assert(pObject != nullptr);
        assert(!props.m_Props.empty());

        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        Serialize(pObject, props, doc, allocator);

        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;

        rapidjson::OStreamWrapper osw(ofs);
        rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
        doc.Accept(writer);

        return true;
    }

    void AssetManager::SaveArrayToJson(const std::string& typeName, const std::string& filePath, const std::string& arrayKey)
    {
        assert(!typeName.empty());
        assert(!arrayKey.empty());

        const auto& assets = GetAssets();

        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        rapidjson::Value array(rapidjson::kArrayType);

        for (const auto& [key, entry] : assets)
        {
            std::cout << entry.typeName << std::endl;
            std::cout << typeName << std::endl;

            if (entry.typeName != typeName) continue;

            rapidjson::Value obj(rapidjson::kObjectType);
            Serialize(entry.pObject.get(), *entry.pProps, obj, allocator);
            array.PushBack(obj, allocator);
        }

        doc.AddMember(rapidjson::Value(arrayKey.c_str(), allocator), array, allocator);

        std::ofstream ofs(filePath);
        if (ofs.is_open())
        {
            rapidjson::OStreamWrapper osw(ofs);
            rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
            writer.SetIndent(' ', 4);
            doc.Accept(writer);
        }
    }
}