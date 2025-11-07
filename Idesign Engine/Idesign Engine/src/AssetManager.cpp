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
    AssetManager::~AssetManager() {}

    void AssetManager::SaveAllAssets()
    {
        // Nvr include windowconfig here
        AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value,    "Assets/JSON/MeshAsset.json", "mesh");
        AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value,   "Assets/JSON/AudioAsset.json", "audio");
        AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, "Assets/JSON/TextureAsset.json", "textures");
        AssetManager::Instance().SaveArrayToJson(TypeName<PrefabProperties>::value,  "Assets/JSON/PrefabAsset.json", "prefabs");
        AssetManager::Instance().SaveArrayToJson(TypeName<SceneProperties>::value,   "Assets/JSON/SceneAsset.json", "scenes");
        AssetManager::Instance().SaveArrayToJson(TypeName<MaterialMetadata>::value,  "Assets/JSON/MaterialAsset.json", "materials");
        std::cout << "Files are being saved" << std::endl;
    }

    void AssetManager::CompileAllAssets()
    {
        //CompileSingleObject<WindowConfig>("src/Data/WindowAsset.json", "src/Data/WindowAsset.cfg", "Descriptor/WindowAsset.desc", g_WindowConfigProps);
        CompileArray<MeshProperties>("Assets/JSON/MeshAsset.json",       "Resource/MeshAsset.bin",      "Descriptor/MeshAsset.desc",     g_MeshProps, "mesh");
        CompileArray<AudioProperties>("Assets/JSON/AudioAsset.json",     "Resource/AudioAsset.bin",     "Descriptor/AudioAsset.desc",    g_AudioPropertiesProps, "audio");
        CompileArray<TextureProperties>("Assets/JSON/TextureAsset.json", "Resource/TextureAsset.bin",   "Descriptor/TextureAsset.desc",  g_TexturePropertiesProps, "textures");
        CompileArray<PrefabProperties>("Assets/JSON/PrefabAsset.json",   "Resource/PrefabAsset.bin",    "Descriptor/PrefabAsset.desc",   g_PrefabProps, "prefabs");
        CompileArray<SceneProperties>("Assets/JSON/SceneAsset.json",     "Resource/SceneAsset.bin",     "Descriptor/SceneAsset.desc",    g_SceneProps, "scenes");
        CompileArray<MaterialMetadata>("Assets/JSON/MaterialAsset.json", "Resource/MaterialAsset.bin",  "Descriptor/MaterialAsset.desc", g_MaterialMetaProps, "materials");
        std::cout << "Files are being compiled to .binary" << std::endl;
    }

    void AssetManager::LoadAllAssets()
    {
        // Mesh & Texture is loaded under Graphics.cpp due to some initialization required under graphics.cpp
        AssetManager::Instance().LoadBinary<WindowConfig>("Resource/WindowAsset.cfg", g_WindowConfigProps);
        AssetManager::Instance().LoadBinaryArray<AudioProperties>("Resource/AudioAsset.bin", g_AudioPropertiesProps);
        AssetManager::Instance().LoadBinaryArray<PrefabProperties>("Resource/PrefabAsset.bin", g_PrefabProps);
        AssetManager::Instance().LoadBinaryArray<SceneProperties>("Resource/SceneAsset.bin", g_SceneProps);
        AssetManager::Instance().LoadBinaryArray<MaterialMetadata>("Resource/MaterialAsset.bin", g_MaterialMetaProps, [](MaterialMetadata& meta)
            {
                auto material = LoadMaterialAsset(meta.path, meta.guid);
                if (material)
                {
                    SetGuid(material.get(), g_MaterialProps);

                    // Always update material's GUID to match metadata
                    material->guid = meta.guid;
                    AssetManager::Instance().SaveToJson(material.get(), meta.path, g_MaterialProps);
                    AssetManager::Instance().RegisterAsset(std::move(material), &g_MaterialProps, meta.path);
                }
                else
                {
                    std::cerr << "Failed to load material file: " << meta.path << " for GUID: " << meta.guid << "\n";
                }
            }
        );
    }

    bool AssetManager::DeleteAssetFile(const std::string& filePath)
    {
        try
        {
            if (std::filesystem::exists(filePath))
            {
                std::filesystem::remove(filePath);
                std::cout << "[AssetManager] Deleted file: " << filePath << std::endl;
                return true;
            }
            else
            {
                std::cerr << "[AssetManager] File not found: " << filePath << std::endl;
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            std::cerr << "[AssetManager] File deletion error: " << e.what() << std::endl;
            return false;
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