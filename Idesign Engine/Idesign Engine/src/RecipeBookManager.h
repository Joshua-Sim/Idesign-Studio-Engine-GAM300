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


        float bookZ = 0.4f;
        float bookmarkZ = 0.5f;
        float bookmarkIconZ = 0.6f;
        float bookflapZ = 0.9f;

        float bookCenterX = 960.0f;
        float bookCenterY = 540.0f;

        // --- LAYOUT CONSTANTS ---
        const float GRID_START_X = 510.0f;  // Top-left of the Left Page
        const float GRID_START_Y = 675.0f;
        const float SLOT_SIZE = 120.0f;
        const float PADDING = 20.0f;
        const int   GRID_COLUMNS = 3;

        // --- TAB STATE ---
        enum class BookTab { INGREDIENTS, POTIONS, ITEMS };
        BookTab m_currentTab = BookTab::INGREDIENTS;

        // --- SIDE TAB CONSTANTS ---
        const float SIDE_TAB_HIDDEN_X = 380.0f;   // Tucked under the flap
        const float SIDE_TAB_EXTENDED_X = 342.0f; // Slid out
        const float SIDE_TAB_START_Y = 700.0f;
        const float SIDE_TAB_SPACING = 150.0f;
        const float SIDE_TAB_WIDTH = 100.0f;
        const float SIDE_TAB_HEIGHT = 120.0f;
        const float SIDE_UV_MIN_X = 310.0f / 1920.0f; // EXTENDED_X left edge approx
        const float SIDE_UV_MAX_X = 430.0f / 1920.0f; // HIDDEN_X right edge approx
        float m_tabVisualX[3] = { 380.0f, 380.0f, 380.0f }; // Start at HIDDEN_X



        int m_hoveredItemID = -1;       // ID of item currently under mouse
        int m_hoveredSideTab = -1;
        Entity m_tooltipEntity = -1;    // ID of the text entity (so we can delete it)
        bool m_tooltipActive = false;   // Is the text currently showing?

        std::vector<Entity> m_tooltipEntities;

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
            RemoveTooltip(); // <--- Add this
            ClearBook();
            std::cout << "[RecipeBook] Shutdown complete." << std::endl;
        }

        // --- INPUT HANDLING ---
        void OnMouseClick(float mouseUV_X, float mouseUV_Y) {
            if (!bookSpawned) return;

            float mouseX = mouseUV_X * 1920.0f;
            float mouseY = mouseUV_Y * 1080.0f;

            // 1. SIDEBAR TAB DETECTION (Replacing floating middle tabs)
            // We check a vertical strip on the left where the bookmarks live
            float sideClickMinX = SIDE_TAB_EXTENDED_X - (SIDE_TAB_WIDTH / 2.0f);
            float sideClickMaxX = SIDE_TAB_HIDDEN_X + (SIDE_TAB_WIDTH / 2.0f);

            for (int i = 0; i < 3; ++i) {
                float tabY = SIDE_TAB_START_Y - (i * SIDE_TAB_SPACING);

                bool hitX = mouseX >= sideClickMinX && mouseX <= sideClickMaxX;
                bool hitY = mouseY >= (tabY - (SIDE_TAB_HEIGHT / 2.0f)) && mouseY <= (tabY + (SIDE_TAB_HEIGHT / 2.0f));

                if (hitX && hitY) {
                    BookTab clickedTab = static_cast<BookTab>(i);
                    if (m_currentTab != clickedTab) {
                        m_currentTab = clickedTab;
                        m_selectedItemID = -1; // Clear right page on tab switch
                        g_AudioSystem->PlayAudio("event:/PageTurn"); // Or "event:/PageTurn"
                        RefreshBook();
                    }
                    return; // Exit early: we clicked a sidebar tab
                }
            }

            // 2. ITEM GRID DETECTION
            float itemX, itemY;
            int itemID = GetItemAtPosition(mouseUV_X, mouseUV_Y, itemX, itemY);

            if (itemID != -1) {
                ItemClickedEvent data;
                data.itemID = itemID;
                EventManager::Instance().Fire("ItemClicked", &data);
            }
        }

        // --- MAIN RENDER FUNCTION ---
        void BuildBookUI() {

            float bookCenterX = 960.0f;
            float bookCenterY = 540.0f;

            // 1. BACK layer: The back cover/shadow
            SpawnBook(bookCenterX, bookCenterY, 1300.0f, 866.0f, bookZ);

            // 2. MIDDLE layer: The Side Bookmarks
            BuildSideTabs(); // Z = 0.1 and 0.2

            // 3. FRONT layer: The Book "Flap" (The page edge that covers the bookmarks)
            // Assuming you have a texture named "Book_FrontFlap"
            SpawnGenericSprite(bookCenterX, bookCenterY, "bookflap", 1300.0f, 866.0f, bookflapZ);

            // 4. CONTENT layer: Grid and Text
            BuildLeftPage(); // Ensure these spawn at Z = 0.4 or higher
            if (m_selectedItemID != -1) {
                BuildRightPage(m_selectedItemID);
            }

            bookSpawned = true;
        }

        void ClearBook() {
            RemoveTooltip(); // <--- Add this
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

        void OnMouseMove(float mouseUV_X, float mouseUV_Y) {
            if (!bookSpawned) return;

            // 1. SIDEBAR HOVER DETECTION (UV SPACE)
            int foundSideTab = -1;

            // Check if X is within the sidebar "strip" on the left
            if (mouseUV_X >= SIDE_UV_MIN_X && mouseUV_X <= SIDE_UV_MAX_X) {
                for (int i = 0; i < 3; ++i) {
                    // Calculate UV Y center of the tab
                    float tabY_UV = (SIDE_TAB_START_Y - (i * SIDE_TAB_SPACING)) / 1080.0f;
                    float halfHeight_UV = (SIDE_TAB_HEIGHT / 2.0f) / 1080.0f;

                    if (mouseUV_Y >= (tabY_UV - halfHeight_UV) && mouseUV_Y <= (tabY_UV + halfHeight_UV)) {
                        foundSideTab = i;
                        break;
                    }
                }
            }

            // Refresh if hover state changed
            if (foundSideTab != m_hoveredSideTab) {
                m_hoveredSideTab = foundSideTab;
                RefreshBook();
            }

            // 2. ITEM GRID HOVER (Your existing logic)
            float itemX, itemY;
            int foundItemID = GetItemAtPosition(mouseUV_X, mouseUV_Y, itemX, itemY);

            if (foundItemID != m_hoveredItemID) {
                RemoveTooltip();
                m_hoveredItemID = foundItemID;
                if (foundItemID != -1) {
                    CreateTooltip(foundItemID, itemX, itemY);
                }
            }
        }

        // Add to public functions:
        void UpdateVisuals(float dt) {
            if (!bookSpawned) return;

            bool needsRefresh = false;
            for (int i = 0; i < 3; ++i) {
                // Determine if this tab SHOULD be extended
                bool active = (static_cast<int>(m_currentTab) == i) || (m_hoveredSideTab == i);
                float targetX = active ? SIDE_TAB_EXTENDED_X : SIDE_TAB_HIDDEN_X;

                // Check if we need to move (using a small epsilon of 0.1f)
                if (std::abs(m_tabVisualX[i] - targetX) > 0.1f) {
                    // Smoothly interpolate: current = current + (dist * speed * dt)
                    m_tabVisualX[i] += (targetX - m_tabVisualX[i]) * 12.0f * dt;
                    needsRefresh = true;
                }
            }

            // If any tab moved, we rebuild the UI to show the new position
            if (needsRefresh) {
                RefreshBook();
            }
        }

    private:
        void CreateTooltip(int itemID, float itemX, float itemY) {
            std::string text = Framework::GetItemDisplayName(itemID);

            // --- 1. CALCULATE DIMENSIONS ---
            // A standard font at scale 1.0 usually averages 18-22 pixels wide per character.
            // 14.0f was too thin. 19.0f is a much safer average for mixed-case text.
            float charWidth = 19.0f;
            float padding = 20.0f;
            float textScale = 1.0f;
            float bgHeight = 60.0f;

            // Calculate estimated width
            float estimatedTextWidth = text.length() * charWidth * textScale;

            // Add a "Base Width" safety buffer. 
            // Even a 1-letter word needs a minimum box size.
            float minBoxWidth = 50.0f;

            // Final Width: Max of (calculated vs minimum) + Padding on both sides
            float bgWidth = estimatedTextWidth + (padding * 2.0f);

            // --- 2. POSITIONING ---
            // Position the box slightly above the item
            // (Assuming Y+ is UP, we add to Y to go up)
            float tooltipY = itemY + (SLOT_SIZE / 2.0f) + (bgHeight / 2.0f) + 10.0f;

            // [CRITICAL FIX] Text Vertical Offset
            // Text draws from the baseline (bottom). The Box draws from the center.
            // We must move the text DOWN (subtract Y) so it sits in the middle of the box.
            // Try values between 10.0f and 15.0f.
            float textVerticalOffset = 15.0f;

            float normX = itemX;
            float normY = tooltipY;

            // --- 3. SPAWN BACKGROUND (Solid Color) ---
            {
                Entity bg = coordinator->CreateEntity();
                m_tooltipEntities.push_back(bg);

                TransformComponent trans;
                trans.position = glm::vec3(normX, normY, 0.1f);
                trans.scale = glm::vec3(bgWidth, bgHeight, 1.0f);

                MeshProperties mesh;
                HierarchyComponent hc;
                TextureProperties texComp;

                texComp.path = "Assets/Textures/White.png";
                texComp.isUI = true;
                TextureLoader::loadTexture(texComp, true);

                coordinator->AddComponent(bg, hc);
                coordinator->AddComponent(bg, trans);
                coordinator->AddComponent(bg, mesh);
                coordinator->AddComponent(bg, texComp);
            }

            // --- 4. SPAWN TEXT ---
            {
                Entity txt = coordinator->CreateEntity();
                m_tooltipEntities.push_back(txt);

                TransformComponent trans;
                // [FIX APPLIED HERE]: Subtract textVerticalOffset from normY
                trans.position = glm::vec3(normX / 1920.0f, (normY - textVerticalOffset) / 1080.0f, 0.21f);
                trans.scale = glm::vec3(1.0f, 1.0f, 1.0f);

                MeshProperties mesh;
                HierarchyComponent hc;
                TextProperties tp;

                tp.screenSpace = true;
                tp.text = text;
                tp.alignment = 1; // Center
                tp.scale = textScale;

                coordinator->AddComponent(txt, hc);
                coordinator->AddComponent(txt, trans);
                coordinator->AddComponent(txt, mesh);
                coordinator->AddComponent(txt, tp);
            }

            m_tooltipActive = true;
        }

            void RemoveTooltip() {
                if (m_tooltipActive) {
                    // Destroy all parts of the tooltip (BG + Text)
                    for (Entity e : m_tooltipEntities) {
                        coordinator->DestroyEntity(e);
                    }
                    m_tooltipEntities.clear();
                    m_tooltipActive = false;
                }
            }

            int GetItemAtPosition(float mouseUV_X, float mouseUV_Y, float& outItemX, float& outItemY) {
                float mouseX = mouseUV_X * 1920.0f;
                float mouseY = mouseUV_Y * 1080.0f;

                float currentX = GRID_START_X;
                float currentY = GRID_START_Y;
                float halfSize = SLOT_SIZE / 2.0f;
                int colCounter = 0;

                for (int i = 0; i < Framework::kItemCount; i++) {
                    Framework::ItemType type = Framework::GetItemType(i);

                    // --- MUST USE EXACT SAME FILTER LOGIC AS BUILDLEFTPAGE ---
                    if (m_currentTab == BookTab::INGREDIENTS && type != Framework::ItemType::INGREDIENT) continue;
                    if (m_currentTab == BookTab::POTIONS && type != Framework::ItemType::POTION) continue;
                    if (m_currentTab == BookTab::ITEMS && type != Framework::ItemType::DEFAULT) continue;

                    // STRICT HIT CHECK
                    bool hitX = mouseX >= (currentX - halfSize) && mouseX <= (currentX + halfSize);
                    bool hitY = mouseY >= (currentY - halfSize) && mouseY <= (currentY + halfSize);

                    if (hitX && hitY) {
                        outItemX = currentX;
                        outItemY = currentY;
                        return i; // Found it!
                    }

                    // GRID ADVANCE
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

                return -1; // Nothing found
            }

        // =========================================================
        //                 LEFT PAGE: ITEM GRID
        // =========================================================
        // --- Inside BuildLeftPage ---
            void BuildLeftPage() {
                // 1. Draw Grid (Removed old tab rendering logic from here)
                float currentX = GRID_START_X;
                float currentY = GRID_START_Y;
                int colCounter = 0;

                for (int i = 0; i < Framework::kItemCount; i++) {
                    Framework::ItemType type = Framework::GetItemType(i);

                    // --- FILTER LOGIC ---
                    // Tab 0 = Ingredients, Tab 1 = Potions, Tab 2 = MortarPestle (Items/Tools)
                    if (m_currentTab == BookTab::INGREDIENTS && type != Framework::ItemType::INGREDIENT) continue;
                    if (m_currentTab == BookTab::POTIONS && type != Framework::ItemType::POTION) continue;
                    if (m_currentTab == BookTab::ITEMS && type != Framework::ItemType::DEFAULT) continue;

                    // Spawn Icon
                    SpawnUISprite(currentX, currentY, Framework::kItemNames[i], 2.0f, 1.0f);

                    // Grid Math
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
        void BuildRightPage(int selectedID) {
            float rightPageX = 960.0f + 230.0f;
            float currentY = GRID_START_Y;
            float rowSpacing = SLOT_SIZE + PADDING;

            SpawnText(rightPageX + 30.0f, currentY + 150.0f, "Ingredients Required", 1.5f);

            // 1. Get the Main Recipe for the selected item
            const PotionRecipe* primaryRecipe = Framework::GetRecipeForResult(selectedID);

            // If it's a raw item (no recipe), just show it and return
            if (!primaryRecipe) {
                SpawnGenericSprite(rightPageX, currentY, "Item_IngredientBG", SLOT_SIZE, SLOT_SIZE, 1.0f);
                SpawnUISprite(rightPageX, currentY, Framework::kItemNames[selectedID], 2.0f, 1.0f);
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

                    SpawnGenericSprite(alignX, currentY, bg, SLOT_SIZE, SLOT_SIZE, 1.0f);
                    SpawnUISprite(alignX, currentY, name, 1.5f, 1.0f);
                }

                currentY -= rowSpacing;
            }

            // 3. FINAL STEP: Show the main recipe at the bottom
            // Add a little extra spacing or a separator
            currentY -= 30.0f;
            SpawnText(rightPageX + 30.0f, currentY + (SLOT_SIZE / 2.0f) - 80.0f, "Final Mixture", 1.5f);

            RenderRecipeRow(rightPageX, currentY - 100.0f, primaryRecipe);
        }

        void BuildSideTabs() {
            std::vector<std::string> sideIcons = { "RB_Ingredient", "RB_Cauldron", "RB_MortarPestle" };
            std::vector<std::string> sideBGs = { "RB_Ingredient_BKMK", "RB_Cauldron_BKMK", "RB_MortarPestle_BKMK" };

            for (int i = 0; i < 3; ++i) {
                float currentY = SIDE_TAB_START_Y - (i * SIDE_TAB_SPACING);

                // --- THE FIX ---
                // Instead of calculating currentX here based on m_currentTab,
                // we use the visual value that UpdateVisuals() is moving frame-by-frame.
                float animatedX = m_tabVisualX[i];

                // 1. Spawn the Bookmark Background using the animated X
                SpawnGenericSprite(animatedX, currentY, sideBGs[i], SIDE_TAB_WIDTH, SIDE_TAB_HEIGHT, bookmarkZ);

                // 2. Spawn the Icon using the animated X
                SpawnGenericSprite(animatedX, currentY, sideIcons[i], 60.0f, 60.0f, bookmarkIconZ);
            }
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
                SpawnGenericSprite(centerX + colAlignmentX, y, "Item_PotionBG", iconSize, iconSize, 1.0f);
                SpawnUISprite(centerX + colAlignmentX, y, resName, 1.5f, 1.0f);
            }

            // --- ARROW (Center) ---
            SpawnGenericSprite(centerX, y, "arrow", arrowWidth, 40.0f, 1.0f);

            // --- INGREDIENTS (Right side) ---
            float currentInputX = centerX + inputStartX;

            // 1. Tool (e.g. Knife)
            if (recipe->toolItemID != -1) {
                std::string toolName = Framework::kItemNames[recipe->toolItemID];
                // [FIX] Use iconSize (120)
                SpawnGenericSprite(currentInputX, y, "Item_DefaultBG", iconSize, iconSize, 1.0f);
                SpawnUISprite(currentInputX, y, toolName, 1.5f, 1.0f);

                currentInputX += itemSpacing;
            }

            // 2. Ingredients (e.g. Herb)
            for (int id : recipe->ingredientIDs) {
                if (id >= 0 && id < Framework::kItemCount) {
                    std::string bg = "Item_IngredientBG";
                    if (Framework::GetItemType(id) == Framework::ItemType::POTION) bg = "Item_PotionBG";

                    // [FIX] Use iconSize (120)
                    SpawnGenericSprite(currentInputX, y, bg, iconSize, iconSize, 1.0f);
                    SpawnUISprite(currentInputX, y, Framework::kItemNames[id], 1.5f, 1.0f);

                    currentInputX += itemSpacing;
                }
            }
        }

        // =========================================================
        //                 HELPER FUNCTIONS
        // =========================================================
        void SpawnUISprite(float x, float y, std::string itemName, float scaleMult = 1.0f, float z = 1.0f) {
            if (itemName.empty()) return;
            SpawnGenericSprite(x, y, itemName, 40.0f * scaleMult, 40.0f * scaleMult, z);
        }

        void SpawnGenericSprite(float x, float y, std::string texName, float width, float height, float z) {
            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x, y, z);
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

        void SpawnBook(float x, float y, float width, float height, float z) {
            SpawnGenericSprite(x, y, "book", width, height, z);
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