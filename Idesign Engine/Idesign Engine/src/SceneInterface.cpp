
#include "pch.h"
#include "SceneInterface.h"
#include "ButtonComponent.h"
#include "TextureProperties.h"
#include "Coordinator.h"

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    void SceneInterface::Initialize() {}

    std::string SceneInterface::GetName()
    {
        return "SceneInterface";
    }

    void SceneInterface::Update(float deltaTime)
    {
        (void)deltaTime;

        // In Play State but not in Editor mode and not paused
        if (engineState.IsPlay() && !engineState.IsInEditor())
        {
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

                    if (mInMenu) texture.isUI = false;
                    else         texture.isUI = true;
                }
            }

            // Pause State
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

            // How To Play
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

                    //texture.isUI = engineState.IsPaused();
                }
            }
        }
    }
}