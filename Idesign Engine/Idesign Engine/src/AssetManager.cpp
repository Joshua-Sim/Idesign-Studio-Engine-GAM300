///////////////////////////////////////////////////////////////////////////////
///
///	@File  : AssetManager.cpp
/// @Brief : Manages assets, including textures, audio, fonts, dictionaries, 
///          windows, entities, and shaders, with methods to load and retrieve 
///          these assets in the Framework namespace.
///
///	@Authors : Edwin
///	@Copyright 2024, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#include "pch.h"
#include "AssetManager.h"
#include "LogicManager.h"

namespace Framework
{
    AssetManager GlobalAssetManager;        // Global instance of AssetManager.
    unsigned char* AssetManager::data{};    // Texture data buffer for loading textures.

    AssetManager::AssetManager() 
    {
        // Initialization of Assets
        //LoadFont("../Unnamed Studios/Assets/Font/arial.ttf", 40);     // Loaded in fontsystem
        //LoadFontShader("src/UE_Font.vert");                           // Loaded in fontsystem
        //LoadFontShader("src/UE_Font.frag");                           // Loaded in fontsystem
        //LoadandStoreShader("src/UE.vert");                            // Loaded in Graphics.cpp
        //LoadandStoreShader("src/UE.frag");                            // Loaded in Graphics.cpp
    }

    AssetManager::~AssetManager() {}

    void AssetManager::LoadEntities(const std::string& filePath)
    {
        // If not, load it and store it in the container
        auto entityAsset = std::make_unique<EntityAsset>(filePath);
        entityAssets[filePath] = std::move(entityAsset);
    }
}