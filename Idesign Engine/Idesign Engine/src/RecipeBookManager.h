#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "Coordinator.h"
#include "MeshProperties.h"
#include "TransformProperties.h"
#include "TextProperties.h"
#include "TextureProperties.h"
#include "ComponentList.h"
#include "TextureLoader.h"
#include "ItemComponent.h" // Must include your updated ItemComponent with GetRecipeForResult & ItemType
#include "AudioSystem.h"
#include "EventManager.h"  // Must include your EventManager

namespace Framework {

    class RecipeBookManager {

    private:
        Coordinator* coordinator;
        std::vector<Entity> uiEntities;

        // --- STATE ---
        int m_selectedItemID = -1;

        // --- LAYOUT CONSTANTS ---
        const float GRID_START_X = 510.0f;  // Top-left of the Left Page
        const float GRID_START_Y = 750.0f;
        const float SLOT_SIZE = 120.0f;
        const float PADDING = 20.0f;
        const int   GRID_COLUMNS = 3;

    public:
        bool bookSpawned = false;

        RecipeBookManager(Coordinator* coord) : coordinator(coord) {
            // Subscribe to ItemClicked event safely
            EventManager::Instance().Subscribe("ItemClicked",
                [this](void* payload) -> bool {
                    if (!payload) return true;

                    // Cast to our specific event struct
                    ItemClickedEvent* ev = static_cast<ItemClickedEvent*>(payload);

                    // Update state and refresh UI
                    this->m_selectedItemID = ev->itemID;
                    this->RefreshBook();

                    return true; // Keep listening
                }
            );
        }

        void Shutdown() {
            ClearBook();
            std::cout << "[RecipeBook] Shutdown complete." << std::endl;
        }

        // --- INPUT HANDLING ---
        void OnMouseClick(float mouseUV_X, float mouseUV_Y) {
            if (!bookSpawned) return;

            // 1. CONVERT UV (0-1) TO PIXELS (1920x1080)
            // We use the same reference resolution you used for placement
            float mouseX = mouseUV_X * 1920.0f;
            float mouseY = mouseUV_Y * 1080.0f;

            float currentX = GRID_START_X;
            float currentY = GRID_START_Y;
            int colCounter = 0;

            for (int i = 0; i < Framework::kItemCount; i++) {
                // AABB Collision Check (Mouse vs Icon Center)
                float halfSize = SLOT_SIZE / 2.0f;

                // Now comparing Pixels vs Pixels
                bool hitX = mouseX >= (currentX - halfSize) && mouseX <= (currentX + halfSize);
                bool hitY = mouseY >= (currentY - halfSize) && mouseY <= (currentY + halfSize);

                if (hitX && hitY) {
                    std::cout << "hi";
                    ItemClickedEvent data;
                    data.itemID = i;
                    EventManager::Instance().Fire("ItemClicked", &data);
                    return;
                }

                // Grid Logic (Advance to next slot)
                colCounter++;
                if (colCounter >= GRID_COLUMNS) {
                    colCounter = 0;
                    currentX = GRID_START_X;
                    currentY -= (SLOT_SIZE + PADDING);
                }
                else {
                    currentX += (SLOT_SIZE + PADDING);
                }
            }
        }

        // --- MAIN RENDER FUNCTION ---
        void BuildBookUI() {
            if (!bookSpawned) g_AudioSystem->PlayAudio("event:/PaperUp");

            float bookCenterX = 960.0f;
            float bookCenterY = 540.0f;

            // 1. Spawn Main Book Background
            SpawnBook(bookCenterX, bookCenterY, 1300.0f, 900.0f);

            // 2. Build Left Page (The Grid)
            BuildLeftPage();

            // 3. Build Right Page (The Context)
            if (m_selectedItemID != -1) {
                BuildRightPage(m_selectedItemID);
            }

            bookSpawned = true;
        }

