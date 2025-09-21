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
                case prop::STRING: { std::string value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                case prop::INT:    { int         value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                case prop::BOOL:   { bool        value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << (value ? "true" : "false"); break; }
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