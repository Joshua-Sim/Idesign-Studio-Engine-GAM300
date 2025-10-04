//Main, starts here 

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
    //SetupSignalHandler();
    EnableMemoryLeakChecking(); // For memory check

    //Create the core engine which manages all the systems that make up the game
    auto engine = std::make_unique<CoreEngine>();

    // ------- Create systems below -------- 
    //  Windows
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //CompileSingleObject<WindowConfig>("src/Data/WindowAsset.json", "src/Data/WindowAsset.cfg", g_WindowConfigProps);
    AssetManager::Instance().LoadBinary<WindowConfig>("src/Data/WindowAsset.cfg", g_WindowConfigProps);
    //auto Window = std::make_unique<GraphicsWindows>(&config, engine.get());
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    //  Audio
    // ------------------------------------------------------------------------------------------------------------------------------------------------------
    CompileArray<AudioProperties>("src/Data/AudioAsset.json", "src/Data/AudioAsset.bin", g_AudioPropertiesProps, "audio");
    AssetManager::Instance().LoadBinaryArray<AudioProperties>("src/Data/AudioAsset.bin", g_AudioPropertiesProps,
        [](AudioProperties& props)
        {
            GlobalAudio.UE_LoadSound(props.name);
        });
    //AssetManager::Instance().SaveArrayToJson(TypeName<AudioProperties>::value, "src/Data/AudioAsset.json", "AudioProperties");
    //GlobalAudio.UE_PlaySound("winloop");
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

    // Registering systems in the ECS interface here
    auto messageSystem = std::make_unique<MessageSystem>();
    inputHandlerSystem->SetMessageSystem(messageSystem.get());

    // ------- Add systems to engine below -------- //
    engine->AddSystem(graphicsSystem.get());
    engine->AddSystem(inputHandlerSystem);
    engine->AddSystem(messageSystem.get());
    engine->AddSystem(imguiSystem.get());

    // Register scene manager last
    engine->AddSystem(sceneSystem.get());

    // Initialize the engine and run the main game loop
    engine->Initialize();
    engine->GameLoop();



    // Clear all systems and other dynamically allocated resources
    engine->DestroySystems();

    // Clean up smart pointers as they go out of scope
    // No need to manually delete as unique_ptr will handle it
    return 0;
}