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
    AssetManager::~AssetManager()
    {
        AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, "Assets/JSON/MeshAsset.json", "mesh");
        AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value, "Assets/JSON/AudioAsset.json", "audio");
        AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, "Assets/JSON/TextureAsset.json", "textures");
        AssetManager::Instance().SaveArrayToJson(TypeName<PrefabProperties>::value, "Assets/JSON/PrefabAsset.json", "prefabs");
        AssetManager::Instance().SaveArrayToJson(TypeName<SceneProperties>::value, "Assets/JSON/SceneAsset.json", "scenes");
        AssetManager::Instance().SaveArrayToJson(TypeName<MaterialMetadata>::value, "Assets/JSON/MaterialAsset.json", "materials");
    }

    void AssetManager::Unload(const std::string& guid)
    {
        auto it = m_Assets.find(guid);
        if (it == m_Assets.end()) return;

        const AssetEntry& entry = it->second;
        std::string filePath;

        // Use reflection to find the "path" property
        if (entry.pProps)
        {
            for (const auto& prop : entry.pProps->m_Props)
            {
                if (std::string(prop.m_pName) == "path" && prop.m_Type == prop::STRING && prop.m_fnGet)
                {
                    prop.m_fnGet(entry.pObject.get(), &filePath);
                    break;
                }
            }
        }

        // Delete file from disk
        if (!filePath.empty() && std::filesystem::exists(filePath))
        {
            std::error_code ec;
            std::filesystem::remove(filePath, ec);
            if (ec)
            {
                std::cerr << "Failed to delete file: " << filePath << "\nReason: " << ec.message() << std::endl;
            }
            else
            {
                std::cout << "Deleted file from disk: " << filePath << std::endl;
            }
        }
        // Remove from memory
        m_Assets.erase(it);
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

        for (const auto& [key, entryObject] : assets)
        {
            if (entryObject.typeName != typeName) continue;
            rapidjson::Value obj(rapidjson::kObjectType);
            Serialize(entryObject.pObject.get(), *entryObject.pProps, obj, allocator);
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

    bool AssetManager::HasTextureByGUID(const std::string& guid) const
    {
        auto it = m_Assets.find(guid);
        return it != m_Assets.end() && it->second.typeName == TypeName<TextureProperties>::value;
    }
}