        void ClearBook() {
            for (Entity e : uiEntities) {
                coordinator->DestroyEntity(e);
            }
            uiEntities.clear();
        }

        // Helper to refresh when state changes (avoids destroying/playing audio if not needed)
        void RefreshBook() {
            ClearBook();
            BuildBookUI();
        }

    private:

        // =========================================================
        //                 LEFT PAGE: ITEM GRID
        // =========================================================
        void BuildLeftPage() {
            float currentX = GRID_START_X;
            float currentY = GRID_START_Y;
            int colCounter = 0;

            SpawnText(currentX + 100.0f, currentY + 130.0f, "All Items", 1.0f);

            for (int i = 0; i < Framework::kItemCount; i++) {

                // 1. Determine Background based on Type
                Framework::ItemType type = Framework::GetItemType(i);
                std::string bgTexture = "Item_DefaultBG";
                if (type == Framework::ItemType::POTION) bgTexture = "Item_PotionBG";
                else if (type == Framework::ItemType::INGREDIENT) bgTexture = "Item_IngredientBG";

                // 2. Spawn Background & Icon
                SpawnGenericSprite(currentX, currentY, bgTexture, SLOT_SIZE, SLOT_SIZE);
                SpawnUISprite(currentX, currentY, Framework::kItemNames[i], 2.0f);

                // 3. Grid Math
                colCounter++;
                if (colCounter >= GRID_COLUMNS) {
                    colCounter = 0;
                    currentX = GRID_START_X;
                    currentY -= (SLOT_SIZE + PADDING);
                }
                else {
                    currentX += (SLOT_SIZE + PADDING);
                }
            }
        }

        // =========================================================
        //                  RIGHT PAGE: INGREDIENT BREAKDOWN
        // =========================================================
// =========================================================
        //                 RIGHT PAGE: INGREDIENT BREAKDOWN
        // =========================================================
        void BuildRightPage(int selectedID) {
            float rightPageX = 960.0f + 250.0f;
            float currentY = GRID_START_Y;
            float rowSpacing = SLOT_SIZE + PADDING;

            SpawnText(rightPageX, currentY + 130.0f, "Ingredients Required", 1.0f);

            // 1. Get the Main Recipe for the selected item
            const PotionRecipe* primaryRecipe = Framework::GetRecipeForResult(selectedID);

            // If it's a raw item (no recipe), just show it and return
            if (!primaryRecipe) {
                SpawnGenericSprite(rightPageX, currentY, "Item_IngredientBG", SLOT_SIZE, SLOT_SIZE);
                SpawnUISprite(rightPageX, currentY, Framework::kItemNames[selectedID], 2.0f);
                return;
            }

            // 2. List Prerequisites (Ingredients / Sub-Recipes)
            for (int ingredientID : primaryRecipe->ingredientIDs) {
                const PotionRecipe* subRecipe = Framework::GetRecipeForResult(ingredientID);

                if (subRecipe) {
                    // Complex Ingredient: Show its recipe (e.g. chopping a herb)
                    RenderRecipeRow(rightPageX, currentY, subRecipe);
                }
                else {
                    // Simple Ingredient: Just show the icon
                    std::string name = Framework::kItemNames[ingredientID];
                    std::string bg = "Item_IngredientBG";
                    if (Framework::GetItemType(ingredientID) == Framework::ItemType::POTION) bg = "Item_PotionBG";

                    // Alignment logic to match RenderRecipeRow
                    float alignX = rightPageX - 160.0f;

                    SpawnGenericSprite(alignX, currentY, bg, SLOT_SIZE, SLOT_SIZE);
                    SpawnUISprite(alignX, currentY, name, 1.5f);
                }

                currentY -= rowSpacing;
            }

            // 3. FINAL STEP: Show the main recipe at the bottom
            // Add a little extra spacing or a separator
            currentY -= 30.0f;
            SpawnText(rightPageX, currentY + (SLOT_SIZE / 2.0f) + 20.0f, "Final Mixture", 0.8f);

            RenderRecipeRow(rightPageX, currentY, primaryRecipe);
        }

