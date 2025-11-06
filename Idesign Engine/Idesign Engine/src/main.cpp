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
#include "AssetCompiler.h"
#include "EngineState.h"
#include "Collision.h"
#include <ImguiManager.h>
#include <SignalHandler.h>
#include "Scenemanager.h"
#include "NavMeshSystem.h"
#include "AnimationSystem.h"

#include <WindowConfigProperties.h>
#include "AudioLoader.h"
#include "Audio.h"
#include "AudioSystem.h"
#include "AudioComponent.h"
#include "ScriptingSystem.h"
#include "lua.hpp"
#include <FileCheckSystem.h>

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
Coordinator
ecsInterface;   // REQUIRED FOR ECS INTERFACE
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
    AssetManager::Instance().LoadBinary<WindowConfig>("Resource/WindowAsset.cfg", g_WindowConfigProps);
    CompileAllPrefabs("Assets/Prefabs", g_PrefabProps);

    //std::cout << "Compiling AudioAsset.json..." << std::endl;
    //bool compileSuccess = CompileArray<AudioProperties>("Assets/JSON/AudioAsset.json", "Resource/AudioAsset.bin", "Descriptor/AudioAsset.desc", g_AudioPropertiesProps, "audio");
    //std::cout << "Audio compilation: " << (compileSuccess ? "SUCCESS" : "FAILED") << std::endl;
    
    // Load audio properties into AssetManager first (WITHOUT calling UE_LoadEvent yet)
    std::cout << "Loading AudioAsset.bin into AssetManager..." << std::endl;
    AssetManager::Instance().LoadBinaryArray<AudioProperties>("Resource/AudioAsset.bin", g_AudioPropertiesProps);
    std::cout << "Audio assets registered in AssetManager!" << std::endl;

    // Initialize global ECS interface and other singleton components
    ecsInterface.Init();

    RegisterComponentTypes();                           // Step 1: Register known types
    AutoRegisterAllReflectedComponents(ecsInterface);   // Step 2: Hook them into ECS

    // Register graphics system and GLFW Window first 
    auto graphicsSystem = ecsInterface.RegisterSystem<Graphics>();
    // Initialize the InputHandler singleton
    InputHandler::Initialize(graphicsSystem->GetWindow());
    auto animationSystem = ecsInterface.RegisterSystem<AnimationSystem>();

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

    auto navMeshSystem = ecsInterface.RegisterSystem<NavMeshSystem>();

    //File Checking System
    auto fileCheckSystem = ecsInterface.RegisterSystem<Framework::FileCheckSystem>();
    // Audio System
    auto audioSystem = ecsInterface.RegisterSystem<AudioSystem>();

    // Registering systems in the ECS interface here

    // ------- Add systems to engine below -------- //
    engine->AddSystem(animationSystem.get()); // Calculate bone matrices
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(inputHandlerSystem);
    engine->AddSystem(imguiSystem.get());
    engine->AddSystem(scriptingSystem.get());
    engine->AddSystem(navMeshSystem.get());
	engine->AddSystem(audioSystem.get());

    // Register scene manager last
    engine->AddSystem(sceneSystem.get());
    engine->AddSystem(fileCheckSystem.get());

    //Folders and filetypes to check for external change
    fileCheckSystem->WatchFolder("Assets/Scripts", { ".lua" });
    fileCheckSystem->WatchFolder("Assets/JSON", { ".json", ".cfg" });
    fileCheckSystem->WatchFolder("Assets/shaders", { ".glsl" });
    fileCheckSystem->WatchFolder("Assets/Textures", { ".png", ".jpg" });
    fileCheckSystem->WatchFolder("Scenes", { ".json" });
    fileCheckSystem->WatchFolder("Assets/Prefabs", { ".prefab" });

    // Initialize the engine and run the main game loop
    engine->Initialize();

    ImguiManager::Log("Initializing FontSystem...", "FontSystem");
    fontSystem.Initialize();
    fontSystem.LoadFont("Assets/Font/arial.ttf", 40, "Arial");
    fontSystem.SetActiveFont("Arial");
    ImguiManager::LogSuccess("FontSystem ready", "FontSystem");

    // Run All Scripts
    if (luaL_dofile(scriptingSystem->GetLuaState(), "Assets/Scripts/cook_all.lua") != LUA_OK) {
        std::cerr << "[Cook] Lua error: "
            << lua_tostring(scriptingSystem->GetLuaState(), -1) << "\n";
    }
    scriptingSystem->CreateScriptEntity("Assets/Scripts/hello.lua");
    scriptingSystem->CreateScriptEntity("Assets/Scripts/controls.lua");
    //scriptingSystem->CreateScriptEntity("assets/scripts/prove_lua.lua");

    engine->GameLoop();

    // Clear all systems and other dynamically allocated resources
    engine->DestroySystems();

    // Clean up smart pointers as they go out of scope
    // No need to manually delete as unique_ptr will handle it
    return 0;
}