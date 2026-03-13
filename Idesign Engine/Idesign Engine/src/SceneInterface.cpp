#include "pch.h"
#include "SceneInterface.h"
#include "ButtonComponent.h"
#include "TextureProperties.h"
#include "Coordinator.h"
#include "TextProperties.h"
#include "TransformProperties.h"
#include "Graphics.h"
#include "InputHandler.h"
#include <PhaseManager.h>
#include <TimerComponent.h>
#include "AudioSystem.h"

extern Framework::Coordinator ecsInterface;

namespace Framework
{
    // Define the global instance
    SceneInterface sceneInterface;

    // ======= CUTSCENE TUNING =======
    static bool cutsceneFinished = false;
    static bool IntroFinished = false;

    static const float sceneDuration = 6.0f;   // each scene lasts x second(s) total
    static const float crossfadeDuration = 0.25f;  // fast seamless fade (tweak 0.12 ~ 0.25)
    static const int   kExpectedScenes = 10;

    static float cutsceneStartTime = -1.0f;
    static float fadeOutStartTime = -1.0f;

    static bool  cutsceneSkipHolding = false;
    static float cutsceneSkipStartTime = 0.0f;
    static const float cutsceneSkipHoldDuration = 2.0f;

    // ===== INTRO TUNING =====
    static const float introSceneDuration = 3.0f;
    static const float introCrossfadeDuration = 1.0f; // quick, smooth
    static const float introStartDelay = 3.0f; // wait 3 seconds before logo 1 fades in
    static float IntrocutsceneStartTime = -2.0f;
    static float IntrofadeOutStartTime = -1.0f;

    // ===== DAY PHASE TUNING =====
    static float dayPhaseStartTime = -1.0f;
    static bool  dayPhaseFinished = false;

    static const float dayPhaseFadeInDuration = 0.90f;
    static const float dayPhaseSlideDuration = 2.0f;
    static const float dayPhaseHoldDuration = 1.0f;
    static const float dayPhaseFadeOutDuration = 0.8f;

    static const float dayPhaseStartX = 3300.0f; // start off-screen
    static const float dayPhaseCenterX = 960.0f; // centered

	// DO NOT TOUCH THIS 
    // DAY COUNTER 
    int SceneInterface::g_CurrentDay = 2;

    // ======== WIN CUTSCENE TUNING ============ 
    static float winCutsceneStartTime = -1.0f;
    static bool  winCutsceneFinished = false;

    static const float winStartDelay = 3.0f;          // hold black first
    static const float winSceneDuration = 5.0f;       // per scene
    static const float winCrossfadeDuration = 1.8f;   // fade between scenes

    // ========= CREDITS TUNING ===============
    static float creditStartTime = -1.0f;
    static bool  creditFinished = false;

    static const float creditStartDelay = 1.0f;       // black screen hold
    static const float creditPanelScrollDuration = 5.0f;
    static const float creditPanelFadeInDuration = 0.6f;
    static const float creditPanelStaggerDelay = 1.0f;

    static const float creditStartY = -1400.0f; // below screen
    static const float creditEndY = 1400.0f;  // above screen


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

