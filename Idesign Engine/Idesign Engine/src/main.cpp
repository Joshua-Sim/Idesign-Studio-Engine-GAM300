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
#include "EnginePath.h"
#include "AudioSystem.h"
#include "AudioComponent.h"
#include "ScriptingSystem.h"
#include "lua.hpp"
#include "PhysicsSystem.h"
#include "FileCheckSystem.h"
#include <EventSystem.h>
#include <EventManager.h>
#include "GameSystem.h"
#include "PlayerInteractSystem.h"
#include"ButtonSystem.h"
#include "ButtonComponent.h"

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
    // Save the initial working directory
    std::filesystem::path initialCWD = std::filesystem::current_path();
    std::cout << "Initial working directory: " << initialCWD << std::endl;

    // SET PROJECT ROOT FOR ALL SYSTEMS THAT NEED IT
    FileCheckSystem::SetProjectRoot(initialCWD.string());
    ScriptingSystem::SetProjectRoot(initialCWD.string());

    SetupSignalHandlers();
    EnableMemoryLeakChecking(); // For memory check

    //Create the core engine which manages all the systems that make up the game
    auto engine = std::make_unique<CoreEngine>();

	// Initialize FMOD Studio Audio System
    std::cout << "Initializing FMOD Studio Audio System..." << std::endl;
    
    // ------- Create systems below -------- 
    // Initialize global ECS interface and other singleton components
    ecsInterface.Init();

    //AssetManager::Instance().CompileAllAssets();
    //AssetManager::Instance().SaveAllAssets();
    AssetManager::Instance().LoadAllAssets();
    
    // To Connect Prop with ECS
    RegisterComponentTypes();                           // Step 1: Register known types
    AutoRegisterAllReflectedComponents(ecsInterface);   // Step 2: Hook them into ECS

    // Explicitly register ButtonComponent as an ECS component
    //ecsInterface.RegisterComponent<ButtonComponent>();
	//AssetManager::Instance().LoadWindowConfig(); // Load WindowConfig before Graphics system is created

    // Register graphics system and GLFW Window first 
    auto graphicsSystem = ecsInterface.RegisterSystem<Graphics>();

    // Initialize the InputHandler singleton
    InputHandler::Initialize(graphicsSystem->GetWindow());
    auto animationSystem = ecsInterface.RegisterSystem<AnimationSystem>();
    auto sceneSystem = ecsInterface.RegisterSystem<SceneManager>();
    auto imguiSystem = ecsInterface.RegisterSystem<ImguiManager>();
    auto gameSystem = ecsInterface.RegisterSystem<GameSystem>();
    auto buttonSystem = ecsInterface.RegisterSystem<ButtonSystem>();
    imguiSystem->SetGraphicsSystem(graphicsSystem.get());
    graphicsSystem->SetImguiManager(imguiSystem.get());
    
    //AssetManager::Instance().LoadAllAssets();
    
    //Access the InputHandler instance from the singleton
    InputHandler* inputHandlerSystem = InputHandler::GetInstance();
    if (!inputHandlerSystem) 
    {
        std::cerr << "Failed to initialize InputHandler!" << std::endl;
        return -1; // Exit if InputHandler initialization failed
    }
    
    // 3. Use the new getter to connect the camera to the input handler
    Camera* graphicsCamera = graphicsSystem.get()->getCamera();
    inputHandlerSystem->SetCamera(graphicsCamera);

    // Registering systems in the ECS interface here
    auto scriptingSystem = ecsInterface.RegisterSystem<ScriptingSystem>();
    scriptingSystem->SetGraphicsSystem(graphicsSystem.get());
    auto navMeshSystem = ecsInterface.RegisterSystem<NavMeshSystem>();
    auto fileCheckSystem = ecsInterface.RegisterSystem<FileCheckSystem>();
    auto audioSystem = ecsInterface.RegisterSystem<AudioSystem>();
    auto physicsSystem = ecsInterface.RegisterSystem<PhysicsSystem>();
    auto tagManager = ecsInterface.RegisterSystem<TagManager>();
    ecsInterface.RegisterComponent<PlayerComponent>(); // register player component here coz of the interact system
    auto playerInteractSystem = ecsInterface.RegisterSystem<PlayerInteractSystem>();
    auto eventSystem = ecsInterface.RegisterSystem<EventSystem>();
    ecsInterface.RegisterComponent<EventSenderComponent>();
    ecsInterface.RegisterComponent<EventReceiverComponent>();
    {
        Signature signature;
        // Tell ECS: "This system only cares about entities with EventReceiverComponent"
        signature.set(ecsInterface.GetComponentType<EventReceiverComponent>());
        ecsInterface.SetSystemSignature<EventSystem>(signature);
    }

    //buttonSystem->SetInputHandler(inputHandlerSystem);
    //{
    //    Signature buttonSig;
    //    buttonSig.set(ecsInterface.GetComponentType<ButtonComponent>());
    //    ecsInterface.SetSystemSignature<ButtonSystem>(buttonSig);
    //}
    // ------- Add systems to engine below -------- //
    engine->AddSystem(animationSystem.get());   // Calculate bone matrices
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(inputHandlerSystem);
    engine->AddSystem(imguiSystem.get());
    engine->AddSystem(scriptingSystem.get());
    engine->AddSystem(navMeshSystem.get());
	engine->AddSystem(audioSystem.get());
    engine->AddSystem(physicsSystem.get());
    engine->AddSystem(tagManager.get());
    engine->AddSystem(playerInteractSystem.get());
    engine->AddSystem(sceneSystem.get());       // Register scene manager last
    engine->AddSystem(fileCheckSystem.get());
    engine->AddSystem(eventSystem.get());
    engine->AddSystem(gameSystem.get());
    engine->AddSystem(buttonSystem.get());

    std::filesystem::path root = GetEngineRootPath();
    fileCheckSystem->WatchAllFolders();

    // Initialize the engine and run the main game loop
    engine->Initialize();

    // Initialize FMOD and load banks (after engine Initialize so AudioSystem::Initialize runs first)
    std::cout << "Initializing FMOD Studio Audio System..." << std::endl;
    audioSystem->InitializeFMOD();

    // Set maximum volumes
    audioSystem->SetMasterVolume(1.0f);
    audioSystem->SetBGMVolume(1.0f);
    audioSystem->SetSFXVolume(1.0f);

    // Set 3D listener position (required for 3D audio!)
    audioSystem->Set3DListener(
        Vector3(0, 0, 0),     // Position at origin
        Vector3(0, 0, 0),     // No velocity
        Vector3(0, 0, -1),    // Looking forward
        Vector3(0, 1, 0)      // Up is +Y
    );

    std::cout << "[Audio] Volumes and 3D listener configured!" << std::endl;
    std::cout << "FMOD Studio Audio System initialized successfully!" << std::endl;

    
    sceneManager.SetSceneBGM("jarrentest", "event:/Level1BGM");
   

    ImguiManager::Log("Initializing FontSystem...", "FontSystem");
    fontSystem.Initialize();
    fontSystem.LoadFont("Assets/Font/arial.ttf", 40, "Arial");
    fontSystem.SetActiveFont("Arial");
    ImguiManager::LogSuccess("FontSystem ready", "FontSystem");

    // Run All Scripts
    if (luaL_dofile(scriptingSystem->GetLuaState(), "Assets/Scripts/cook_all.lua") != LUA_OK) 
    {
        std::cerr << "[Cook] Lua error: " << lua_tostring(scriptingSystem->GetLuaState(), -1) << "\n";
    }
   //scriptingSystem->CreateScriptEntity("Assets/Scripts/hello.lua");
    //scriptingSystem->CreateScriptEntity("Assets/Scripts/controls.lua");
    //scriptingSystem->CreateScriptEntity("assets/scripts/prove_lua.lua");

    engine->GameLoop();

    // Restore the original directory before saving/compiling
    std::filesystem::current_path(initialCWD);
    std::cout << "Restored working directory: " << std::filesystem::current_path() << std::endl;

    AssetManager::Instance().SaveAllAssets();
    AssetManager::Instance().CompileAllAssets();

    // Clear all systems and other dynamically allocated resources
    engine->DestroySystems();
    EventManager::Instance().Reset();
    TypeRegistry::Instance().Shutdown();

    // Clean up smart pointers as they go out of scope
    // No need to manually delete as unique_ptr will handle it
    return 0;
}