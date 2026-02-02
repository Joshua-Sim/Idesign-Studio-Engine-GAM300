
#include "pch.h"
#include "SceneInterface.h"
#include "ButtonComponent.h"
#include "TextureProperties.h"
#include "Coordinator.h"
#include "TextProperties.h"

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    // Define the global instance
    SceneInterface sceneInterface;

    // SceneInterface.h (or cpp static)
    static float cutsceneStartTime = -1.0f;
    static bool  cutsceneInit = false;

    void SceneInterface::Initialize() {}

    std::string SceneInterface::GetName()
    {
        return "SceneInterface";
    }

    void SceneInterface::Update(float deltaTime)
    {
        (void)deltaTime;

        // [FIX] Run this logic whenever the Game Simulation is active.
        // In Standalone, IsPlay() should always be true.
        // In Editor, IsPlay() is true only when you hit the Play button.
        if (engineState.HasState(EngineMode::Play))
        {
            // 1. MAIN MENU LOGIC
            auto menuEntities = ecsInterface.GetEntitiesByTag("menu");
            for (Entity e : menuEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::Menu); }
                }
            }

            // 2. PAUSE MENU LOGIC
            auto pauseEntities = ecsInterface.GetEntitiesByTag("pause");
            for (Entity e : pauseEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::Paused); }
                }
            }

            // 3. HOW TO PLAY LOGIC
            auto howtoplayEntities = ecsInterface.GetEntitiesByTag("howtoplay");
            for (Entity e : howtoplayEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::HowToPlay); }
                }
            }

            // 4. Upgrade
            auto upgradeEntities = ecsInterface.GetEntitiesByTag("upgrade");
            for (Entity e : upgradeEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::Upgrade); }
                }
            }

            // 5. Settings (Vol Control)
            auto settingsEntities = ecsInterface.GetEntitiesByTag("settings");
            for (Entity e : settingsEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    // Only enforce UI flag if NOT in editor mode 
                    if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::Settings); }
                }

                if (ecsInterface.HasComponent<TextProperties>(e))
                {
                    auto& text = ecsInterface.GetComponent<TextProperties>(e);
                    
                    // Only enforce opacity if NOT in editor mode 
                    if (!engineState.Is(EngineMode::Editor)) { text.opacity = engineState.Is(EngineMode::Settings) ? 1.f : 0.f; }
                }
            }

            // 6. Cutscene (Open after click Play)
            if (engineState.Is(EngineMode::Cutscene))
            {
                // start timer once
                if (cutsceneStartTime < 0.0f)
                    cutsceneStartTime = (float)glfwGetTime();

                float now = (float)glfwGetTime();

                // SLOW DOWN / SPEED UP HERE
                const float timeScale = 0.6f; // <--- smaller = slower
                float t = (now - cutsceneStartTime) * timeScale;

                auto slides = ecsInterface.GetEntitiesByTag("cutscene");

                // If you REALLY want hard-coded order, DO NOT rely on vector order.
                // Best: tag them cutscene0..cutscene12 and fetch each tag separately.
                // But if you're ok assuming creation order, this works.

                const float fadeDuration = 1.0f; // slower fade per slide
                const float gap = 0.8f; // pause between fades
                const float holdAfter = 1.0f; // optional hold at end before switching

                // init once: force all alphas to 0 at the start
                if (!cutsceneInit)
                {
                    cutsceneInit = true;
                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f; // start invisible
                    }
                }

                // Fade each slide in sequence
                for (size_t i = 0; i < slides.size(); ++i)
                {
                    Entity e = slides[i];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = true;

                    float startTime = (float)i * (fadeDuration + gap);
                    float localT = (t - startTime) / fadeDuration;   // 0..1 over fadeDuration
                    localT = glm::clamp(localT, 0.0f, 1.0f);

                    tex.uiAlpha = localT; // 0 -> 1
                }

                // AUTO TRANSITION when done
                float totalTime =
                    (float)slides.size() * fadeDuration +
                    (float)(slides.size() - 1) * gap +
                    holdAfter;

                if (t >= totalTime)
                {
                    // reset for next time
                    cutsceneStartTime = -1.0f;
                    cutsceneInit = false;

                    // move to next state (example)
                    engineState.PushState(EngineMode::Play); // or Menu/Game/etc
                }
            }
            else
            {
                // reset when not in cutscene
                cutsceneStartTime = -1.0f;
                cutsceneInit = false;
            }
        }
    }
}