
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
    // somewhere persistent (SceneInterface members or static in cpp)
    static bool cutsceneFinished = false;

    // Tune these
    static const float fadeDuration = 1.2f;  // slide fade-in time
    static const float gap = 0.5f;  // delay between slides
    static const float holdAfterLast = 0.6f;  // hold before fade-out starts
    static const float fadeOutDuration = 0.8f;  // global fade-out time

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
                if (cutsceneStartTime < 0.0f)
                {
                    cutsceneStartTime = (float)glfwGetTime();
                    cutsceneFinished = false;

                    // Optional: force all cutscene slides visible but alpha 0 at start
                    auto slides = ecsInterface.GetEntitiesByTag("cutscene");
                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }
                }

                float now = (float)glfwGetTime();
                float t = now - cutsceneStartTime;

                auto slides = ecsInterface.GetEntitiesByTag("cutscene");
                // Expecting 13 images: slides[0] is your black screen
                // Slides must be created in the correct order since you said "no sort".

                const int count = (int)slides.size();
                if (count > 0)
                {
                    // 1) Slide 0: instant black alpha=1
                    {
                        Entity e0 = slides[0];
                        if (ecsInterface.IsEntityValid(e0) && ecsInterface.HasComponent<TextureProperties>(e0))
                        {
                            auto& tex0 = ecsInterface.GetComponent<TextureProperties>(e0);
                            tex0.isUI = true;
                            tex0.uiAlpha = 1.0f; // INSTANT
                        }
                    }

                    // 2) Slides 1..(count-1): sequential fade-in
                    // start time for slide i (i>=1) = (i-1) * (fadeDuration + gap)
                    for (int i = 1; i < count; ++i)
                    {
                        Entity e = slides[i];
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;

                        float startTime = (float)(i - 1) * (fadeDuration + gap);
                        float localT = (t - startTime) / fadeDuration;
                        localT = glm::clamp(localT, 0.0f, 1.0f);

                        tex.uiAlpha = localT;
                    }

                    // 3) Global fade-out after everything has faded in + hold
                    // total time for last slide fade-in to finish:
                    // lastIndex = count-1 => start = (count-2)*(fadeDuration+gap)
                    float lastStart = (float)(count - 2) * (fadeDuration + gap);
                    float fadeInEnd = lastStart + fadeDuration;
                    float fadeOutStart = fadeInEnd + holdAfterLast;

                    if (t >= fadeOutStart)
                    {
                        float outT = (t - fadeOutStart) / fadeOutDuration; // 0..1
                        outT = glm::clamp(outT, 0.0f, 1.0f);

                        float globalAlpha = 1.0f - outT; // 1 -> 0

                        // Apply globalAlpha to ALL slides (including black) so it ends fully transparent
                        for (int i = 0; i < count; ++i)
                        {
                            Entity e = slides[i];
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha *= globalAlpha;
                        }

                        // When fade-out finishes, hide all cutscene UI and push next state once.
                        if (outT >= 1.0f && !cutsceneFinished)
                        {
                            cutsceneFinished = true;

                            for (int i = 0; i < count; ++i)
                            {
                                Entity e = slides[i];
                                if (!ecsInterface.IsEntityValid(e)) continue;
                                if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                                auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                                tex.uiAlpha = 0.0f;
                                tex.isUI = false; // optional: fully remove them from UI pass
                            }

                            // Move to gameplay
                            engineState.PushState(EngineMode::Play);    // or whatever your next state is
                        }
                    }
                }
            }
            else
            {
                // Reset for next time
                cutsceneStartTime = -1.0f;
                cutsceneFinished = false;
            }

        }
    }
}