
#include "pch.h"
#include "SceneInterface.h"
#include "ButtonComponent.h"
#include "TextureProperties.h"
#include "Coordinator.h"
#include "TextProperties.h"
#include "TransformProperties.h"
#include <PhaseManager.h>
#include <TimerComponent.h>

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

    // ===== Intro tuning (3 slides) =====
    static const float introSceneDuration = 3.0f;
    static const float introCrossfadeDuration = 1.0f; // quick, smooth
    static const float introStartDelay = 3.0f; // wait 3 seconds before logo 1 fades in
    static float IntrocutsceneStartTime = -2.0f;
    static float IntrofadeOutStartTime = -1.0f;


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
                for (Entity e : upgradeEntities)
                {
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;
                    if (!ecsInterface.HasComponent<TransformComponent>(e)) continue;

                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    auto& transform = ecsInterface.GetComponent<TransformComponent>(e);

                    texture.isUI = true; // always UI in upgrade screen
                    bool shouldShow = false;

                    // -------------------------
                    // COMPLETE STAMPS (highest priority - always show if complete)
                    // -------------------------
                    if (ecsInterface.EntityHasTag(e, "quarrycomplete"))
                    {
                        shouldShow = quarryComplete;
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldcomplete"))
                    {
                        shouldShow = goldComplete;
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventorycomplete"))
                    {
                        shouldShow = backpackComplete;
                    }

                    // -------------------------
                    // QUARRY STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "quarrypass"))
                    {
                        shouldShow = (!quarryComplete && quarryHovered && canAffordQuarry);
                    }
                    else if (ecsInterface.EntityHasTag(e, "quarryfail"))
                    {
                        shouldShow = (!quarryComplete && quarryHovered && !canAffordQuarry);
                    }

                    // -------------------------
                    // GOLD STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "goldpass"))
                    {
                        shouldShow = (!goldComplete && goldHovered && canAffordGold);
                    }
                    else if (ecsInterface.EntityHasTag(e, "goldfail"))
                    {
                        shouldShow = (!goldComplete && goldHovered && !canAffordGold);
                    }

                    // -------------------------
                    // INVENTORY STAMPS (only show if NOT complete)
                    // -------------------------
                    else if (ecsInterface.EntityHasTag(e, "inventorypass"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && canAffordInventory);
                    }
                    else if (ecsInterface.EntityHasTag(e, "inventoryfail"))
                    {
                        shouldShow = (!backpackComplete && inventoryHovered && !canAffordInventory);
                    }

                    // ------------------------------------------------------------
                    // MAIN BUTTON BACKGROUNDS (base/unhovered versions)
                    // ------------------------------------------------------------
                    else
                    {
                        bool isQuarryBase =    ecsInterface.EntityHasTag(e, "quarry");
                        bool isGoldBase =      ecsInterface.EntityHasTag(e, "gold");
                        bool isInventoryBase = ecsInterface.EntityHasTag(e, "inventory");

                        // Hide base card if hovered OR if completed
                        bool hideBase =
                            (isQuarryBase && (quarryHovered || quarryComplete)) ||
                            (isGoldBase && (goldHovered || goldComplete)) ||
                            (isInventoryBase && (inventoryHovered || backpackComplete));

                        shouldShow = !hideBase;
                    }

                    // Set Z position based on visibility
                    transform.position.z = shouldShow ? 1.0f : -100.0f;
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

                if (ecsInterface.HasComponent<ButtonComponent>(e) &&
                    ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    // Only enforce UI flag if NOT in editor mode
                    if (!engineState.Is(EngineMode::Editor)) {
                        if (!engineState.Is(EngineMode::Editor)) { texture.isUI = engineState.Is(EngineMode::MapSelection) ? 1.f : 0.f; }
                    }
                }
            }


            // 7. Cutscene (Open after click Play)
            if (engineState.Is(EngineMode::Cutscene))
            {
                if (cutsceneStartTime < 0.0f)
                {
                    cutsceneStartTime = (float)glfwGetTime();
                    cutsceneFinished = false;

                    auto slides = ecsInterface.GetEntitiesByTag("cutscene");

                    // Init: make them UI fullscreen images, all hidden
                    for (auto e : slides)
                    {
                        if (!ecsInterface.IsEntityValid(e)) continue;
                        if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                        auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                        tex.isUI = true;
                        tex.uiAlpha = 0.0f;

                        // OPTIONAL: if you have UI sizing/anchoring fields, set them here
                        // tex.uiAnchor = UIAnchor::Center;
                        // tex.uiSize   = { viewportW, viewportH };
                        // tex.uiPos    = { viewportW * 0.5f, viewportH * 0.5f };
                    }

                    // Scene 0 (black screen) should be instant on
                    if (!slides.empty() &&
                        ecsInterface.IsEntityValid(slides[0]) &&
                        ecsInterface.HasComponent<TextureProperties>(slides[0]))
                    {
                        ecsInterface.GetComponent<TextureProperties>(slides[0]).uiAlpha = 1.0f;
                    }
                }

                float now = (float)glfwGetTime();
                float t = now - cutsceneStartTime;

                auto slides = ecsInterface.GetEntitiesByTag("cutscene");
                int count = (int)slides.size();

                // Expecting 9 scenes (but clamp safely)
                int sceneCount = glm::min(count, kExpectedScenes);
                if (sceneCount <= 0) return;

                // Which scene are we in?
                int i = (int)glm::floor(t / sceneDuration);

                // Finished all scenes?
                if (i >= sceneCount)
                {
                    if (!cutsceneFinished)
                    {
                        cutsceneFinished = true;

                        // Hide all cutscene slides
                        for (int s = 0; s < sceneCount; ++s)
                        {
                            Entity e = slides[s];
                            if (!ecsInterface.IsEntityValid(e)) continue;
                            if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                            auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                            tex.uiAlpha = 0.0f;
                            tex.isUI = false; // optional
                        }

                        // Move to next state
                        engineState.PushState(EngineMode::MapSelection);
                    }
                    return;
                }

                float sceneT = t - (float)i * sceneDuration;             // 0..sceneDuration
                float fadeStart = sceneDuration - crossfadeDuration;

                // Compute crossfade blend (0..1)
                float blend = 0.0f;

                // Scene 0: instant black screen (no fade-in). Only fade OUT into scene 1 near the end.
                if (i == 0)
                {
                    if (sceneT >= fadeStart)
                        blend = Saturate((sceneT - fadeStart) / crossfadeDuration);
                    else
                        blend = 0.0f;
                }
                else
                {
                    // Normal scenes: crossfade at the end
                    if (sceneT >= fadeStart)
                        blend = Saturate((sceneT - fadeStart) / crossfadeDuration);
                    else
                        blend = 0.0f;
                }

                // Set all alphas to 0 first (so only current + next are visible)
                for (int s = 0; s < sceneCount; ++s)
                {
                    Entity e = slides[s];
                    if (!ecsInterface.IsEntityValid(e)) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(e)) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(e);
                    tex.isUI = true;
                    tex.uiAlpha = 0.0f;
                }

                // Current slide alpha goes 1 -> 0 during crossfade
                {
                    Entity cur = slides[i];
                    if (ecsInterface.IsEntityValid(cur) && ecsInterface.HasComponent<TextureProperties>(cur))
                    {
                        auto& texCur = ecsInterface.GetComponent<TextureProperties>(cur);
                        texCur.isUI = true;
                        texCur.uiAlpha = 1.0f - blend;
                    }
                }

                // Next slide alpha goes 0 -> 1 during crossfade (if exists)
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
            }

            // 8. DIGIPEN LOGO INTRO + GROUP LOGO INTRO
            if (engineState.Is(EngineMode::Intro))
            {
                auto slides = ecsInterface.GetEntitiesByTag("intro");
                int sceneCount = (int)slides.size();
                if (sceneCount <= 0) return;

                const int bgIndex = 0;
                const int logoCount = sceneCount - 1; // slides[1], slides[2], ...

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

                    // Background always on
                    if (ecsInterface.IsEntityValid(slides[bgIndex]) && ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                    {
                        auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                        bg.isUI = true;
                        bg.uiAlpha = 1.0f;
                    }
                }

                float totalT = (float)glfwGetTime() - IntrocutsceneStartTime;

                // Keep first scene only for 3 seconds before logos start
                if (totalT < introStartDelay)
                {
                    // Background visible, all logos hidden
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

                // Start logo timeline only AFTER delay
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

                        engineState.PushState(EngineMode::Menu);
                    }
                    return;
                }

                float sceneT = t - (float)logoIndex * introSceneDuration;
                float fadeIn = Saturate(sceneT / introCrossfadeDuration);

                // Clear logo slides only
                for (int s = 1; s < sceneCount; ++s)
                {
                    if (!ecsInterface.IsEntityValid(slides[s])) continue;
                    if (!ecsInterface.HasComponent<TextureProperties>(slides[s])) continue;

                    auto& tex = ecsInterface.GetComponent<TextureProperties>(slides[s]);
                    tex.isUI = true;
                    tex.uiAlpha = 0.0f;
                }

                // Keep background always visible
                if (ecsInterface.IsEntityValid(slides[bgIndex]) && ecsInterface.HasComponent<TextureProperties>(slides[bgIndex]))
                {
                    auto& bg = ecsInterface.GetComponent<TextureProperties>(slides[bgIndex]);
                    bg.isUI = true;
                    bg.uiAlpha = 1.0f;
                }

                // Current logo slide
                int currentSlide = logoIndex + 1; // maps 0->1, 1->2
                if (currentSlide < sceneCount)
                {
                    if (ecsInterface.IsEntityValid(slides[currentSlide]) && ecsInterface.HasComponent<TextureProperties>(slides[currentSlide]))
                    {
                        auto& texCur = ecsInterface.GetComponent<TextureProperties>(slides[currentSlide]);

                        // First logo fades in from 0 to 1
                        // Later logos can also fade in the same way
                        texCur.uiAlpha = fadeIn;
                    }
                }

                // Optional: fade previous logo out during transition to next
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

            auto coinMoonEntity = ecsInterface.GetEntitiesByTag("coinmoonicon");
            for (Entity e : coinMoonEntity)
            {
                if (!ecsInterface.IsEntityValid(e)) continue;
                if (ecsInterface.HasComponent<TextureProperties>(e))
                {
                    auto& texture = ecsInterface.GetComponent<TextureProperties>(e);
                    texture.isUI = isNightPlay;
                }
            }

            Entity nightTextEntity = ecsInterface.GetEntityByName("nightText");
            if (ecsInterface.IsEntityValid(nightTextEntity) &&
                ecsInterface.HasComponent<TextProperties>(nightTextEntity))
            {
                auto& text = ecsInterface.GetComponent<TextProperties>(nightTextEntity);
                text.opacity = isNightPlay ? 1.0f : 0.0f;
                if (isNightPlay)
                    text.text = ": Night 1|7"; // wire up dynamic values later
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