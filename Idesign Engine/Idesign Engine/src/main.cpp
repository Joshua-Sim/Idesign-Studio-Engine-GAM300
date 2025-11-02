///////////////////////////////////////////////////////////////////////////////////////
///
/// @file main.cpp
/// 
/// @brief This file defines the main start point of the idesign studio engine. It handles system update loop and clean-up.
/// 
/// @Authors: IDesign Studio Team
///	Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "pch.h"
#include "Core.h"
#include "Graphics.h"
#include "InputHandler.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include "FontSystem.h" 
#include "MessageSystem.h"
#include "TestSystem.h"
#include "FontShader.h"
#include "AssetManager.h"
#include "EngineState.h"
#include "Collision.h"
#include <ImguiManager.h>
#include <SignalHandler.h>
#include "Scenemanager.h"

#include <WindowConfigProperties.h>
#include "AudioLoader.h"
#include "Audio.h"
#include "AudioSystem.h"
#include "AudioComponent.h"
#include "ScriptingSystem.h"
#include "ComponentList.h" 
#include "lua.hpp"

using namespace Framework;

void EnableMemoryLeakChecking(int breakAlloc = -1)
{
    //Set the leak checking flag
    int tmpDbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(tmpDbgFlag);

    //If a valid break alloc provided set the breakAlloc
    if (breakAlloc != -1) _CrtSetBreakAlloc(breakAlloc);
}

// Define global ptrs
Coordinator ecsInterface;   // REQUIRED FOR ECS INTERFACE
EngineState engineState;

