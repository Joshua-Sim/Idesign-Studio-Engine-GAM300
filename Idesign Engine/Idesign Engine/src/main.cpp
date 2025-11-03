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
#include "NavMeshSystem.h"

#include <WindowConfigProperties.h>
#include "AudioLoader.h"
#include "Audio.h"
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
Coordinator
ecsInterface;   // REQUIRED FOR ECS INTERFACE
EngineState engineState;

int main(void)
{
    SetupSignalHandlers();
    EnableMemoryLeakChecking(); // For memory check

    //Create the core engine which manages all the systems that make up the game
    auto engine = std::make_unique<CoreEngine>();

    // ------- Create systems below -------- 
    //  Windows
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    CompileSingleObject<WindowConfig>("Assets/JSON/WindowAsset.json", "Resource/WindowAsset.cfg", "Descriptor/WindowAsset.desc", g_WindowConfigProps);
    AssetManager::Instance().LoadBinary<WindowConfig>("Resource/WindowAsset.cfg", g_WindowConfigProps);
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Audio
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileArray<AudioProperties>("Assets/JSON/AudioAsset.json", "Resource/AudioAsset.bin", "Descriptor/AudioAsset.desc", g_AudioPropertiesProps, "audio");
    AssetManager::Instance().LoadBinaryArray<AudioProperties>("Resource/AudioAsset.bin", g_AudioPropertiesProps,
        [](AudioProperties& props)
        {
            GlobalAudio.UE_LoadSound(props.name);
        });

    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Prefabs
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileArray<PrefabProperties>("Assets/JSON/PrefabAsset.json", "Resource/PrefabAsset.bin", "Descriptor/PrefabAsset.desc", g_PrefabProps, "prefabs");
    AssetManager::Instance().LoadBinaryArray<PrefabProperties>("Resource/PrefabAsset.bin", g_PrefabProps);

    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Textures
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileArray<TextureProperties>("Assets/JSON/TextureAsset.json", "Resource/TextureAsset.bin", "Descriptor/TextureAsset.desc", g_TexturePropertiesProps, "textures");
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Mesh
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileArray<MeshProperties>("Assets/JSON/MeshAsset.json", "Resource/MeshAsset.bin", "Descriptor/MeshProperties.meta", g_MeshProps, "mesh");

    // Initialize global ECS interface and other singleton components
    ecsInterface.Init();

    RegisterComponentTypes();                           // Step 1: Register known types
    AutoRegisterAllReflectedComponents(ecsInterface);   // Step 2: Hook them into ECS

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

    auto navMeshSystem = ecsInterface.RegisterSystem<NavMeshSystem>();
    // Registering systems in the ECS interface here

    // ------- Add systems to engine below -------- //
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(inputHandlerSystem);
    engine->AddSystem(imguiSystem.get());
    engine->AddSystem(scriptingSystem.get());
    engine->AddSystem(navMeshSystem.get());

    // Register scene manager last
    engine->AddSystem(sceneSystem.get());

    // Initialize the engine and run the main game loop
    engine->Initialize();

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