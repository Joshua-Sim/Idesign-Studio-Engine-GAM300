
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
    // Put these as members/static near your SceneInterface or wherever you store cutscene state:
    static bool  cutsceneFinished = false;
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
                if (cutsceneStartTime < 0.0f)
                {
                    cutsceneStartTime = (float)glfwGetTime();
                    cutsceneFinished = false;

                    // Optional: force everything invisible at the start
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
                const int N = (int)slides.size(); // should be 13

                // Timing knobs
                const float fadeInDuration = 1.5f;
                const float gap = 0.8f;   // delay between slides
                const float holdAfterLast = 0.4f;   // pause after last is fully visible
                const float fadeOutDuration = 0.8f;  // fade everything to 0

                // Total time when last slide finishes fading in
                float lastFadeInEnd = (N > 0) ? ((N - 1) * (fadeInDuration + gap) + fadeInDuration) : 0.0f;

                // Fade-out starts after last fully visible + hold
                float fadeOutStart = lastFadeInEnd + holdAfterLast;
                float fadeOutEnd = fadeOutStart + fadeOutDuration;

                // 1) Fade-in sequence (per slide)
                for (int i = 0; i < N; ++i)
                {
                    Entity e = slides[i];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = true;

                    float startTime = i * (fadeInDuration + gap);
                    float localT = (t - startTime) / fadeInDuration;      // 0..1
                    localT = glm::clamp(localT, 0.0f, 1.0f);

                    tex.uiAlpha = localT; // just fade in to 1
                }

                // 2) Global fade-out (multiplies everything down to 0)
                if (t >= fadeOutStart)
                {
                    float outT = (t - fadeOutStart) / fadeOutDuration;     // 0..1
                    outT = glm::clamp(outT, 0.0f, 1.0f);

                    float global = 1.0f - outT; // 1 -> 0

                    for (int i = 0; i < N; ++i)
                    {
                        Entity e = slides[i];
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.uiAlpha *= global; // ensures they ALL go to 0 smoothly
                    }
                }

                // 3) Done: hard-force alpha 0 so there is NO 1-frame flash
                if (!cutsceneFinished && t >= fadeOutEnd)
                {
                    cutsceneFinished = true;

                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.uiAlpha = 0.0f;     // HARD zero
                        tex.isUI = false;       // Hide from UI pass entirely
                    }

                    cutsceneStartTime = -1.0f;

                    // Now change state ONCE (example)
                    engineState.PushState(EngineMode::Play);
                }
            }
            else
            {
                // If not in cutscene, reset flags so next time it can start cleanly
                cutsceneStartTime = -1.0f;
                cutsceneFinished = false;
            }


        }
    }
}