#include "pch.h"
#include "PlayerInteractSystem.h"
#include "Camera.h"
#include "Coordinator.h"
#include "ScriptingSystem.h"
#include "EventManager.h"
#include "Inventory.h"
#include "BoxColliderComponent.h"
#include "ItemComponent.h"
#include "MeshLoader.h" // [IMPORTANT] Needed to load the Quad for the icon
#include <iostream>
#include "AudioSystem.h"
#include "EngineState.h"

extern Framework::Coordinator ecsInterface;
namespace Framework
{
    constexpr float ZIP_TARGET_Y_OFFSET = -3.0f; // tweak this
    void PlayerInteractSystem::Initialize()
    {
        mPhysicsSystem = ecsInterface.GetSystem<PhysicsSystem>();
        mInputHandler = InputHandler::GetInstance();
        mGraphicsSystem = ecsInterface.GetSystem<Graphics>();
    }

    void PlayerInteractSystem::Update(float dt)
    {
        // 1. LAZY INIT: Ensure UI entities exist
        // We check for the Text entity. If missing, we assume we need to respawn everything.
        static bool prevL = false;
        bool lDown = mInputHandler->IsKeyPressed(GLFW_KEY_J);
        if (lDown && !prevL)
        {
            auto& inv = Framework::Inventory::Instance();
            inv.SetHotbarLevel(inv.GetHotbarLevel() + 1);
        }
        prevL = lDown;

        static bool prevP = false;
        bool PDown = mInputHandler->IsKeyPressed(GLFW_KEY_P);
        if (PDown && !prevP)
        {
            auto& inv = Framework::Inventory::Instance();
            inv.SetBackpackLevel(inv.GetBackpackLevel() + 1);
        }
        prevP = PDown;

        if (!ecsInterface.IsEntityValid(mPromptTextEntity) && !ecsInterface.IsEntityValid(mPromptIconEntity))
        {
            SpawnInteractText();
        }

        // =========================================================
        // PICKUP NOTIFICATION UPDATE (Simple, RecipeBook-style)
        // =========================================================

        // Clear immediately if in Dialogue, Upgrade, or Menu state
        if (engineState.HasState(EngineMode::Dialogue) ||
            engineState.HasState(EngineMode::Upgrade) ||
            engineState.HasState(EngineMode::Menu))
        {
            ClearAllPickupNotifications();
        }
        else
        {
            // Hide + pause if in Settings or HowToPlay — frozen and invisible, resumes when back
            const bool hiddenAndFrozen = engineState.HasState(EngineMode::Settings) ||
                engineState.HasState(EngineMode::HowToPlay);

            for (auto it = mActiveNotifications.begin(); it != mActiveNotifications.end(); )
            {
                if (hiddenAndFrozen)
                {
                    // Hide all notification entities while frozen
                    if (ecsInterface.HasComponent<TextureProperties>(it->bg))
                        ecsInterface.GetComponent<TextureProperties>(it->bg).isUI = false;
                    if (ecsInterface.HasComponent<TextureProperties>(it->icon))
                        ecsInterface.GetComponent<TextureProperties>(it->icon).isUI = false;
                    if (ecsInterface.HasComponent<TextProperties>(it->text))
                        ecsInterface.GetComponent<TextProperties>(it->text).opacity = 0.0f;
                    ++it;
                    continue;
                }

                // Paused: freeze timer, float, and fade — skip all updates this frame
                // Opacity is left as-is (notification stays visible while paused)
                if (engineState.HasState(EngineMode::Paused))
                {
                    ++it;
                    continue;
                }

                // Restore visibility when returning from a hidden+frozen state (Settings/HowToPlay)
                // Only needed here since Paused leaves opacity untouched
                if (ecsInterface.HasComponent<TextureProperties>(it->bg))
                    ecsInterface.GetComponent<TextureProperties>(it->bg).isUI = true;
                if (ecsInterface.HasComponent<TextureProperties>(it->icon))
                    ecsInterface.GetComponent<TextureProperties>(it->icon).isUI = true;
                // (opacity is restored naturally by the fade logic below)

                it->timer -= dt;

                // --- Lifetime end ---
                if (it->timer <= 0.0f)
                {
                    ecsInterface.DestroyEntity(it->bg);
                    ecsInterface.DestroyEntity(it->icon);
                    ecsInterface.DestroyEntity(it->text);
                    it = mActiveNotifications.erase(it);
                    continue;
                }

                // --- Float upward (pixel space) ---
                const float moveUp = 25.0f * dt;

                auto& bgT = ecsInterface.GetComponent<TransformComponent>(it->bg);
                auto& iconT = ecsInterface.GetComponent<TransformComponent>(it->icon);
                auto& txtT = ecsInterface.GetComponent<TransformComponent>(it->text);

                bgT.position.y += moveUp;
                iconT.position.y += moveUp;

                // Text is screen-space → convert Y only
                txtT.position.y = (bgT.position.y - 10.0f) / 1080.0f;

                // --- Fade using opacity ---
                float alpha = 1.0f;

                // Fade out last 0.5s
                if (it->timer < 0.5f)
                {
                    alpha = it->timer / 0.5f;
                }
                // Fade in first 0.3s
                else if (it->timer > 2.2f)
                {
                    alpha = (2.5f - it->timer) / 0.3f;
                }

                alpha = glm::clamp(alpha, 0.0f, 1.0f);

                if (ecsInterface.HasComponent<TextProperties>(it->text))
                {
                    auto& tp = ecsInterface.GetComponent<TextProperties>(it->text);
                    tp.opacity = alpha;
                }


                ++it;
            }
        }

        // --- 2. RAYCAST LOGIC ---
        Camera* gameCamera = mGraphicsSystem ? mGraphicsSystem->getCamera() : nullptr;
        if (!gameCamera) return;

        glm::vec3 origin = gameCamera->position;
        glm::vec3 direction = glm::normalize(gameCamera->front);
        Framework::RaycastHit hit;

        bool didHit = mPhysicsSystem->Raycast(origin, direction, mInteractRange, hit, 1);

        bool foundInteractable = false;
        std::string textToShow = "";

        if (didHit) {
            Entity targetEntity = hit.entity;

            // 1. Check for Item FIRST (Specific)
            if (ecsInterface.HasComponent<ItemComponent>(targetEntity)) {
                foundInteractable = true;
                auto& item = ecsInterface.GetComponent<ItemComponent>(targetEntity);
                textToShow = "Pick up " + Framework::GetItemDisplayName(Framework::ItemNameToId(item.name));
            }
            // 2. Check for Generic Collider SECOND (Fallback)
            else if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity)) {
                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(targetEntity);
                for (const auto& entry : colliderComp.colliders) {
                    if (entry.isInteractable) {
                        foundInteractable = true;
                        textToShow = entry.promptText.empty() ? "Interact" : entry.promptText;
                        break;
                    }
                }
            }
        }

        // --- 2.5 LOOK-AT HIGHLIGHT ---
        if (mGraphicsSystem)
        {
            int lookAtId = (didHit && foundInteractable) ? (int)hit.entity : -1;
            mGraphicsSystem->SetLookAtHighlightedId(lookAtId);
        }

        // --- 3. UPDATE UI VISUALS ---

        // A. Handle ICON Visibility (Scale Toggle)
        if (mPromptIconEntity != -1 && ecsInterface.HasComponent<TransformComponent>(mPromptIconEntity))
        {
            auto& iconTrans = ecsInterface.GetComponent<TransformComponent>(mPromptIconEntity);

            if (foundInteractable) {
                iconTrans.scale = glm::vec3(50.0f); // SHOW (Restore size)
            }
            else {
                iconTrans.scale = glm::vec3(0.0f);  // HIDE (Scale to zero)
            }
        }

        // B. Handle TEXT Content
        if (mPromptTextEntity != -1 && ecsInterface.HasComponent<TextProperties>(mPromptTextEntity))
        {
            auto& textComp = ecsInterface.GetComponent<TextProperties>(mPromptTextEntity);

            if (foundInteractable) {
                textComp.text = textToShow;
            }
            else {
                textComp.text = "";
            }
        }

        // --- 4. HANDLE INPUT ---
        bool isEPressed = mInputHandler->IsKeyPressed(GLFW_KEY_E);
        if (isEPressed && !mWasEPressed) {
            if (foundInteractable && didHit) {
                PerformInteract(hit.entity);
            }
        }
        mWasEPressed = isEPressed;
        Entity player = ecsInterface.GetEntityByName("Player");
        if (ecsInterface.IsEntityValid(player))
        {
            glm::vec3 playerPos = ecsInterface.GetComponent<TransformComponent>(player).position;
            playerPos.y += 1.0f; // Target chest height

            for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
            {
                if (!ecsInterface.IsEntityValid(entity)) continue;
                if (!ecsInterface.HasComponent<ItemComponent>(entity)) continue;

                auto& item = ecsInterface.GetComponent<ItemComponent>(entity);

                // Only move items that have been flagged by PerformInteract
                if (item.isZipping)
                {
                    auto& trans = ecsInterface.GetComponent<TransformComponent>(entity);
                    glm::vec3 targetPos = playerPos;
                    targetPos.y += ZIP_TARGET_Y_OFFSET;

                    glm::vec3 toPlayer = targetPos - trans.position;
                    float distance = glm::length(toPlayer);

                    // Acceleration math: Faster as it gets closer
                    float speed = 12.0f + (1.0f / (distance + 0.1f)) * 5.0f;

                    if (distance > 0.3f)
                    {
                        trans.position += glm::normalize(toPlayer) * speed * dt;
                    }
                    else
                    {
                        // REACHED PLAYER: Finalize
                        int itemId = Framework::ItemNameToId(item.name);
                        if (itemId >= 0) {
                            int slot = Framework::Inventory::Instance().TryAddItem_BackpackFirst(itemId, 0, 1);
                            if (slot >= 0) {
                                SpawnPickupNotification(itemId, Framework::GetItemDisplayName(itemId));

                                // Audio Logic
                                std::string lowerName = item.name;
                                for (auto& c : lowerName) c = (char)tolower(c);
                                if (lowerName.find("potion") != std::string::npos) {
                                    g_AudioSystem->PlayAudio("event:/PickUpPotion");
                                }
                                else if (lowerName.find("wool") != std::string::npos) {
                                    glm::vec3 pos = ecsInterface.GetComponent<TransformComponent>(entity).position;
                                    g_AudioSystem->PlaySound3D("event:/Sheep7",
                                        Framework::Vector3(pos.x, pos.y, pos.z));
                                }
                                else {
                                    g_AudioSystem->PlayAudio("event:/PickingUpHerb");
                                }

                                ecsInterface.DestroyEntity(entity);
                            }
                            else {
                                // Inventory full: Drop it
                                item.isZipping = false;
                                // Re-add physics if you want it to fall back to the ground
                            }
                        }

                        // Unlock it in the global database
                        if (itemId != -1) {
                            Framework::UnlockItem(itemId);
                        }

                    }
                }
            }
        }
        HandlePlayerMovementandJump(dt);
    }

    void PlayerInteractSystem::PerformInteract(Entity targetEntity)
    {
        // ---------------------------------------------------------
        // 1. SCRIPT / EVENT INTERACTIONS (doors, levers, etc.)
        // ---------------------------------------------------------
        if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity))
        {
            auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(targetEntity);
            for (const auto& entry : colliderComp.colliders)
            {
                if (entry.isInteractable && !entry.interactMessage.empty())
                {
                    CollisionEventData payload;
                    payload.senderID = ecsInterface.GetEntityByName("Player");
                    payload.targetID = targetEntity;
                    payload.actionType = 1;

                    EventManager::Instance().Fire(entry.interactMessage, &payload);
                    return;
                }
            }
        }

        // ---------------------------------------------------------
        // 2. ITEM INTERACTION
        // ---------------------------------------------------------
        if (!ecsInterface.HasComponent<ItemComponent>(targetEntity))
            return;

        auto& item = ecsInterface.GetComponent<ItemComponent>(targetEntity);
        int itemId = Framework::ItemNameToId(item.name);
        if (itemId < 0) return;

        // --- CHECK IF FULL ---
        if (Framework::Inventory::Instance().IsInventoryFull())
        {
            auto logEntity = Framework::InstantiatePrefabByName("CauldronLog_Prefab");
            ecsInterface.GetComponent<TextProperties>(logEntity).text = "Inventory Full! Could not pick up " + Framework::GetItemDisplayName(itemId);

            std::cout << "[PlayerInteractSystem] Interaction blocked: Inventory Full." << std::endl;
            return;
        }

        // --- PROCEED WITH ZIP ---
        glm::vec3 sourcePos = ecsInterface.GetComponent<TransformComponent>(targetEntity).position;

        // BEFORE setting isZipping
        if (Framework::Inventory::Instance().IsInventoryFull())
        {
            auto logEntity = Framework::InstantiatePrefabByName("CauldronLog_Prefab");
            ecsInterface.GetComponent<TextProperties>(logEntity).text =
                "Inventory Full! Could not pick up " +
                Framework::GetItemDisplayName(itemId);

            return;
        }


        if (item.toDestroyWhenPicked)
        {
            item.isZipping = true;

            if (mPhysicsSystem)
                mPhysicsSystem->RemoveBodyForEntity(targetEntity);

            if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity))
                ecsInterface.RemoveComponent<BoxColliderComponent>(targetEntity);

            // Visual "pop"
            if (ecsInterface.HasComponent<PhysicsProperties>(targetEntity))
            {
                auto& phys = ecsInterface.GetComponent<PhysicsProperties>(targetEntity);
                phys.gravityFactor = 0.0f;
                phys.linearVelocity = glm::vec3(0.0f, 3.0f, 0.0f);
                phys.startAwake = true;
            }
            return;
        }

        SpawnZipPickup(itemId, sourcePos);
    }


    void PlayerInteractSystem::SpawnInteractText() {

        Entity existingIcon = ecsInterface.GetEntityByName("InteractionIcon");
        if (ecsInterface.IsEntityValid(existingIcon)) {
            ecsInterface.DestroyEntity(existingIcon);
        }

        Entity existingText = ecsInterface.GetEntityByName("InteractionPrompt");
        if (ecsInterface.IsEntityValid(existingText)) {
            ecsInterface.DestroyEntity(existingText);
        }
        // [Setup: 1920x1080 Assumption]
        float centerX = 1920.0f * 0.5f; // 960.0f
        float centerY = 1080.0f * 0.5f; // 540.0f


        // =========================================================
    // 1. CREATE ICON
    // =========================================================
        mPromptIconEntity = ecsInterface.GetEntityByName("InteractionIcon");
        if (!ecsInterface.IsEntityValid(mPromptIconEntity))
        {
            mPromptIconEntity = ecsInterface.CreateEntity();
            ecsInterface.SetEntityName(mPromptIconEntity, "InteractionIcon");

            TransformComponent transform;
            transform.position = glm::vec3(centerX, centerY - 40.0f, 0.0f);
            transform.scale = glm::vec3(50.0f, 50.0f, 1.0f);

            ecsInterface.AddComponent(mPromptIconEntity, transform);

            // TEXTURE
            TextureProperties texComp;
            texComp.path = "Assets/Textures/Key_E.png";
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);

            ecsInterface.AddComponent(mPromptIconEntity, texComp);

            HierarchyComponent hc;
            ecsInterface.AddComponent(mPromptIconEntity, hc);

            MeshProperties mp;
            ecsInterface.AddComponent(mPromptIconEntity, mp);
        }

        // =========================================================
        // 2. CREATE TEXT
        // =========================================================
        mPromptTextEntity = ecsInterface.GetEntityByName("InteractionPrompt");
        if (!ecsInterface.IsEntityValid(mPromptTextEntity))
        {
            mPromptTextEntity = ecsInterface.CreateEntity();
            ecsInterface.SetEntityName(mPromptTextEntity, "InteractionPrompt");

            TransformComponent transform;
            transform.position = glm::vec3(0.5f, (centerY - 100.0f) / 1080, 0.0f);
            transform.scale = glm::vec3(1.0f);

            HierarchyComponent hc;
            MeshProperties mp;

            TextProperties textComp;
            textComp.text = "";
            textComp.color = glm::vec3(1.0f, 1.0f, 0.0f);
            textComp.scale = 1.0f;
            textComp.screenSpace = true;
            textComp.alignment = 1;


            ecsInterface.AddComponent(mPromptTextEntity, transform);
            ecsInterface.AddComponent(mPromptTextEntity, textComp);
            ecsInterface.AddComponent(mPromptTextEntity, mp);
            ecsInterface.AddComponent(mPromptTextEntity, hc);


            std::cout << "[PlayerInteractSystem] UI Spawned." << std::endl;
        }
    }

    void PlayerInteractSystem::HandlePlayerMovementandJump(float dt)
    {
        if (!mGraphicsSystem) return;
        if (mGraphicsSystem->GetMouseTargetView() != 0) return; // only in game view

        if (engineState.IsAnyUIOpen() && !Framework::Inventory::Instance().IsBackpackOpen()) return; // skip movement/jump when not in active gameplay

        GLFWwindow* window = mGraphicsSystem->GetWindow();
        if (!window) return;

        //Link key 0-9 to hotbar selected box
        auto& inv = Framework::Inventory::Instance();
        static bool prevNum[10] = {};

        int keys[10] = {
            GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
            GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6,
            GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,  GLFW_KEY_0
        };

        for (int i = 0; i < 10; ++i)
        {
            bool down = glfwGetKey(window, keys[i]) == GLFW_PRESS;
            if (down && !prevNum[i])
                inv.SetSelectedIndex(i);
            prevNum[i] = down;
        }

        // Find player
        Framework::Entity player = ecsInterface.GetEntityByName("Player");
        if (!ecsInterface.IsEntityValid(player) ||
            !ecsInterface.HasComponent<Framework::PhysicsProperties>(player) ||
            !ecsInterface.HasComponent<Framework::TransformComponent>(player))
            return;

        auto& phys = ecsInterface.GetComponent<Framework::PhysicsProperties>(player);
        auto& trans = ecsInterface.GetComponent<Framework::TransformComponent>(player);

        // Camera basis (Y-up world)
        Camera* cam = mGraphicsSystem->GetCameraByIndex(0); // 0 = game view
        if (!cam) return;

        glm::vec3 fwd = cam->front;  fwd.y = 0.0f; fwd = glm::normalize(fwd);
        glm::vec3 rgt = cam->right;  rgt.y = 0.0f; rgt = glm::normalize(rgt);

        // --- Movement ---
        glm::vec3 targetVel(0.0f);
        const float moveSpeed = 10.0f;

        const bool camRotationEnabled = mGraphicsSystem->IsCameraRotationEnabled();
        const bool inventoryOpen = Framework::Inventory::Instance().IsBackpackOpen();
        const bool allowMovementInput = camRotationEnabled || inventoryOpen;

        // Only accumulate input if camera rotation enabled (same as old code)
        if (allowMovementInput)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) targetVel += fwd;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) targetVel -= fwd;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) targetVel -= rgt;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) targetVel += rgt;
        }

        if (glm::length(targetVel) > 0.1f)
            targetVel = glm::normalize(targetVel) * moveSpeed;

        //if (camRotationEnabled)
        //{
        //    // NORMAL: apply X/Z and let gravity act on Y
        //    auto moveToward = [](float cur, float tgt, float maxDelta)
        //        {
        //            float diff = tgt - cur;
        //            if (diff > maxDelta) diff = maxDelta;
        //            if (diff < -maxDelta) diff = -maxDelta;
        //            return cur + diff;
        //        };

        //    const float accel = 30.0f;          // tune: higher = snappier
        //    const float brake = 120.0f;     // extra strong decel when stopping
        //    const float maxAccelDelta = accel * dt;
        //    const float maxBrakeDelta = brake * dt;

        //    phys.linearVelocity.x = moveToward(phys.linearVelocity.x, targetVel.x,
        //        (glm::abs(targetVel.x) < 0.01f) ? maxBrakeDelta : maxAccelDelta);

        //    phys.linearVelocity.z = moveToward(phys.linearVelocity.z, targetVel.z,
        //        (glm::abs(targetVel.z) < 0.01f) ? maxBrakeDelta : maxAccelDelta);

        //    phys.gravityFactor = 10.0f;// edit for faster dropdown

        //    const bool noInput = glm::length(targetVel) < 0.01f;
        //    if (noInput)
        //    {
        //        // stop horizontal drift so gravity doesn't pull you down the slope
        //        phys.linearVelocity.x = 0.0f;
        //        phys.linearVelocity.z = 0.0f;
        //    }
        //}
        //else
        //{
        //    // STIRRING / LOCKED: freeze all motion, turn off gravity
        //    phys.linearVelocity = glm::vec3(0.0f);
        //    phys.gravityFactor = 0.0f;
        //}

        if (allowMovementInput)
        {
            // NORMAL: apply X/Z and let gravity act on Y
            auto moveToward = [](float cur, float tgt, float maxDelta)
                {
                    float diff = tgt - cur;
                    if (diff > maxDelta) diff = maxDelta;
                    if (diff < -maxDelta) diff = -maxDelta;
                    return cur + diff;
                };

            const float accel = 30.0f;
            const float brake = 120.0f;
            const float maxAccelDelta = accel * dt;
            const float maxBrakeDelta = brake * dt;

            phys.linearVelocity.x = moveToward(phys.linearVelocity.x, targetVel.x,
                (glm::abs(targetVel.x) < 0.01f) ? maxBrakeDelta : maxAccelDelta);

            phys.linearVelocity.z = moveToward(phys.linearVelocity.z, targetVel.z,
                (glm::abs(targetVel.z) < 0.01f) ? maxBrakeDelta : maxAccelDelta);

            phys.gravityFactor = 10.0f;

            const bool noInput = glm::length(targetVel) < 0.01f;
            if (noInput)
            {
                phys.linearVelocity.x = 0.0f;
                phys.linearVelocity.z = 0.0f;
            }
        }
        else
        {
            // only freeze for *other menus*, not inventory
            phys.linearVelocity = glm::vec3(0.0f);
            phys.gravityFactor = 0.0f;
        }


        // Always ensure the body wakes up when we drive it
        phys.startAwake = true;

        // --- Jump (no startAwake, same style as movement) ---
        static bool  prevSpace = false;  // edge trigger
        static float jumpLock = 0.0f;   // anti-spam
        if (jumpLock > 0.0f) jumpLock -= dt;

        const bool spaceDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

        // TODO: later add && mGrounded here once your collision-based grounded is wired
        if (spaceDown && !prevSpace && jumpLock <= 0.0f && allowMovementInput)
        {
            const float jumpSpeed = 25.0f;
            phys.linearVelocity.y = jumpSpeed;
            jumpLock = 1.25f;
            phys.startAwake = true;
        }

        prevSpace = spaceDown;

        // (Camera-follow is still in GraphicsSystem; if you want to move it here,
        //  also set cam->position = trans.position + offset, like the old code.)
    }

    void PlayerInteractSystem::SpawnPickupNotification(int itemId, const std::string& itemName)
    {
        // Don't spawn notifications in Dialogue, Upgrade, or Menu states
        if (engineState.HasState(EngineMode::Dialogue) ||
            engineState.HasState(EngineMode::Upgrade) ||
            engineState.HasState(EngineMode::Menu))
            return;

        ClearAllPickupNotifications();
        // --- Layout (pixel space) ---
        const float startX = 180.0f;
        const float startY = 300.0f;

        const float bgWidth = 360.0f;
        const float bgHeight = 90.0f;

        // =========================================================
        // 1. Background
        // =========================================================
        Entity bg = ecsInterface.CreateEntity();
        {
            TransformComponent trans;
            trans.position = glm::vec3(startX, startY, 0.1f);
            trans.scale = glm::vec3(bgWidth, bgHeight, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextureProperties tex;

            tex.path = "Assets/Textures/UI_Notification_BG.png";
            tex.isUI = true;
            TextureLoader::loadTexture(tex, true);

            ecsInterface.AddComponent(bg, hc);
            ecsInterface.AddComponent(bg, trans);
            ecsInterface.AddComponent(bg, mesh);
            ecsInterface.AddComponent(bg, tex);
        }

        // =========================================================
        // 2. Icon
        // =========================================================
        Entity icon = ecsInterface.CreateEntity();
        {
            TransformComponent trans;
            trans.position = glm::vec3(startX - 135.0f, startY, 0.2f);
            trans.scale = glm::vec3(60.0f, 60.0f, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextureProperties tex;

            tex.path = Framework::MakeItemIconPath(itemId);
            tex.isUI = true;
            TextureLoader::loadTexture(tex, true);

            ecsInterface.AddComponent(icon, hc);
            ecsInterface.AddComponent(icon, trans);
            ecsInterface.AddComponent(icon, mesh);
            ecsInterface.AddComponent(icon, tex);
        }

        // =========================================================
        // 3. Text (same pattern as RecipeBook::SpawnText)
        // =========================================================
        Entity text = ecsInterface.CreateEntity();
        {
            TransformComponent trans;
            trans.position = glm::vec3(
                (startX - 80.0f) / 1920.0f,
                startY - 30.0f,
                0.3f
            );
            trans.scale = glm::vec3(1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextProperties tp;

            tp.screenSpace = true;
            tp.text = itemName + " x1";
            tp.alignment = 0; // Left
            tp.opacity = 0.0f;   // start hiddenf

            tp.scale = 1.0f;
            tp.color = glm::vec3(1.0f);

            ecsInterface.AddComponent(text, hc);
            ecsInterface.AddComponent(text, trans);
            ecsInterface.AddComponent(text, mesh);
            ecsInterface.AddComponent(text, tp);
        }

        // =========================================================
        // 4. Store for lifetime handling
        // =========================================================
        mActiveNotifications.push_back({ bg, icon, text, 2.5f });
    }

    void PlayerInteractSystem::ClearAllPickupNotifications()
    {
        for (auto& n : mActiveNotifications)
        {
            ecsInterface.DestroyEntity(n.bg);
            ecsInterface.DestroyEntity(n.icon);
            ecsInterface.DestroyEntity(n.text);
        }

        mActiveNotifications.clear();
    }

    void PlayerInteractSystem::SpawnZipPickup(int itemId, const glm::vec3& spawnPos)
    {
        Entity e = ecsInterface.CreateEntity();

        TransformComponent trans;
        trans.position = spawnPos;
        trans.scale = glm::vec3(0.5f);

        HierarchyComponent hc;
        TextureProperties tx;
        tx.path = "Assets/Textures/" + std::string(kItemNames[itemId]) + "3D.png";
        TextureLoader::loadTexture(tx, false);

        MeshProperties mp;
        mp.guid = AssetManager::Instance().FindGuid<MeshProperties>(kItemNames[itemId]);
        MeshLoader::loadMesh("Assets/Objects/" + std::string(kItemNames[itemId]) + ".obj", mp);

        ItemComponent itemComp;
        itemComp.isZipping = true;
        itemComp.name = kItemNames[itemId];
        itemComp.toDestroyWhenPicked = true;

        ecsInterface.AddComponent(e, trans);
        ecsInterface.AddComponent(e, hc);
        ecsInterface.AddComponent(e, tx);
        ecsInterface.AddComponent(e, itemComp);
        ecsInterface.AddComponent(e, mp);
    }
}