
#include "pch.h"
#include "SceneInterface.h"
#include "ButtonComponent.h"
#include "TextureProperties.h"
#include "Coordinator.h"

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    // Define the global instance
    SceneInterface sceneInterface;

    void SceneInterface::Initialize() {}

    std::string SceneInterface::GetName()
    {
        return "SceneInterface";
    }

    void SceneInterface::Update(float deltaTime)
    {
        (void)deltaTime;

        // In Play State but not in Editor mode and not paused
        if (engineState.IsInEditor())
        {
            // Should only trigger once, unless quit level
            auto menuEntities = ecsInterface.GetEntitiesByTag("menu");
            for (Entity e : menuEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                // Check required components before rendering
                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& button = ecsInterface.GetComponent<ButtonComponent>(e);
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);

                    // Show menu only if mInMenu is true
                    texture.isUI = engineState.IsInMenu();
                    //std::cout << engineState.IsInMenu() << std::endl;
                }
            }

            // Pause State (Triggered by ESC in Inputhandler)
            auto pauseEntities = ecsInterface.GetEntitiesByTag("pause");
            for (Entity e : pauseEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                // Check required components before rendering
                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& button = ecsInterface.GetComponent<ButtonComponent>(e);
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);

                    texture.isUI = engineState.IsPaused();
                }
            }

            // How To Play (Triggered by button click)
            auto howtoplayEntities = ecsInterface.GetEntitiesByTag("howtoplay");
            for (Entity e : howtoplayEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                // Check required components before rendering
                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& button = ecsInterface.GetComponent<ButtonComponent>(e);
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);

                    texture.isUI = engineState.InHowToPlay();
                }
            }
        }
    }
}