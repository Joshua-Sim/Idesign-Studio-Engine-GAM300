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
    // somewhere persistent (SceneInterface members or static in cpp)
    static bool cutsceneFinished = false;
    static bool IntroFinished = false;

    // Tune these
    static const float sceneDuration = 3.0f;   // each scene lasts x second(s) total
    static const float crossfadeDuration = 0.25f;  // fast seamless fade (tweak 0.12 ~ 0.25)
    static const int   kExpectedScenes = 10;

    static float cutsceneStartTime = -1.0f;
    static float fadeOutStartTime = -1.0f;

    static bool  cutsceneSkipHolding = false;
    static float cutsceneSkipStartTime = 0.0f;
    static const float cutsceneSkipHoldDuration = 2.0f;

    // ===== Intro tuning (3 slides) =====
    static const float introSceneDuration = 3.0f;
    static const float introCrossfadeDuration = 1.0f; // quick, smooth
    static const float introStartDelay = 3.0f; // wait 3 seconds before logo 1 fades in
    static float IntrocutsceneStartTime = -2.0f;
    static float IntrofadeOutStartTime = -1.0f;

    // ===== Day Phases =====
    static float dayPhaseStartTime = -1.0f;
    static bool  dayPhaseFinished = false;

    // Tuning
    static const float dayPhaseFadeInDuration = 0.4f;
    static const float dayPhaseSlideDuration = 1.2f;
    static const float dayPhaseHoldDuration = 0.8f;
    static const float dayPhaseFadeOutDuration = 0.5f;

    // X positions (tune these to your UI layout)
    static const float dayPhaseStartX = -900.0f; // off-screen left
    static const float dayPhaseCenterX = 0.0f;    // center position


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

                auto& upgradeMgr = UpgradeManager::Instance();

                // ------------------------------------------------------------
                // 1. Cache hover states from the MAIN buttons
                // ------------------------------------------------------------
                bool quarryHovered = false;
                bool goldHovered = false;
                bool inventoryHovered = false;

                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<ButtonComponent>(e)) continue;

                    auto& btn = ecsInterface.GetComponent<ButtonComponent>(e);

                    if (ecsInterface.EntityHasTag(e, "quarry"))      quarryHovered = btn.isHovered;
                    if (ecsInterface.EntityHasTag(e, "gold"))        goldHovered = btn.isHovered;
                    if (ecsInterface.EntityHasTag(e, "inventory"))   inventoryHovered = btn.isHovered;
                }

                // ------------------------------------------------------------
                // 2. Affordability and Completion checks
                // ------------------------------------------------------------
                bool canAffordQuarry = GameSystem::GetCurrency() >= upgradeMgr.GetAreaUnlockCost(GameArea::Quarry);
                bool canAffordGold = upgradeMgr.CanAffordCurrencyUpgrade();
                bool canAffordInventory = upgradeMgr.CanAffordInventoryUpgrade();

                // Completion checks
                bool quarryComplete = upgradeMgr.IsAreaUnlocked(GameArea::Quarry);
                bool goldComplete = upgradeMgr.IsCurrencyMaxLevel();
                bool backpackComplete = upgradeMgr.IsBackpackMaxLevel();

                // ------------------------------------------------------------
                // 3. Apply visibility to ALL upgrade UI entities
                // ------------------------------------------------------------
                // Z-layer constants for upgrade UI draw order (higher = in front)
                // Base cards draw behind hover/complete stamps
                // ------------------------------------------------------------
                static constexpr float Z_BASE_CARD = 0.8f;  // idle base card backgrounds
                static constexpr float Z_HOVER_STAMP = 0.9f;  // pass/fail stamps shown on hover
                static constexpr float Z_COMPLETE = 1.0f; // purchased/complete stamps (always on top)

                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                    if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(e);

                    bool shouldShow = false;
                    float zLayer = Z_BASE_CARD;

                    // -------------------------
                    // COMPLETE STAMPS (highest priority - always show if complete)
                    // -------------------------
                    if (ecsInterface.EntityHasTag(e, "quarrycomplete"))
                    {
                        shouldShow = quarryComplete;
                        zLayer = Z_COMPLETE;
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldcomplete"))
                    {
                        shouldShow = goldComplete;
                        zLayer = Z_COMPLETE;
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventorycomplete"))
                    {
                        shouldShow = backpackComplete;
                        zLayer = Z_COMPLETE;
                    }

                    // -------------------------
                    // QUARRY STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "quarrypass"))
                    {
                        shouldShow = (!quarryComplete && quarryHovered && canAffordQuarry);
                        zLayer = Z_HOVER_STAMP;
                    }
                    else if (ecsInterface.EntityHasTag(e, "quarryfail"))
                    {
                        shouldShow = (!quarryComplete && quarryHovered && !canAffordQuarry);
                        zLayer = Z_HOVER_STAMP;
                    }

                    // -------------------------
                    // GOLD STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "goldpass"))
                    {
                        shouldShow = (!goldComplete && goldHovered && canAffordGold);
                        zLayer = Z_HOVER_STAMP;
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldfail"))
                    {
                        shouldShow = (!goldComplete && goldHovered && !canAffordGold);
                        zLayer = Z_HOVER_STAMP;
                    }

                    // -------------------------
                    // INVENTORY STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "inventorypass"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && canAffordInventory);
                        zLayer = Z_HOVER_STAMP;
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventoryfail"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && !canAffordInventory);
                        zLayer = Z_HOVER_STAMP;
                    }

                    // ------------------------------------------------------------
                    // MAIN BUTTON BACKGROUNDS (base/unhovered versions)
                    // ------------------------------------------------------------
                    else
                    {
                        bool isQuarryBase = ecsInterface.EntityHasTag(e, "quarry");
                        bool isGoldBase = ecsInterface.EntityHasTag(e, "gold");
                        bool isInventoryBase = ecsInterface.EntityHasTag(e, "inventory");

                        // Hide base card if hovered OR if completed
                        bool hideBase =
                            (isQuarryBase && (quarryHovered || quarryComplete)) ||
                            (isGoldBase && (goldHovered || goldComplete)) ||
                            (isInventoryBase && (inventoryHovered || backpackComplete));

                        shouldShow = !hideBase;
                        zLayer = Z_BASE_CARD;
                    }

                    // Use isUI for visibility, z for layering — never use z=-100 as a hide hack
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
                        texture.isUI = engineState.Is(EngineMode::MapSelection) ? 1.f : 0.f;
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
                // All day images should share the same tag, e.g. "dayphase"
                auto dayEntities = ecsInterface.GetEntitiesByTag("dayphase");
                int totalEntities = (int)dayEntities.size();
                if (totalEntities <= 0) return;

                // Determine which day we are transitioning TO
                int targetDay = 0;
                if (engineState.Is(EngineMode::Day2)) targetDay = 2;
                else if (engineState.Is(EngineMode::Day3)) targetDay = 3;
                else if (engineState.Is(EngineMode::Day4)) targetDay = 4;
                else if (engineState.Is(EngineMode::Day5)) targetDay = 5;

                if (targetDay <= 0) return;

                // Example:
                // targetDay = 2 -> show 1 image? or 2 images?
                // Based on your description:
                // Day2 shows Day2
                // Day3 shows Day2, Day3
                // Day4 shows Day2, Day3, Day4
                //
                // So index 0 = Day2, index 1 = Day3, index 2 = Day4, ...
                int visibleCount = glm::min(targetDay - 1, totalEntities);

                if (visibleCount <= 0) return;

                // Timeline
                const float fadeInEnd = dayPhaseFadeInDuration;
                const float slideEnd = fadeInEnd + dayPhaseSlideDuration;
                const float holdEnd = slideEnd + dayPhaseHoldDuration;
                const float fadeOutEnd = holdEnd + dayPhaseFadeOutDuration;

                if (dayPhaseStartTime < 0.0f)
                {
                    dayPhaseStartTime = (float)glfwGetTime();
                    dayPhaseFinished = false;

                    // Initialize all day entities hidden/offscreen
                    for (int i = 0; i < totalEntities; ++i)
                    {
                        Entity e = dayEntities[i];
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                        if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;

                        // Start all off-screen left
                        tr.position.x = dayPhaseStartX;
                    }
                }

                float t = (float)glfwGetTime() - dayPhaseStartTime;

                // Done
                if (t >= fadeOutEnd)
                {
                    if (!dayPhaseFinished)
                    {
                        dayPhaseFinished = true;

                        for (int i = 0; i < totalEntities; ++i)
                        {
                            Entity e = dayEntities[i];
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false;
                        }

                        engineState.PopState();
                        engineState.PushState(EngineMode::Play);
                    }
                    return;
                }

                // Alpha control
                float globalAlpha = 1.0f;

                if (t < fadeInEnd)
                {
                    globalAlpha = Saturate(t / dayPhaseFadeInDuration);
                }
                else if (t < holdEnd)
                {
                    globalAlpha = 1.0f;
                }
                else
                {
                    float fadeOutT = (t - holdEnd) / dayPhaseFadeOutDuration;
                    globalAlpha = 1.0f - Saturate(fadeOutT);
                }

                // Slide X control
                float currentX = dayPhaseCenterX;

                if (t < fadeInEnd)
                {
                    currentX = dayPhaseStartX;
                }
                else if (t < slideEnd)
                {
                    float slideT = (t - fadeInEnd) / dayPhaseSlideDuration;
                    slideT = Saturate(slideT);

                    // smoothstep-like easing
                    float eased = slideT * slideT * (3.0f - 2.0f * slideT);
                    currentX = glm::mix(dayPhaseStartX, dayPhaseCenterX, eased);
                }
                else
                {
                    currentX = dayPhaseCenterX;
                }

                // Hide all first
                for (int i = 0; i < totalEntities; ++i)
                {
                    Entity e = dayEntities[i];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = false;
                    tex.uiAlpha = 0.0f;
                }

                // Show only the needed day images
                // Example:
                // targetDay=3 => show indices 0..1  (Day2, Day3)
                // targetDay=4 => show indices 0..2  (Day2, Day3, Day4)
                for (int i = 0; i < visibleCount; ++i)
                {
                    Entity e = dayEntities[i];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                    if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    auto& tr = ecsInterface.GetComponent<TransformComponent>(e);

                    tex.isUI = true;
                    tex.uiAlpha = globalAlpha;

                    // Move the strip horizontally
                    //
                    // Each image should be placed next to the previous one.
                    // Replace 600.0f with your actual image spacing.
                    float imageSpacing = 600.0f;

                    tr.position.x = currentX + i * imageSpacing;
                }
            }
            else
            {
                dayPhaseStartTime = -1.0f;
                dayPhaseFinished = false;
            }

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
                    text.text = ": Day 1|7"; // wire up dynamic values later
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
                    text.text = ": Night 1|7";
            }

            // Mini Controls
            auto miniControlsEntity = ecsInterface.GetEntitiesByTag("minicontrols");
            for (Entity e : miniControlsEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    bool shouldShow = engineState.Is(EngineMode::Play);
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
}