        void RenderRecipeRow(float centerX, float y, const PotionRecipe* recipe) {
            if (!recipe) return;

            // [FIX] Layout Constants
            float iconSize = SLOT_SIZE;             // 120.0f (Same as left page)
            float arrowWidth = 80.0f;
            float colAlignmentX = -160.0f;          // Where the "Result" column sits relative to center
            float inputStartX = 100.0f;             // Where ingredients start on the right
            float itemSpacing = SLOT_SIZE + 30.0f;  // Spacing between tool/ingredients

            // --- RESULT (Left side) ---
            if (recipe->resultItemID >= 0) {
                std::string resName = Framework::kItemNames[recipe->resultItemID];
                // [FIX] Use iconSize (120) instead of 70/80
                SpawnGenericSprite(centerX + colAlignmentX, y, "Item_PotionBG", iconSize, iconSize);
                SpawnUISprite(centerX + colAlignmentX, y, resName, 1.5f);
            }

            // --- ARROW (Center) ---
            SpawnGenericSprite(centerX, y, "arrow", arrowWidth, 40.0f);

            // --- INGREDIENTS (Right side) ---
            float currentInputX = centerX + inputStartX;

            // 1. Tool (e.g. Knife)
            if (recipe->toolItemID != -1) {
                std::string toolName = Framework::kItemNames[recipe->toolItemID];
                // [FIX] Use iconSize (120)
                SpawnGenericSprite(currentInputX, y, "Item_DefaultBG", iconSize, iconSize);
                SpawnUISprite(currentInputX, y, toolName, 1.5f);

                currentInputX += itemSpacing;
            }

            // 2. Ingredients (e.g. Herb)
            for (int id : recipe->ingredientIDs) {
                if (id >= 0 && id < Framework::kItemCount) {
                    std::string bg = "Item_IngredientBG";
                    if (Framework::GetItemType(id) == Framework::ItemType::POTION) bg = "Item_PotionBG";

                    // [FIX] Use iconSize (120)
                    SpawnGenericSprite(currentInputX, y, bg, iconSize, iconSize);
                    SpawnUISprite(currentInputX, y, Framework::kItemNames[id], 1.5f);

                    currentInputX += itemSpacing;
                }
            }
        }

        // =========================================================
        //                 HELPER FUNCTIONS
        // =========================================================
        void SpawnUISprite(float x, float y, std::string itemName, float scaleMult = 1.0f) {
            if (itemName.empty()) return;
            SpawnGenericSprite(x, y, itemName, 40.0f * scaleMult, 40.0f * scaleMult);
        }

        void SpawnGenericSprite(float x, float y, std::string texName, float width, float height) {
            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0.0f);
            trans.scale = glm::vec3(width, height, 1.0f);

            MeshProperties mesh;
            TextureProperties texComp;
            HierarchyComponent hc;

            std::string fullPath = "Assets/Textures/" + texName + ".png";
            texComp.path = fullPath;
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);

            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, texComp);

            uiEntities.push_back(e);
        }

        void SpawnBook(float x, float y, float width, float height) {
            SpawnGenericSprite(x, y, "book", width, height);
        }

        void SpawnText(float x, float y, std::string textContent, float scaleMult = 1.0f) {
            if (textContent.empty()) return;

            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            // Assuming 1920x1080 reference resolution
            trans.position = glm::vec3(x / 1920.0f, y / 1080.0f, 0.0f);
            trans.scale = glm::vec3(1.0f, 1.0f, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextProperties tp;

            tp.screenSpace = true;
            tp.text = textContent;
            tp.alignment = 1; // Center
            tp.scale = scaleMult;

            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, tp);

            uiEntities.push_back(e);
        }
    };
}