            // Upgrade Logic
            if (engineState.Is(EngineMode::Upgrade))
            {
                auto upgradeEntities = ecsInterface.GetEntitiesByTag("upgrade");

                // NEXT DAY BUTTON ONLY
                auto nextDayButt = ecsInterface.GetEntitiesByTag("nextdaybutt");
                for (Entity e : nextDayButt)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;

                    if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                        ecsInterface.HasComponent<TextureProperties>(e))
                    {
                        auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                        if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::Upgrade); }
                    }
                }

                auto* input = InputHandler::GetInstance();
                if (input && input->IsKeyPressed(GLFW_KEY_KP_8))
                {

                    engineState.PopState();                   // remove Intro
                    engineState.PushState(EngineMode::Win); // go to Menu

                    // If your engine has SetState / ChangeState, that's even better:
                    // engineState.SetState(EngineMode::Menu);

                    return;
                }

                auto& upgradeMgr = UpgradeManager::Instance();

                // ------------------------------------------------------------
                // 1. Cache hover states from the MAIN buttons
                // ------------------------------------------------------------
                bool cauldronHovered = false;
                bool goldHovered = false;
                bool inventoryHovered = false;

                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<ButtonComponent>(e)) continue;

                    auto& btn = ecsInterface.GetComponent<ButtonComponent>(e);

                    if (ecsInterface.EntityHasTag(e, "cauldron"))    cauldronHovered = btn.isHovered;
                    if (ecsInterface.EntityHasTag(e, "gold"))        goldHovered = btn.isHovered;
                    if (ecsInterface.EntityHasTag(e, "inventory"))   inventoryHovered = btn.isHovered;
                }

                // ------------------------------------------------------------
                // 2. Affordability and Completion checks
                // ------------------------------------------------------------
                bool canAffordCauldron = upgradeMgr.CanAffordCauldronUpgrade();
                bool canAffordGold = upgradeMgr.CanAffordCurrencyUpgrade();
                bool canAffordInventory = upgradeMgr.CanAffordInventoryUpgrade();

                // Completion checks
                bool cauldronComplete = upgradeMgr.IsCauldronUnlocked();
                bool goldComplete = upgradeMgr.IsCurrencyMaxLevel();
                bool backpackComplete = upgradeMgr.IsBackpackMaxLevel();

                // ------------------------------------------------------------
                // 3. Apply visibility to ALL upgrade UI entities
                // ------------------------------------------------------------
                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                    if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(e);

                    texture.isUI = true; // always UI in upgrade screen
                    bool shouldShow = false;
                    float zLayer = 0.3f;

                    // -------------------------
                    // COMPLETE STAMPS (highest priority - always show if complete)
                    // -------------------------
                    if (ecsInterface.EntityHasTag(e, "cauldroncomplete"))
                    {
                        shouldShow = cauldronComplete;
                        zLayer = 0.9f;
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldcomplete"))
                    {
                        shouldShow = goldComplete;
                        zLayer = 0.9f;
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventorycomplete"))
                    {
                        shouldShow = backpackComplete;
                        zLayer = 0.9f;
                    }

                    // -------------------------
                    // CAULDRON STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "cauldronpass"))
                    {
                        shouldShow = (!cauldronComplete && cauldronHovered && canAffordCauldron);
                        zLayer = 0.6f;
                    }
                    else if (ecsInterface.EntityHasTag(e, "cauldronfail"))
                    {
                        shouldShow = (!cauldronComplete && cauldronHovered && !canAffordCauldron);
                        zLayer = 0.6f;
                    }

                    // -------------------------
                    // GOLD STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "goldpass"))
                    {
                        shouldShow = (!goldComplete && goldHovered && canAffordGold);
                        zLayer = 0.6f;
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldfail"))
                    {
                        shouldShow = (!goldComplete && goldHovered && !canAffordGold);
                        zLayer = 0.6f;
                    }

                    // -------------------------
                    // INVENTORY STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "inventorypass"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && canAffordInventory);
                        zLayer = 0.6f;
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventoryfail"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && !canAffordInventory);
                        zLayer = 0.6f;
                    }

                    // ------------------------------------------------------------
                    // MAIN BUTTON BACKGROUNDS (base/unhovered versions)
                    // ------------------------------------------------------------
                    else
                    {
                        bool isCauldronBase = ecsInterface.EntityHasTag(e, "cauldron");
                        bool isGoldBase = ecsInterface.EntityHasTag(e, "gold");
                        bool isInventoryBase = ecsInterface.EntityHasTag(e, "inventory");

                        // Hide base card if hovered OR if completed
                        bool hideBase =
                            (isCauldronBase && (cauldronHovered || cauldronComplete)) ||
                            (isGoldBase && (goldHovered || goldComplete)) ||
                            (isInventoryBase && (inventoryHovered || backpackComplete));

                        shouldShow = !hideBase;
                        zLayer = 0.3f;
                    }

                    // Use isUI for visibility, z for layering
                    texture.isUI = shouldShow;
                    transform.position.z = zLayer;
                }
            }
            else
            {
                // NOT in Upgrade mode - hide all upgrade-tagged entities
                auto upgradeEntities = ecsInterface.GetEntitiesByTag("upgrade");
                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (ecsInterface.HasComponent<TextureProperties>(e))
                    {
                        auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                        texture.isUI = false;  // Hide when not in Upgrade mode
                    }
                }

                // NOT in Upgrade mode - hide all upgrade-tagged entities
                auto nextDayButt = ecsInterface.GetEntitiesByTag("nextdaybutt");
                for (Entity e : nextDayButt)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;

                    if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                        ecsInterface.HasComponent<TextureProperties>(e))
                    {
                        auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                        if (!engineState.Is(EngineMode::Editor)) { texture.isUI = false; }
                    }
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

            // 6. MAP SELECTION LOGIC
            auto mapEntities = ecsInterface.GetEntitiesByTag("map");
            for (Entity e : mapEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    if (!engineState.Is(EngineMode::Editor)) {
                        texture.isUI = engineState.HasState(EngineMode::MapSelection) ? 1.f : 0.f;
                    }
                }
            }


            // 7. Cutscene (Open after click Play)
            if (engineState.Is(EngineMode::Cutscene))
            {
                auto slides = ecsInterface.GetEntitiesByTag("cutscene");
                int count = (int)slides.size();

                // Expecting 9 scenes (but clamp safely)
                int sceneCount = glm::min(count, kExpectedScenes);
                if (sceneCount <= 0) return;

                // -------------------------------------------------
                // SKIP CUTSCENE INPUT (put this EARLY, before returns)
                // -------------------------------------------------
                // Hold LMB for 2 seconds to skip
                auto* grapInput = Graphics::GetInstance();
                if (glfwGetMouseButton(grapInput->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                {
                    if (!cutsceneSkipHolding)
                    {
                        cutsceneSkipHolding = true;
                        cutsceneSkipStartTime = (float)glfwGetTime();
                    }

                    float heldTime = (float)glfwGetTime() - cutsceneSkipStartTime;

                    if (heldTime >= cutsceneSkipHoldDuration)
                    {
                        for (int s = 0; s < sceneCount; ++s)
                        {
                            Entity e = slides[s];
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        cutsceneStartTime = -1.0f;
                        cutsceneFinished = true;

                        cutsceneSkipHolding = false;
                        cutsceneSkipStartTime = 0.0f;

                        if (g_AudioSystem) {
                            g_AudioSystem->StopEvent("event:/QuarryBGM");
                            g_AudioSystem->PlayAudio("event:/Level1BGM");
                        }

                        engineState.PopState();
                        engineState.PushState(EngineMode::MapSelection);
                        return;
                    }
                }
                else
                {
                    cutsceneSkipHolding = false;
                    cutsceneSkipStartTime = 0.0f;
                }

                if (cutsceneStartTime < 0.0f)
                {
                    cutsceneStartTime = (float)glfwGetTime();
                    cutsceneFinished = false;

                    // Play quarry BGM during cutscene
                    if (g_AudioSystem) {
                        g_AudioSystem->StopEvent("event:/Level1BGM");
                        g_AudioSystem->PlayAudio("event:/QuarryBGM");
                    }

                    // Init: make them UI fullscreen images, all hidden
                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    // Scene 0 (black screen) instant on
                    if (ecsInterface.IsEntityValid(slides[0]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[0]))
                    {
                        ecsInterface.GetComponent<TextureProperties>(slides[0]).uiAlpha = 1.0f;
                    }
                }

                float now = (float)glfwGetTime();
                float t = now - cutsceneStartTime;

                // Which scene are we in?
                int i = (int)glm::floor(t / sceneDuration);

                // Finished all scenes?
                if (i >= sceneCount)
                {
                    if (!cutsceneFinished)
                    {
                        cutsceneFinished = true;

                        for (int s = 0; s < sceneCount; ++s)
                        {
                            Entity e = slides[s];
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        if (g_AudioSystem) {
                            g_AudioSystem->StopEvent("event:/QuarryBGM");
                            g_AudioSystem->PlayAudio("event:/Level1BGM");
                        }

                        engineState.PopState();
                        engineState.PushState(EngineMode::MapSelection);
                    }
                    return;
                }

                float sceneT = t - (float)i * sceneDuration;
                float fadeStart = sceneDuration - crossfadeDuration;

                float blend = 0.0f;
                if (sceneT >= fadeStart)
                    blend = Saturate((sceneT - fadeStart) / crossfadeDuration);

                // Clear all alphas first
                for (int s = 0; s < sceneCount; ++s)
                {
                    Entity e = slides[s];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = true;
                    tex.uiAlpha = 0.0f;
                }

                // Current slide
                {
                    Entity cur = slides[i];
                    if (ecsInterface.IsEntityValid(cur) && ecsInterface.HasComponent<TextureProperties>(cur))
                    {
                        auto& texCur = ecsInterface.GetComponent<TextureProperties>(cur);
                        texCur.isUI = true;
                        texCur.uiAlpha = 1.0f - blend;
                    }
                }

                // Next slide
                if (i + 1 < sceneCount)
                {
                    Entity nxt = slides[i + 1];
                    if (ecsInterface.IsEntityValid(nxt) && ecsInterface.HasComponent<TextureProperties>(nxt))
                    {
                        auto& texNext = ecsInterface.GetComponent<TextureProperties>(nxt);
                        texNext.isUI = true;
                        texNext.uiAlpha = blend;
                    }
                }
            }
            else
            {
                // Reset for next time
                cutsceneStartTime = -1.0f;
                cutsceneFinished = false;
                cutsceneSkipHolding = false;
                cutsceneSkipStartTime = 0.0f;
            }

            // 8. DIGIPEN LOGO INTRO + GROUP LOGO INTRO
            if (engineState.Is(EngineMode::Intro))
            {
                auto slides = ecsInterface.GetEntitiesByTag("intro");
                int sceneCount = (int)slides.size();
                if (sceneCount <= 0) return;

                const int bgIndex = 0;
                const int logoCount = sceneCount - 1;

                // -------------------------------------------------
                // SKIP INTRO INPUT (put this EARLY, before any return)
                // -------------------------------------------------
                auto* input = InputHandler::GetInstance();
                if (input && input->IsMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT))
                {
                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.uiAlpha = 0.0f;
                        tex.isUI = false;
                    }

                    IntrocutsceneStartTime = -1.0f;
                    IntroFinished = true;

                    // IMPORTANT:
                    // If Intro is the current state, remove/replace it properly.
                    // Prefer ONE of these depending on your engine design:

                    engineState.PopState();                   // remove Intro
                    engineState.PushState(EngineMode::Menu); // go to Menu

                    // If your engine has SetState / ChangeState, that's even better:
                    // engineState.SetState(EngineMode::Menu);

                    return;
                }

                if (logoCount <= 0)
                {
                    if (ecsInterface.IsEntityValid(slides[0]) && ecsInterface.HasComponent<TextureProperties>(slides[0]))
                    {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[0]);
                        tex.isUI = true;
                        tex.uiAlpha = 1.0f;
                    }
                    return;
                }

                if (IntrocutsceneStartTime < 0.0f)
                {
                    IntrocutsceneStartTime = (float)glfwGetTime();
                    IntroFinished = false;

                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    if (ecsInterface.IsEntityValid(slides[bgIndex]) && ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                    {
                        auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                        bg.isUI = true;
                        bg.uiAlpha = 1.0f;
                    }
                }

                float totalT = (float)glfwGetTime() - IntrocutsceneStartTime;

                if (totalT < introStartDelay)
                {
                    for (int s = 1; s < sceneCount; ++s)
                    {
                        if (!ecsInterface.IsEntityValid(slides[s])) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(slides[s])) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[s]);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    if (ecsInterface.IsEntityValid(slides[bgIndex]) && ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                    {
                        auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                        bg.isUI = true;
                        bg.uiAlpha = 1.0f;
                    }

                    return;
                }

                float t = totalT - introStartDelay;
                int logoIndex = (int)glm::floor(t / introSceneDuration);

                if (logoIndex >= logoCount)
                {
                    if (!IntroFinished)
                    {
                        IntroFinished = true;

                        for (auto e : slides)
                        {
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        engineState.PopState();
                        engineState.PushState(EngineMode::Menu);
                    }
                    return;
                }

                float sceneT = t - (float)logoIndex * introSceneDuration;
                float fadeIn = Saturate(sceneT / introCrossfadeDuration);

                for (int s = 1; s < sceneCount; ++s)
                {
                    if (!ecsInterface.IsEntityValid(slides[s])) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(slides[s])) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[s]);
                    tex.isUI = true;
                    tex.uiAlpha = 0.0f;
                }

                if (ecsInterface.IsEntityValid(slides[bgIndex]) && ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                {
                    auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                    bg.isUI = true;
                    bg.uiAlpha = 1.0f;
                }

                int currentSlide = logoIndex + 1;
                if (currentSlide < sceneCount)
                {
                    if (ecsInterface.IsEntityValid(slides[currentSlide]) && ecsInterface.HasComponent<TextureProperties>(slides[currentSlide]))
                    {
                        auto& texCur = ecsInterface.GetComponent<TextureProperties>(slides[currentSlide]);
                        texCur.uiAlpha = fadeIn;
                    }
                }

                if (logoIndex > 0)
                {
                    int prevSlide = currentSlide - 1;
                    if (prevSlide >= 1 && prevSlide < sceneCount)
                    {
                        if (ecsInterface.IsEntityValid(slides[prevSlide]) && ecsInterface.HasComponent<TextureProperties>(slides[prevSlide]))
                        {
                            auto& texPrev = ecsInterface.GetComponent<TextureProperties>(slides[prevSlide]);
                            texPrev.uiAlpha = 1.0f - fadeIn;
                        }
                    }
                }
            }
            else
            {
                IntrocutsceneStartTime = -1.0f;
                IntroFinished = false;
            }

            // 9. DAY PHASE TRANSITION
            if (
                engineState.Is(EngineMode::Day2) ||
                engineState.Is(EngineMode::Day3) ||
                engineState.Is(EngineMode::Day4) ||
                engineState.Is(EngineMode::Day5)
                )
            {
                std::vector<Entity> dayEntities;

                if (engineState.Is(EngineMode::Day2))
                    dayEntities = ecsInterface.GetEntitiesByTag("day2phase");
                else if (engineState.Is(EngineMode::Day3))
                    dayEntities = ecsInterface.GetEntitiesByTag("day3phase");
                else if (engineState.Is(EngineMode::Day4))
                    dayEntities = ecsInterface.GetEntitiesByTag("day4phase");
                else if (engineState.Is(EngineMode::Day5))
                    dayEntities = ecsInterface.GetEntitiesByTag("day5phase");

                int totalEntities = (int)dayEntities.size();
                if (totalEntities < 3) return; // need exactly 3 pieces

                // Expected:
                // [0] = permanent black background
                // [1] = moving day image
                // [2] = black fade-out overlay
                Entity bgEntity = dayEntities[0];
                Entity bannerEntity = dayEntities[1];
                Entity fadeEntity = dayEntities[2];

                const float fadeInEnd = dayPhaseFadeInDuration;
                const float slideEnd = fadeInEnd + dayPhaseSlideDuration;
                const float holdEnd = slideEnd + dayPhaseHoldDuration;
                const float fadeOutEnd = holdEnd + dayPhaseFadeOutDuration;

                if (dayPhaseStartTime < 0.0f)
                {
                    dayPhaseStartTime = (float)glfwGetTime();
                    dayPhaseFinished = false;

                    // Init all 3 entities
                    for (Entity e : dayEntities)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    // Background black: instant on
                    if (ecsInterface.IsEntityValid(bgEntity) &&
                        ecsInterface.HasComponent<TextureProperties>(bgEntity))
                    {
                        auto& bgTex = ecsInterface.GetComponent<TextureProperties>(bgEntity);
                        bgTex.isUI = true;
                        bgTex.uiAlpha = 1.0f;
                    }

                    // Moving banner starts off-screen left
                    if (ecsInterface.IsEntityValid(bannerEntity) &&
                        ecsInterface.HasComponent<TextureProperties>(bannerEntity) &&
                        ecsInterface.HasComponent<TransformComponent>(bannerEntity))
                    {
                        auto& bannerTex = ecsInterface.GetComponent<TextureProperties>(bannerEntity);
                        auto& bannerTr = ecsInterface.GetComponent<TransformComponent>(bannerEntity);

                        bannerTex.isUI = true;
                        bannerTex.uiAlpha = 0.0f;
                        bannerTr.position.x = dayPhaseStartX;
                    }

                    // Fade overlay starts invisible
                    if (ecsInterface.IsEntityValid(fadeEntity) &&
                        ecsInterface.HasComponent<TextureProperties>(fadeEntity))
                    {
                        auto& fadeTex = ecsInterface.GetComponent<TextureProperties>(fadeEntity);
                        fadeTex.isUI = true;
                        fadeTex.uiAlpha = 0.0f;
                    }
                }

                float t = (float)glfwGetTime() - dayPhaseStartTime;

                // Done
                if (t >= fadeOutEnd)
                {
                    if (!dayPhaseFinished)
                    {
                        dayPhaseFinished = true;

                        for (Entity e : dayEntities)
                        {
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        engineState.PopState();
                        engineState.PushState(EngineMode::MapSelection);
                    }
                    return;
                }

                // -------------------------------------------------
                // 1) Permanent black background
                // -------------------------------------------------
                if (ecsInterface.IsEntityValid(bgEntity) &&
                    ecsInterface.HasComponent<TextureProperties>(bgEntity))
                {
                    auto& bgTex = ecsInterface.GetComponent<TextureProperties>(bgEntity);
                    bgTex.isUI = true;
                    bgTex.uiAlpha = 1.0f;
                }

                // -------------------------------------------------
                // 2) Moving banner
                //    - fade in
                //    - slide from left to center
                //    - hold in middle
                // -------------------------------------------------
                if (ecsInterface.IsEntityValid(bannerEntity) &&
                    ecsInterface.HasComponent<TextureProperties>(bannerEntity) &&
                    ecsInterface.HasComponent<TransformComponent>(bannerEntity))
                {
                    auto& bannerTex = ecsInterface.GetComponent<TextureProperties>(bannerEntity);
                    auto& bannerTr = ecsInterface.GetComponent<TransformComponent>(bannerEntity);

                    bannerTex.isUI = true;

                    // Fade in only during first segment
                    if (t < fadeInEnd)
                        bannerTex.uiAlpha = Saturate(t / dayPhaseFadeInDuration);
                    else
                        bannerTex.uiAlpha = 1.0f;

                    // Slide X movement
                    if (t < fadeInEnd)
                    {
                        bannerTr.position.x = dayPhaseStartX;
                    }
                    else if (t < slideEnd)
                    {
                        float slideT = (t - fadeInEnd) / dayPhaseSlideDuration;
                        slideT = Saturate(slideT);

                        float eased = slideT * slideT * (3.0f - 2.0f * slideT); // smoothstep
                        bannerTr.position.x = glm::mix(dayPhaseStartX, dayPhaseCenterX, eased);
                    }
                    else
                    {
                        bannerTr.position.x = dayPhaseCenterX;
                    }
                }

                // -------------------------------------------------
                // 3) Fade-to-black overlay
                //    starts only at the end
                // -------------------------------------------------
                if (ecsInterface.IsEntityValid(fadeEntity) &&
                    ecsInterface.HasComponent<TextureProperties>(fadeEntity))
                {
                    auto& fadeTex = ecsInterface.GetComponent<TextureProperties>(fadeEntity);
                    fadeTex.isUI = true;

                    if (t < holdEnd)
                    {
                        fadeTex.uiAlpha = 0.0f;
                    }
                    else
                    {
                        float fadeOutT = (t - holdEnd) / dayPhaseFadeOutDuration;
                        fadeTex.uiAlpha = Saturate(fadeOutT); // 0 -> 1
                    }
                }
            }
            else
            {
                dayPhaseStartTime = -1.0f;
                dayPhaseFinished = false;
            }

            // WIN CUTSCENE
            if (engineState.Is(EngineMode::Win))
            {
                auto slides = ecsInterface.GetEntitiesByTag("wincs");
                int sceneCount = (int)slides.size();
                if (sceneCount <= 0) return;

                const int bgIndex = 0;                  // permanent black background
                const int animatedCount = sceneCount - 1; // everything after background

                if (animatedCount <= 0)
                {
                    if (ecsInterface.IsEntityValid(slides[0]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[0]))
                    {
                        auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[0]);
                        tex.isUI = true;
                        tex.uiAlpha = 1.0f;
                    }
                    return;
                }

                // 1) Init
                if (winCutsceneStartTime < 0.0f)
                {
                    winCutsceneStartTime = (float)glfwGetTime();
                    winCutsceneFinished = false;

                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    // Permanent black background ON
                    if (ecsInterface.IsEntityValid(slides[bgIndex]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                    {
                        auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                        bg.isUI = true;
                        bg.uiAlpha = 1.0f;
                    }
                }

                float totalT = (float)glfwGetTime() - winCutsceneStartTime;

                // Hold on pure black first
                if (totalT < winStartDelay)
                {
                    for (int s = 1; s < sceneCount; ++s)
                    {
                        if (!ecsInterface.IsEntityValid(slides[s])) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(slides[s])) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[s]);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;
                    }

                    if (ecsInterface.IsEntityValid(slides[bgIndex]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                    {
                        auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                        bg.isUI = true;
                        bg.uiAlpha = 1.0f;
                    }

                    return;
                }

                // Start animated sequence after delay
                float t = totalT - winStartDelay;

                // Animated slides are [1..sceneCount-1]
                int animIndex = (int)glm::floor(t / winSceneDuration);

                // Finished sequence
                if (animIndex >= animatedCount)
                {
                    if (!winCutsceneFinished)
                    {
                        winCutsceneFinished = true;

                        for (auto e : slides)
                        {
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        //NEXT STATE
                        engineState.PushState(EngineMode::Credit);
                    }
                    return;
                }

                float sceneT = t - (float)animIndex * winSceneDuration;
                float fadeStart = winSceneDuration - winCrossfadeDuration;
                float blend = 0.0f;

                if (sceneT >= fadeStart)
                    blend = Saturate((sceneT - fadeStart) / winCrossfadeDuration);

                // Clear all animated slides first
                for (int s = 1; s < sceneCount; ++s)
                {
                    Entity e = slides[s];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = true;
                    tex.uiAlpha = 0.0f;
                }

                // Keep permanent black background visible the whole time
                if (ecsInterface.IsEntityValid(slides[bgIndex]) &&
                    ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                {
                    auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                    bg.isUI = true;
                    bg.uiAlpha = 1.0f;
                }

                // Current animated slide
                int currentSlide = animIndex + 1; // maps 0->1, 1->2, ...
                if (currentSlide < sceneCount)
                {
                    if (ecsInterface.IsEntityValid(slides[currentSlide]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[currentSlide]))
                    {
                        auto& texCur = ecsInterface.GetComponent<TextureProperties>(slides[currentSlide]);
                        texCur.uiAlpha = 1.0f;

                        if (sceneT >= fadeStart)
                            texCur.uiAlpha = 1.0f - blend;
                    }
                }

                // Next animated slide
                int nextSlide = currentSlide + 1;
                if (nextSlide < sceneCount)
                {
                    if (ecsInterface.IsEntityValid(slides[nextSlide]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[nextSlide]))
                    {
                        auto& texNext = ecsInterface.GetComponent<TextureProperties>(slides[nextSlide]);
                        texNext.uiAlpha = blend;
                    }
                }
            }
            else
            {
                winCutsceneStartTime = -1.0f;
                winCutsceneFinished = false;
            }

            // CREDIT CUTSCENE
            // CREDIT CUTSCENE
            if (engineState.Is(EngineMode::Credit))
            {
                auto slides = ecsInterface.GetEntitiesByTag("credit");
                int sceneCount = (int)slides.size();
                if (sceneCount < 2) return;

                const int bgIndex = 0;

                const float startY = -900.0f;
                const float endY = 2500.0f;
                const float speed = 300.0f;   // movement speed

                if (creditStartTime < 0.0f)
                {
                    creditStartTime = (float)glfwGetTime();
                    creditFinished = false;

                    // Init panels
                    for (int i = 0; i < sceneCount; ++i)
                    {
                        Entity e = slides[i];
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 1.0f;
                    }

                    // Place panels at bottom
                    for (int i = 1; i < sceneCount; ++i)
                    {
                        Entity e = slides[i];
                        if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                        auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
                        tr.position.y = startY;
                    }
                }

                float t = (float)glfwGetTime() - creditStartTime;

                // Background always visible
                if (ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                {
                    auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                    bg.isUI = true;
                    bg.uiAlpha = 1.0f;
                }

                bool allFinished = true;

                for (int i = 1; i < sceneCount; ++i)
                {
                    Entity e = slides[i];

                    if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tr = ecsInterface.GetComponent<TransformComponent>(e);
                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);

                    float delay = (i - 1) * 5.0f; // 0s, 2s, 4s, 6s

                    if (t >= delay)
                    {
                        float localT = t - delay;

                        tr.position.y = startY + speed * localT;
                        tex.isUI = true;

                        if (tr.position.y < endY)
                            allFinished = false;
                    }
                    else
                    {
                        tex.isUI = false;
                        allFinished = false;
                    }
                }

                if (allFinished && !creditFinished)
                {
                    creditFinished = true;

                    for (auto e : slides)
                    {
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = false;
                    }

                    engineState.PopState();
                    engineState.PushState(EngineMode::Menu);
                }
            }
            else
            {
                creditStartTime = -1.0f;
                creditFinished = false;
            }

            // ===================== END OF CUTSCENES / LOGIC SCENES ========================== //

            // Coin & Sun Icon + Day Text
            bool isDayPlay = engineState.Is(EngineMode::Play) &&
                phaseManager->GetCurrentPhase() == GamePhase::Day;

            auto coinSunEntity = ecsInterface.GetEntitiesByTag("coinsunicon");
            for (Entity e : coinSunEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = isDayPlay;
                }
            }

            Entity dayTextEntity = ecsInterface.GetEntityByName("dayText");
            if (ecsInterface.IsEntityValid(dayTextEntity) &&
                ecsInterface.HasComponent<TextProperties>(dayTextEntity))
            {
                auto& text = ecsInterface.GetComponent<TextProperties>(dayTextEntity);
                text.opacity = isDayPlay ? 1.0f : 0.0f;

                if (isDayPlay)
                    text.text = "Day " + std::to_string(g_CurrentDay);
            }

            // Coin & Moon Icon + Night Text
            bool isNightPlay = engineState.Is(EngineMode::Play) &&
                phaseManager->GetCurrentPhase() == GamePhase::Night;

            bool showCoinUI = isNightPlay || engineState.Is(EngineMode::Upgrade);

            auto coinMoonEntity = ecsInterface.GetEntitiesByTag("coinmoonicon");
            for (Entity e : coinMoonEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = showCoinUI;
                }
            }

            Entity nightTextEntity = ecsInterface.GetEntityByName("nightText");
            if (ecsInterface.IsEntityValid(nightTextEntity) &&
                ecsInterface.HasComponent<TextProperties>(nightTextEntity))
            {
                auto& text = ecsInterface.GetComponent<TextProperties>(nightTextEntity);
                text.opacity = showCoinUI ? 1.0f : 0.0f;
                if (isNightPlay)
                    text.text = "Night " + std::to_string(g_CurrentDay);
            }

            // Mini Controls
            auto miniControlsEntity = ecsInterface.GetEntitiesByTag("minicontrols");
            for (Entity e : miniControlsEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    bool shouldShow = false;

                    std::string name = ecsInterface.GetEntityName(e);

                    if (name == "minicontrolUI")
                    {
                        // Only show if inventory has been upgraded
                        auto& inv = Inventory::Instance();
                        bool inventoryUnlocked = inv.GetBackpackLevel() > 0;
                        shouldShow = engineState.Is(EngineMode::Play) && inventoryUnlocked;
                    }
                    else if (name == "minicontrolUI_2")
                    {
                        // Always show during play
                        shouldShow = engineState.Is(EngineMode::Play);
                    }
                    else if (name == "minicontrolUI_3")
                    {
                        // Only show if Quarry has been unlocked via Day Counter
                        bool quarryUnlocked = SceneInterface::GetCurrentDay() >= 3;
                        shouldShow = engineState.Is(EngineMode::Play) && quarryUnlocked;
                    }
                    texture.isUI = shouldShow;
                }
            }

            // Clock Hand Controls
            auto clockHandIcon = ecsInterface.GetEntitiesByTag("clockhandicon");
            for (Entity e : clockHandIcon)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    bool shouldShow = engineState.Is(EngineMode::Play);
                    texture.isUI = shouldShow;
                }
            }

            // Clock Icon + Phase Timer Icon + Phase Timer Text
            Entity phaseTimerText = ecsInterface.GetEntityByName("PhaseTimerText");
            bool isPlay = engineState.Is(EngineMode::Play);

            // --- Clock Icon: isUI + Rotation ---
            auto clockEntities = ecsInterface.GetEntitiesByTag("clockicon");
            for (Entity e : clockEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;

                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = isPlay;
                }

                if (isPlay &&
                    ecsInterface.IsEntityValid(phaseTimerText) &&
                    ecsInterface.HasComponent<TransformComponent>(e) &&
                    ecsInterface.HasComponent<TimerComponent>(phaseTimerText))
                {
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(e);
                    auto& timer = ecsInterface.GetComponent<TimerComponent>(phaseTimerText);

                    if (timer.isRunning)
                    {
                        float progress = 1.0f - timer.GetProgress();
                        float baseAngle = (phaseManager->GetCurrentPhase() == GamePhase::Night) ? 180.0f : 0.0f;
                        transform.rotation.z = baseAngle + (progress * 180.0f);
                    }
                }
            }

            // --- Phase Timer Icon: isUI only ---
            auto phaseTimerEntity = ecsInterface.GetEntitiesByTag("phasetimericon");
            for (Entity e : phaseTimerEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = isPlay;
                }
            }

            // --- Phase Timer Text: opacity + MM:SS display ---
            if (ecsInterface.IsEntityValid(phaseTimerText))
            {
                if (ecsInterface.HasComponent<TextProperties>(phaseTimerText))
                {
                    auto& text = ecsInterface.GetComponent<TextProperties>(phaseTimerText);
                    text.opacity = isPlay ? 1.0f : 0.0f;

                    if (isPlay && ecsInterface.HasComponent<TimerComponent>(phaseTimerText))
                    {
                        auto& timer = ecsInterface.GetComponent<TimerComponent>(phaseTimerText);
                        text.text = FormatMMSS(timer.remainingTime);
                    }
                }
            }

            // --- Active Order UI ---
            auto orderUIEntities = ecsInterface.GetEntitiesByTag("OrderUI");
            for (auto e : orderUIEntities)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = isNightPlay;
                }
            }
        }
    }

    int SceneInterface::GetCurrentDay()
    {
        return g_CurrentDay;
    }

    void SceneInterface::SetCurrentDay(int day)
    {
        g_CurrentDay = day;
    }

    void SceneInterface::AdvanceToNextDay()
    {
        ++g_CurrentDay;
    }

    void SceneInterface::PushDayTransitionStateForCurrentDay()
    {
        switch (g_CurrentDay)
        {
        case 2:
            engineState.PushState(EngineMode::Day2);
            break;
        case 3:
            engineState.PushState(EngineMode::Day3);
            break;
        case 4:
            engineState.PushState(EngineMode::Day4);
            break;
        case 5:
            engineState.PushState(EngineMode::Day5);
            break;
        default:
            // If no special transition state exists, just go to play/map directly
            engineState.PushState(EngineMode::Play);
            break;
        }
    }

    void SceneInterface::OnNextDayPressed()
    {
        // Leave upgrade first
        engineState.PopState(); // pop Upgrade if Upgrade is the current state

        // Advance day
        SceneInterface::AdvanceToNextDay();

        // Push matching transition
        SceneInterface::PushDayTransitionStateForCurrentDay();
    }

    // Checks if player has >= 5000.0f gold to show player Win or Lose game
    bool SceneInterface::WinOrLose()
    {
        if (GameSystem::GetCurrency() >= 5000.0f)
        {
            return true; //win game so push EngineMode::Win
        }
        else
        {
            return false;
        }
    }
}