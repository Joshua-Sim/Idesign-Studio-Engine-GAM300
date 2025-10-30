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

        /*
        for (const auto& guid : m_DirtyAssets)
        {
            auto it = m_Assets.find(guid);
            if (it == m_Assets.end()) continue;

            const auto& entry = it->second;
            if (!entry.pObject || !entry.pProps) continue;

            std::ofstream out(entry.binaryPath, std::ios::binary);

            if (entry.typeName == TypeName<MeshProperties>::value)
            {
                auto* mesh = static_cast<MeshProperties*>(entry.pObject.get());
                JsonToBinary(out, *mesh, *entry.pProps);
            }
            else if (entry.typeName == TypeName<AudioProperties>::value)
            {
                auto* audio = static_cast<AudioProperties*>(entry.pObject.get());
                JsonToBinary(out, *audio, *entry.pProps);
            }
            else if (entry.typeName == TypeName<TextureProperties>::value)
            {
                auto* tex = static_cast<TextureProperties*>(entry.pObject.get());
                JsonToBinary(out, *tex, *entry.pProps);
            }
            else
            {
                std::cerr << "SaveAssets Unknown asset type: " << entry.typeName << std::endl;
            }
            out.close();
            std::cout << "SaveAssets Saved: " << guid << "  " << entry.binaryPath << std::endl;
        }
        m_DirtyAssets.clear(); // Reset after saving
        */
    }

    void AssetManager::PrintAssetsOfType(const std::string& typeName)
    {
        std::cout << "=== Assets of Type: " << typeName << " ===\n";

        for (const auto& [key, entry] : m_Assets)
        {
            if (entry.typeName != typeName) continue;

            std::cout << "Asset GUID: " << entry.guid << "\n";
            std::cout << "Binary Path: " << entry.binaryPath << "\n";

            if (!entry.pProps || entry.pProps->m_Props.empty())
            {
                std::cout << "  [No properties registered]\n";
                continue;
            }

            for (const auto& prop : entry.pProps->m_Props)
            {
                if (!prop.m_fnGet) continue;

                std::cout << "  " << prop.m_pName << ": ";

                switch (prop.m_Type)
                {
                    case prop::STRING:  { std::string value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                    case prop::INT:     { int         value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                    case prop::FLOAT:   { float       value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                    case prop::DOUBLE:  { double      value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << value; break; }
                    case prop::BOOL:    { bool        value; prop.m_fnGet(entry.pObject.get(), &value); std::cout << (value ? "true" : "false"); break; }
                    default:            std::cout << "[unsupported type]"; break;
                }
                std::cout << "\n";
            }
            // Optional: Print runtime-only fields if type matches
            if (typeName == TypeName<TextureProperties>::value)
            {
                const auto* tex = static_cast<const TextureProperties*>(entry.pObject.get());
                std::cout << "  [Runtime] TextureID: " << tex->textureID << "\n";
                std::cout << "  [Runtime] Width: " << tex->width << "\n";
                std::cout << "  [Runtime] Height: " << tex->height << "\n";
            }
            std::cout << "------------------------\n";
        }
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