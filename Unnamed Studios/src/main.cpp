//Main, starts here 

#pragma once
#include "pch.h"
#include "Core.h"
#include "GraphicsWindows.h"
#include "Graphics.h"
#include "InputHandler.h"
#include "Coordinator.h"
#include "ComponentList.h"
#include "PhysicsSystem.h" // Dummy Physics System for demonstration purposes
#include "FontSystem.h" 
#include "MessageSystem.h"
#include "TestSystem.h"
#include "FontShader.h"
#include "AssetManager.h"
#include "Audio.h"
#include "EngineState.h"
#include "PlayerSystem.h"
#include "Collision.h"
#include "EnemyManager.h"
#include "AnimationSystem.h"
#include "LogicManager.h"

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
    //EnableMemoryLeakChecking(); // For memory check

  //Create the core engine which manages all the systems that make up the game
    auto engine = std::make_unique<CoreEngine>();

    // ------- Create systems below -------- //

    Window& myWindowConfig = GlobalAssetManager.LoadWindow("src/Data/WindowAsset.json");
    //auto AudioManager = std::make_unique<Audio>();
    auto Window = std::make_unique<GraphicsWindows>(myWindowConfig.GetConfig(), engine.get()); // Replace with serialized data

    // Initialize the InputHandler singleton
    InputHandler::Initialize(Window->GetWindow());
    GlobalLogicManager.Initialize();


    // Access the InputHandler instance from the singleton
    InputHandler* inputHandlerSystem = InputHandler::GetInstance();
    if (!inputHandlerSystem) {
        std::cerr << "Failed to initialize InputHandler!" << std::endl;
        return -1; // Exit if InputHandler initialization failed
    }

    // Initialize global ECS interface and other singleton components
    ecsInterface.Init();

    // Registering systems in the ECS interface.
    auto graphicsSystem = ecsInterface.RegisterSystem<Graphics>(Window.get());
    auto messageSystem = std::make_unique<MessageSystem>();

    inputHandlerSystem->SetMessageSystem(messageSystem.get());    // Inject the MessageSystem into InputHandler

    // ------- Add systems to engine below -------- //
    engine->AddSystem(Window.get());
    engine->AddSystem(inputHandlerSystem);
 //   engine->AddSystem(physicsSystem.get());
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(messageSystem.get());
 //   engine->AddSystem(playersystem.get());

    // Initialize the engine and run the main game loop
    engine->Initialize();
    engine->GameLoop();

    // Clear all systems and other dynamically allocated resources
    engine->DestroySystems();

    // Clean up smart pointers as they go out of scope
    // No need to manually delete as unique_ptr will handle it

    return 0;
}