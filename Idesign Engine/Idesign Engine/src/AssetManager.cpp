///////////////////////////////////////////////////////////////////////////////
///
///	@File� : AssetManager.cpp
/// @Brief : Manages assets, including textures, audio, fonts, dictionaries,�
///� � � � � windows, entities, and shaders, with methods to load and retrieve�
///� � � � � these assets in the Framework namespace.
///
///	@Authors : Edwin
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "AssetManager.h"
#include "EnginePath.h" // <-- Added this
#include <filesystem>   // <-- Added this

#include "ImguiManager.h"
#include "MessageSystem.h"

namespace Framework
{
	AssetManager::~AssetManager() {}

	void AssetManager::SaveAllAssets()
	{
		std::filesystem::path root = GetEngineRootPath();
		std::cout << "[AssetManager] SaveAllAssets using root path: " << root.string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] SaveAllAssets using root path: " + root.string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("SaveAllAssets using root path: " + root.string(), "AssetManager");

		// Nvr include windowconfig here
		std::string meshPath = (root / "Assets/JSON/MeshAsset.json").string();
		std::cout << "  Saving: " << meshPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + meshPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + meshPath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<MeshProperties>::value, meshPath, "mesh");

		std::string audioPath = (root / "Assets/JSON/AudioAsset.json").string();
		std::cout << "  Saving: " << audioPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + audioPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + audioPath, "AssetManager");
		//AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value, audioPath, "audio");
		AssetManager::Instance().SaveArrayToJson(TypeName<AudioMetadata>::value, audioPath, "audio");

		std::string texturePath = (root / "Assets/JSON/TextureAsset.json").string();
		std::cout << "  Saving: " << texturePath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + texturePath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + texturePath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<TextureProperties>::value, texturePath, "textures");

		std::string prefabPath = (root / "Assets/JSON/PrefabAsset.json").string();
		std::cout << "  Saving: " << prefabPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + prefabPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + prefabPath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<PrefabProperties>::value, prefabPath, "prefabs");

		std::string scenePath = (root / "Assets/JSON/SceneAsset.json").string();
		std::cout << "  Saving: " << scenePath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + scenePath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + scenePath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<SceneProperties>::value, scenePath, "scenes");

		std::string materialPath = (root / "Assets/JSON/MaterialAsset.json").string();
		std::cout << "  Saving: " << materialPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + materialPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + materialPath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<MaterialMetadata>::value, materialPath, "materials");

		std::string animPath = (root / "Assets/JSON/AnimationAsset.json").string();
		std::cout << "  Saving: " << animPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Saving: " + animPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Saving: " + animPath, "AssetManager");
		AssetManager::Instance().SaveArrayToJson(TypeName<AnimationProperties>::value, animPath, "animation");

		std::cout << "Files are being saved" << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("Files are being saved", Framework::ConsoleLogMessage::LogLevel::SUCCESS, "AssetManager"));
		ImguiManager::LogSuccess("Files are being saved", "AssetManager");
	}

	void AssetManager::CompileAllAssets()
	{
		std::filesystem::path root = GetEngineRootPath();
		std::cout << "[AssetManager] CompileAllAssets using root path: " << root.string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] CompileAllAssets using root path: " + root.string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("CompileAllAssets using root path: " + root.string(), "AssetManager");

		std::cout << "  Compiling Mesh:\n    JSON: " << (root / "Assets/JSON/MeshAsset.json").string() << "\n    BIN: " << (root / "Resource/MeshAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Mesh:\n    JSON: " + (root / "Assets/JSON/MeshAsset.json").string() + "\n    BIN: " + (root / "Resource/MeshAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Mesh: JSON: " + (root / "Assets/JSON/MeshAsset.json").string() + " BIN: " + (root / "Resource/MeshAsset.bin").string(), "AssetManager");
		CompileArray<MeshProperties>((root / "Assets/JSON/MeshAsset.json").string(), (root / "Resource/MeshAsset.bin").string(), (root / "Descriptor/MeshAsset.desc").string(), g_MeshProps, "mesh");

		std::cout << "  Compiling Audio:\n    JSON: " << (root / "Assets/JSON/AudioAsset.json").string() << "\n    BIN: " << (root / "Resource/AudioAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Audio:\n    JSON: " + (root / "Assets/JSON/AudioAsset.json").string() + "\n    BIN: " + (root / "Resource/AudioAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Audio: JSON: " + (root / "Assets/JSON/AudioAsset.json").string() + " BIN: " + (root / "Resource/AudioAsset.bin").string(), "AssetManager");
		//CompileArray<AudioProperties>((root / "Assets/JSON/AudioAsset.json").string(), (root / "Resource/AudioAsset.bin").string(), (root / "Descriptor/AudioAsset.desc").string(), g_AudioPropertiesProps, "audio");
		CompileArray<AudioMetadata>((root / "Assets/JSON/AudioAsset.json").string(), (root / "Resource/AudioAsset.bin").string(), (root / "Descriptor/AudioAsset.desc").string(), g_AudioMetadataProps, "audio");

		std::cout << "  Compiling Texture:\n    JSON: " << (root / "Assets/JSON/TextureAsset.json").string() << "\n    BIN: " << (root / "Resource/TextureAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Texture:\n    JSON: " + (root / "Assets/JSON/TextureAsset.json").string() + "\n    BIN: " + (root / "Resource/TextureAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Texture: JSON: " + (root / "Assets/JSON/TextureAsset.json").string() + " BIN: " + (root / "Resource/TextureAsset.bin").string(), "AssetManager");
		CompileArray<TextureProperties>((root / "Assets/JSON/TextureAsset.json").string(), (root / "Resource/TextureAsset.bin").string(), (root / "Descriptor/TextureAsset.desc").string(), g_TexturePropertiesProps, "textures");

		std::cout << "  Compiling Prefab:\n    JSON: " << (root / "Assets/JSON/PrefabAsset.json").string() << "\n    BIN: " << (root / "Resource/PrefabAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Prefab:\n    JSON: " + (root / "Assets/JSON/PrefabAsset.json").string() + "\n    BIN: " + (root / "Resource/PrefabAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Prefab: JSON: " + (root / "Assets/JSON/PrefabAsset.json").string() + " BIN: " + (root / "Resource/PrefabAsset.bin").string(), "AssetManager");
		CompileArray<PrefabProperties>((root / "Assets/JSON/PrefabAsset.json").string(), (root / "Resource/PrefabAsset.bin").string(), (root / "Descriptor/PrefabAsset.desc").string(), g_PrefabProps, "prefabs");

		std::cout << "  Compiling Scene:\n    JSON: " << (root / "Assets/JSON/SceneAsset.json").string() << "\n    BIN: " << (root / "Resource/SceneAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Scene:\n    JSON: " + (root / "Assets/JSON/SceneAsset.json").string() + "\n    BIN: " + (root / "Resource/SceneAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Scene: JSON: " + (root / "Assets/JSON/SceneAsset.json").string() + " BIN: " + (root / "Resource/SceneAsset.bin").string(), "AssetManager");
		CompileArray<SceneProperties>((root / "Assets/JSON/SceneAsset.json").string(), (root / "Resource/SceneAsset.bin").string(), (root / "Descriptor/SceneAsset.desc").string(), g_SceneProps, "scenes");

		std::cout << "  Compiling Material:\n    JSON: " << (root / "Assets/JSON/MaterialAsset.json").string() << "\n    BIN: " << (root / "Resource/MaterialAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Material:\n    JSON: " + (root / "Assets/JSON/MaterialAsset.json").string() + "\n    BIN: " + (root / "Resource/MaterialAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Material: JSON: " + (root / "Assets/JSON/MaterialAsset.json").string() + " BIN: " + (root / "Resource/MaterialAsset.bin").string(), "AssetManager");
		CompileArray<MaterialMetadata>((root / "Assets/JSON/MaterialAsset.json").string(), (root / "Resource/MaterialAsset.bin").string(), (root / "Descriptor/MaterialAsset.desc").string(), g_MaterialMetaProps, "materials");

		std::cout << "  Compiling Animation:\n    JSON: " << (root / "Assets/JSON/AnimationAsset.json").string() << "\n    BIN: " << (root / "Resource/AnimationAsset.bin").string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Compiling Animation:\n    JSON: " + (root / "Assets/JSON/AnimationAsset.json").string() + "\n    BIN: " + (root / "Resource/AnimationAsset.bin").string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Compiling Animation: JSON: " + (root / "Assets/JSON/AnimationAsset.json").string() + " BIN: " + (root / "Resource/AnimationAsset.bin").string(), "AssetManager");
		CompileArray<AnimationProperties>((root / "Assets/JSON/AnimationAsset.json").string(), (root / "Resource/AnimationAsset.bin").string(), (root / "Descriptor/AnimationAsset.desc").string(), g_AnimationPropertiesProps, "animation");

		std::cout << "Files are being compiled to .binary" << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("Files are being compiled to .binary", Framework::ConsoleLogMessage::LogLevel::SUCCESS, "AssetManager"));
		ImguiManager::LogSuccess("Files are being compiled to .binary", "AssetManager");
	}


	void AssetManager::LoadAllAssets()
	{
		std::filesystem::path root = GetEngineRootPath();
		std::cout << "[AssetManager] LoadAllAssets using root path: " << root.string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] LoadAllAssets using root path: " + root.string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("LoadAllAssets using root path: " + root.string(), "AssetManager");


		// Mesh & Texture is loaded under Graphics.cpp due to some initialization required under graphics.cpp
		std::string windowCfgPath = (root / "Resource/WindowAsset.cfg").string();
		std::cout << "  Loading: " << windowCfgPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + windowCfgPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + windowCfgPath, "AssetManager");
		AssetManager::Instance().LoadBinary<WindowConfig>(windowCfgPath, g_WindowConfigProps);

		std::string audioBinPath = (root / "Resource/AudioAsset.bin").string();
		std::cout << "  Loading: " << audioBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + audioBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + audioBinPath, "AssetManager");
		//AssetManager::Instance().LoadBinaryArray<AudioProperties>(audioBinPath, g_AudioPropertiesProps);
		AssetManager::Instance().LoadBinaryArray<AudioMetadata>(audioBinPath, g_AudioMetadataProps);

		std::string prefabBinPath = (root / "Resource/PrefabAsset.bin").string();
		std::cout << "  Loading: " << prefabBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + prefabBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + prefabBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<PrefabProperties>(prefabBinPath, g_PrefabProps);

		std::string sceneBinPath = (root / "Resource/SceneAsset.bin").string();
		std::cout << "  Loading: " << sceneBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + sceneBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + sceneBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<SceneProperties>(sceneBinPath, g_SceneProps);

		std::string materialBinPath = (root / "Resource/MaterialAsset.bin").string();
		std::cout << "  Loading: " << materialBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + materialBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + materialBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<MaterialMetadata>(
			materialBinPath,
			g_MaterialMetaProps,
			[=](MaterialMetadata& meta) // Capture root by copy
			{
				// Register metadata
				AssetManager::Instance().RegisterAsset(
					std::make_unique<MaterialMetadata>(meta),
					&g_MaterialMetaProps,
					meta.path);

				// Load and register properties
				// IMPORTANT: Assumes meta.path is relative to the root
				std::string fullMaterialPath = (root / meta.path).string();

				// Added debug line here
				std::cout << "    Loading individual material: " << fullMaterialPath << std::endl;
				Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("    Loading individual material: " + fullMaterialPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
				ImguiManager::Log("    Loading individual material: " + fullMaterialPath, "AssetManager");

				auto material = LoadMaterialAsset(fullMaterialPath, meta.guid);
				if (material)
				{
					material->guid = meta.guid;
					ImguiManager::Log("[AssetManager] Registering MaterialProperties for GUID=" + meta.guid.id + " at path=" + meta.path, "AssetManager");
					AssetManager::Instance().RegisterAsset(std::move(material), &g_MaterialProps, meta.path);
				}
				else
				{
					std::cerr << "[AssetManager] Failed to load material file: "
						<< fullMaterialPath << " for GUID=" << meta.guid << std::endl;
					Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] Failed to load material file: " + fullMaterialPath + " for GUID=" + meta.guid.id, Framework::ConsoleLogMessage::LogLevel::ERR, "AssetManager"));
					ImguiManager::LogError("Failed to load material file: " + fullMaterialPath + " for GUID=" + meta.guid.id, "AssetManager");
				}
			}
		);
	}

	bool AssetManager::DeleteAssetFile(const std::string& filePath)
	{
		// Assumes filePath is relative to the engine root
		std::filesystem::path fullPath = std::filesystem::path(GetEngineRootPath()) / filePath;

		try
		{
			if (std::filesystem::exists(fullPath))
			{
				std::filesystem::remove(fullPath);
				std::cout << "[AssetManager] Deleted file: " << fullPath.string() << std::endl;
				Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] Deleted file: " + fullPath.string(), Framework::ConsoleLogMessage::LogLevel::SUCCESS, "AssetManager"));
				ImguiManager::LogSuccess("Deleted file: " + fullPath.string(), "AssetManager");
				return true;
			}
			else
			{
				std::cerr << "[AssetManager] File not found: " << fullPath.string() << std::endl;
				Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] File not found: " + fullPath.string(), Framework::ConsoleLogMessage::LogLevel::ERR, "AssetManager"));
				ImguiManager::LogError("File not found: " + fullPath.string(), "AssetManager");
				return false;
			}
		}
		catch (const std::filesystem::filesystem_error& e)
		{
			std::cerr << "[AssetManager] File deletion error: " << e.what() << std::endl;
			Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] File deletion error: " + std::string(e.what()), Framework::ConsoleLogMessage::LogLevel::ERR, "AssetManager"));
			ImguiManager::LogError("File deletion error: " + std::string(e.what()), "AssetManager");
			return false;
		}
	}

	void AssetManager::Unload(const std::string& guid)
	{
		AssetHandle handle(guid);
		std::filesystem::path root = GetEngineRootPath();

		// Collect all keys with this GUID
		std::vector<AssetKey> keysToErase;
		for (auto& [key, entry] : m_Assets)
		{
			if (key.guid == handle)
			{
				// Delete file from disk if path property exists
				std::string filePath;
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

				if (!filePath.empty())
				{
					// Assumes filePath is relative to the engine root
					std::filesystem::path fullPath = root / filePath;
					if (std::filesystem::exists(fullPath))
					{
						std::error_code ec;
						std::filesystem::remove(fullPath, ec);
						if (ec) {
							std::cerr << "Failed to delete file: " << fullPath.string() << "\nReason: " << ec.message() << std::endl;
							Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("Failed to delete file: " + fullPath.string() + "\nReason: " + ec.message(), Framework::ConsoleLogMessage::LogLevel::ERR, "AssetManager"));
							ImguiManager::LogError("Failed to delete file: " + fullPath.string() + " Reason: " + ec.message(), "AssetManager");
						}
						else {
							std::cout << "Deleted file from disk: " << fullPath.string() << std::endl;
							Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("Deleted file from disk: " + fullPath.string(), Framework::ConsoleLogMessage::LogLevel::SUCCESS, "AssetManager"));
							ImguiManager::LogSuccess("Deleted file from disk: " + fullPath.string(), "AssetManager");
						}
					}
				}

				keysToErase.push_back(key);
			}
		}

		// Erase all entries for this GUID
		for (const auto& key : keysToErase)
			m_Assets.erase(key);
	}

	bool AssetManager::SaveToJson(void* pObject, const std::string& filePath, const prop_list& props)
	{
		assert(pObject != nullptr);
		assert(!props.m_Props.empty());

		rapidjson::Document doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		Serialize(pObject, props, doc, allocator);

		// Assumes filePath is relative to the engine root
		std::filesystem::path fullPath = std::filesystem::path(GetEngineRootPath()) / filePath;
		std::ofstream ofs(fullPath);
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

		// Assumes filePath is relative to the engine root
		std::filesystem::path fullPath = std::filesystem::path(GetEngineRootPath()) / filePath;
		std::ofstream ofs(fullPath);
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
		AssetHandle handle(guid);
		AssetKey key{ handle, TypeName<TextureProperties>::value };

		auto it = m_Assets.find(key);
		return it != m_Assets.end();
	}

	// NEW FUNCTION - Load WindowConfig before Graphics system is created
	void AssetManager::LoadWindowConfig()
	{
		std::filesystem::path root = GetEngineRootPath();
		std::cout << "[AssetManager] LoadWindowConfig using root path: " << root.string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] LoadWindowConfig using root path: " + root.string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("LoadWindowConfig using root path: " + root.string(), "AssetManager");

		std::string windowCfgPath = (root / "Resource/WindowAsset.cfg").string();
		std::cout << "  Loading: " << windowCfgPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + windowCfgPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + windowCfgPath, "AssetManager");
		AssetManager::Instance().LoadBinary<WindowConfig>(windowCfgPath, g_WindowConfigProps);
	}

	// NEW FUNCTION - Load remaining assets after OpenGL initialization
	void AssetManager::LoadRemainingAssets()
	{
		std::filesystem::path root = GetEngineRootPath();
		std::cout << "[AssetManager] LoadRemainingAssets using root path: " << root.string() << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] LoadRemainingAssets using root path: " + root.string(), Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("LoadRemainingAssets using root path: " + root.string(), "AssetManager");

		// Mesh & Texture is loaded under Graphics.cpp due to some initialization required under graphics.cpp

		std::string audioBinPath = (root / "Resource/AudioAsset.bin").string();
		std::cout << "  Loading: " << audioBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + audioBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + audioBinPath, "AssetManager");
		//AssetManager::Instance().LoadBinaryArray<AudioProperties>(audioBinPath, g_AudioPropertiesProps);

		std::string prefabBinPath = (root / "Resource/PrefabAsset.bin").string();
		std::cout << "  Loading: " << prefabBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + prefabBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + prefabBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<PrefabProperties>(prefabBinPath, g_PrefabProps);

		std::string sceneBinPath = (root / "Resource/SceneAsset.bin").string();
		std::cout << "  Loading: " << sceneBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + sceneBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + sceneBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<SceneProperties>(sceneBinPath, g_SceneProps);

		std::string materialBinPath = (root / "Resource/MaterialAsset.bin").string();
		std::cout << "  Loading: " << materialBinPath << std::endl;
		Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("  Loading: " + materialBinPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
		ImguiManager::Log("  Loading: " + materialBinPath, "AssetManager");
		AssetManager::Instance().LoadBinaryArray<MaterialMetadata>(
			materialBinPath,
			g_MaterialMetaProps,
			[=](MaterialMetadata& meta) // Capture root by copy
			{
				// Register metadata
				AssetManager::Instance().RegisterAsset(
					std::make_unique<MaterialMetadata>(meta),
					&g_MaterialMetaProps,
					meta.path);

				// Load and register properties
				// IMPORTANT: Assumes meta.path is relative to the root
				std::string fullMaterialPath = (root / meta.path).string();

				// Added debug line here
				std::cout << "    Loading individual material: " << fullMaterialPath << std::endl;
				Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("    Loading individual material: " + fullMaterialPath, Framework::ConsoleLogMessage::LogLevel::INFO, "AssetManager"));
				ImguiManager::Log("    Loading individual material: " + fullMaterialPath, "AssetManager");

				auto material = LoadMaterialAsset(fullMaterialPath, meta.guid);
				if (material)
				{
					material->guid = meta.guid;
					ImguiManager::Log("[AssetManager] Registering MaterialProperties for GUID=" + meta.guid.id + " at path=" + meta.path, "AssetManager");
					AssetManager::Instance().RegisterAsset(std::move(material), &g_MaterialProps, meta.path);
				}
				else
				{
					std::cerr << "[AssetManager] Failed to load material file: "
						<< fullMaterialPath << " for GUID=" << meta.guid << std::endl;
					Framework::MessageBus::Get().SendImmediate(Framework::ConsoleLogMessage("[AssetManager] Failed to load material file: " + fullMaterialPath + " for GUID=" + meta.guid.id, Framework::ConsoleLogMessage::LogLevel::ERR, "AssetManager"));
					ImguiManager::LogError("Failed to load material file: " + fullMaterialPath + " for GUID=" + meta.guid.id, "AssetManager");
				}
			}
		);
	}
}