int main(void)
{
    SetupSignalHandlers();
    EnableMemoryLeakChecking(); // For memory check

    //Create the core engine which manages all the systems that make up the game
    auto engine = std::make_unique<CoreEngine>();

	// Initialize FMOD Studio Audio System
    std::cout << "Initializing FMOD Studio Audio System..." << std::endl;
    GlobalAudio.Initialize();
    std::cout << "FMOD Studio Audio System initialized successfully!" << std::endl;

    // ------- Create systems below -------- 
    //  Windows
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileSingleObject<WindowConfig>("src/Data/WindowAsset.json", "src/Data/WindowAsset.cfg", g_WindowConfigProps);
    AssetManager::Instance().LoadBinary<WindowConfig>("src/Data/WindowAsset.cfg", g_WindowConfigProps);
    //auto Window = std::make_unique<GraphicsWindows>(&config, engine.get());
    

    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Audio
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    /*CompileArray<AudioProperties>("src/Data/AudioAsset.json", "src/Data/AudioAsset.bin", g_AudioPropertiesProps, "audio");
    AssetManager::Instance().LoadBinaryArray<AudioProperties>("src/Data/AudioAsset.bin", g_AudioPropertiesProps,
        [](AudioProperties& props)
        {
            GlobalAudio.UE_LoadEvent(props.name);
        });*/
    //AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value, "src/Data/AudioAsset.json", "AudioProperties");
    //GlobalAudio.UE_PlaySound("winloop");
    std::cout << "Compiling AudioAsset.json..." << std::endl;
    bool compileSuccess = CompileArray<AudioProperties>("src/Data/AudioAsset.json", "src/Data/AudioAsset.bin", g_AudioPropertiesProps, "audio");
    std::cout << "Audio compilation: " << (compileSuccess ? "SUCCESS" : "FAILED") << std::endl;
    
    // Load audio properties into AssetManager first (WITHOUT calling UE_LoadEvent yet)
    std::cout << "Loading AudioAsset.bin into AssetManager..." << std::endl;
    AssetManager::Instance().LoadBinaryArray<AudioProperties>("src/Data/AudioAsset.bin", g_AudioPropertiesProps);
    std::cout << "Audio assets registered in AssetManager!" << std::endl;
  
    // NOW load FMOD events - AssetManager has all AudioProperties registered, so lookups will work
    std::cout << "Loading FMOD events..." << std::endl;
    
    for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
    {
        if (entry.typeName == TypeName<AudioProperties>::value)
        {
            AudioProperties* audioProps = static_cast<AudioProperties*>(entry.pObject.get());
            std::cout << "  Loading FMOD event: " << audioProps->name << " (path: " << audioProps->path << ")" << std::endl;
            GlobalAudio.UE_LoadEvent(audioProps->name);
        }
    }
    std::cout << "All FMOD events loaded successfully!" << std::endl;
   

    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Textures
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    
    CompileArray<TextureProperties>("src/Data/TextureAsset.json", "src/Data/TextureAsset.bin", g_TexturePropertiesProps, "textures");
    

    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Mesh
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    
    CompileArray<MeshProperties>("src/Data/MeshAsset.json", "src/Data/MeshAsset.bin", g_MeshProps, "mesh");
    

    // Initialize the InputHandler singleton
    //InputHandler::Initialize(Window->GetWindow());

    // Initialize global ECS interface and other singleton components
    ecsInterface.Init();

    // Register graphics system and GLFW Window first 
    auto graphicsSystem = ecsInterface.RegisterSystem<Graphics>();
    // Initialize the InputHandler singleton
    InputHandler::Initialize(graphicsSystem->GetWindow());

    auto sceneSystem = ecsInterface.RegisterSystem<SceneManager>();
    auto imguiSystem = ecsInterface.RegisterSystem<ImguiManager>();
    imguiSystem->SetGraphicsSystem(graphicsSystem.get());
    graphicsSystem->SetImguiManager(imguiSystem.get());
    

    //Access the InputHandler instance from the singleton
    InputHandler* inputHandlerSystem = InputHandler::GetInstance();
    if (!inputHandlerSystem) {
        std::cerr << "Failed to initialize InputHandler!" << std::endl;
        return -1; // Exit if InputHandler initialization failed
    }
    // 3. Use the new getter to connect the camera to the input handler
    Camera* graphicsCamera = graphicsSystem.get()->getCamera();
    inputHandlerSystem->SetCamera(graphicsCamera);

    // Scripting System
    auto scriptingSystem = ecsInterface.RegisterSystem<ScriptingSystem>();
    scriptingSystem->SetGraphicsSystem(graphicsSystem.get());

    // Audio System
    auto audioSystem = ecsInterface.RegisterSystem<AudioSystem>();

    // Registering systems in the ECS interface here

    // ------- Add systems to engine below -------- //
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(inputHandlerSystem);
    engine->AddSystem(imguiSystem.get());
    engine->AddSystem(scriptingSystem.get());
	engine->AddSystem(audioSystem.get());
	

    // Register scene manager last
    engine->AddSystem(sceneSystem.get());

    // Initialize the engine and run the main game loop
    engine->Initialize();

    ImguiManager::Log("Initializing FontSystem...", "FontSystem");
    fontSystem.Initialize();
    fontSystem.LoadFont("Assets/Font/arial.ttf", 40, "Arial");
    fontSystem.SetActiveFont("Arial");
    ImguiManager::LogSuccess("FontSystem ready", "FontSystem");

    // Run All Scripts
    if (luaL_dofile(scriptingSystem->GetLuaState(), "assets/scripts/cook_all.lua") != LUA_OK) {
        std::cerr << "[Cook] Lua error: "
            << lua_tostring(scriptingSystem->GetLuaState(), -1) << "\n";
    }
    scriptingSystem->CreateScriptEntity("assets/scripts/hello.lua");
    scriptingSystem->CreateScriptEntity("assets/scripts/controls.lua");
    //scriptingSystem->CreateScriptEntity("assets/scripts/prove_lua.lua");

    engine->GameLoop();

    // Clear all systems and other dynamically allocated resources
    engine->DestroySystems();

    // Clean up smart pointers as they go out of scope
    // No need to manually delete as unique_ptr will handle it
    return 0;
}