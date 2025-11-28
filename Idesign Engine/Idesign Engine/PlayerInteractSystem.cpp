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

extern Framework::Coordinator ecsInterface;



namespace Framework
{

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
        

        if (!ecsInterface.IsEntityValid(mPromptTextEntity) && !ecsInterface.IsEntityValid(mPromptIconEntity))
        {
            SpawnInteractText();
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

            // Priority A: BoxCollider with custom text
            if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity)) {
                auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(targetEntity);
                for (const auto& entry : colliderComp.colliders) {
                    if (entry.isInteractable) {
                        foundInteractable = true;
                        textToShow = entry.promptText.empty() ? "Interact" : entry.promptText;
                        break;
                    }
                }
            }

            // Priority B: Generic Item Fallback
            if (!foundInteractable && ecsInterface.HasComponent<ItemComponent>(targetEntity)) {
                foundInteractable = true;
                auto& item = ecsInterface.GetComponent<ItemComponent>(targetEntity);
                textToShow = "Pick up " + item.name;
            }
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
                textComp.text = ""; // HIDE (Empty string)
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
        HandlePlayerMovementandJump(dt);
    }

    void PlayerInteractSystem::PerformInteract(Entity targetEntity)
    {
        bool handledByScript = false;

        if (ecsInterface.HasComponent<BoxColliderComponent>(targetEntity)) {
            auto& colliderComp = ecsInterface.GetComponent<BoxColliderComponent>(targetEntity);
            for (const auto& entry : colliderComp.colliders) {
                if (entry.isInteractable && !entry.interactMessage.empty()) {

                    CollisionEventData payload;
                    payload.senderID = ecsInterface.GetEntityByName("Player");
                    payload.targetID = targetEntity;
                    payload.actionType = 1;

                    EventManager::Instance().Fire(entry.interactMessage, &payload);
                    handledByScript = true;
                }
            }
        }

        if (handledByScript) return;

        if (ecsInterface.HasComponent<ItemComponent>(targetEntity)) {
            auto& item = ecsInterface.GetComponent<ItemComponent>(targetEntity);
            int itemId = Framework::ItemNameToId(item.name);

            if (itemId >= 0) {
                int slot = Framework::Inventory::Instance().AddFirstEmpty(itemId);
                if (slot >= 0) {
                    if (item.toDestroyWhenPicked) {
                        mPhysicsSystem->RemoveBodyForEntity(targetEntity);
                        ecsInterface.DestroyEntity(targetEntity);
                    }
                }
            }
        }
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
            transform.position = glm::vec3(0.5f, (centerY - 100.0f)/1080 , 0.0f);
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

        GLFWwindow* window = mGraphicsSystem->GetWindow();
        if (!window) return;

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
        const float moveSpeed = 5.0f;

        const bool camRotationEnabled = mGraphicsSystem->IsCameraRotationEnabled();

        // Only accumulate input if camera rotation enabled (same as old code)
        if (camRotationEnabled)
        {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) targetVel += fwd;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) targetVel -= fwd;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) targetVel -= rgt;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) targetVel += rgt;
        }

        if (glm::length(targetVel) > 0.1f)
            targetVel = glm::normalize(targetVel) * moveSpeed;

        if (camRotationEnabled)
        {
            // NORMAL: apply X/Z and let gravity act on Y
            phys.linearVelocity.x = targetVel.x;
            phys.linearVelocity.z = targetVel.z;
            phys.gravityFactor = 1.0f;
        }
        else
        {
            // STIRRING / LOCKED: freeze all motion, turn off gravity
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
        if (spaceDown && !prevSpace && jumpLock <= 0.0f && camRotationEnabled)
        {
            const float jumpSpeed = 7.5f;
            phys.linearVelocity.y = jumpSpeed;  // immediate upward impulse
            jumpLock = 1.65f;
            phys.startAwake = true;            // make sure this impulse wakes the body
        }

        prevSpace = spaceDown;

        // (Camera-follow is still in GraphicsSystem; if you want to move it here,
        //  you’d also set cam->position = trans.position + offset, like the old code.)
    }

}