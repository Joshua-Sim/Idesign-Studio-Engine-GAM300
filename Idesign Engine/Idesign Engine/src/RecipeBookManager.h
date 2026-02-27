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

		// Left Side Page (Grid)
        float itemFrameZ = 0.5f;
		float itemIconZ = 0.65f;
        // Bookmarks
        float bookmarkZ = 0.5f;
        float bookmarkIconZ = 0.55f;
        float bookflapZ = 0.6f;
        // All Text
        float baseTextZ = 0.65f;
        float tooltipTextZ = 0.7f;

        // Right Side Tooltip
        float toolTipBackgroundZ = 0.67f; // six seven
        float toolTipIconZ = 0.7f;


        float bookCenterX = 960.0f;
        float bookCenterY = 540.0f;

        // --- LAYOUT CONSTANTS ---
        const float GRID_START_X = 500.0f;
        const float GRID_START_Y = 850.0f;
        const float SLOT_SIZE = 120.0f;
        const float PADDING_X = 50.0f;   // Horizontal gap between frames
        const float PADDING_Y = 100.0f;   // Vertical gap (increased for text + underline)
        const int   GRID_COLUMNS = 3;

        // --- TAB STATE ---
        enum class BookTab { INGREDIENTS, POTIONS, ITEMS };
        BookTab m_currentTab = BookTab::INGREDIENTS;

        // --- SIDE TAB CONSTANTS ---
        const float SIDE_TAB_HIDDEN_X = 370.0f;   // Tucked under the flap
        const float SIDE_TAB_EXTENDED_X = 342.0f; // Slid out
        const float SIDE_TAB_START_Y = 700.0f;
        const float SIDE_TAB_SPACING = 150.0f;
        const float SIDE_TAB_WIDTH = 100.0f;
        const float SIDE_TAB_HEIGHT = 120.0f;
        const float SIDE_UV_MIN_X = 310.0f / 1920.0f; // EXTENDED_X left edge approx
        const float SIDE_UV_MAX_X = 430.0f / 1920.0f; // HIDDEN_X right edge approx
        float m_tabVisualX[3] = { 380.0f, 380.0f, 380.0f }; // Start at HIDDEN_X

        // Private members
        Entity m_tabBGEntities[3] = { 0, 0, 0 };
        Entity m_tabIconEntities[3] = { 0, 0, 0 };

        // HOVER STUFF
        int m_hoveredItemID = -1;      
        int m_hoveredSideTab = -1;
        std::map<int, Entity> m_leftPageIconEntities;
        Entity m_tooltipEntity = -1;   
        bool m_tooltipActive = false;   

        std::vector<std::pair<int, glm::vec2>> m_usedInHitboxes;
        int m_hoveredRightIconID = -1;
        bool m_rightTooltipActive = false;
        std::vector<Entity> m_rightTooltipEntities;
        std::vector<Entity> m_tooltipEntities;

        std::vector<std::pair<int, glm::vec2>> m_recipeIngredientHitboxes;
        int m_hoveredRecipeIngID = -1;
        bool m_recipeTooltipActive = false;
        std::vector<Entity> m_recipeTooltipEntities;

        std::map<int, Entity> m_recipeIngredientBGEntities;
        std::map<int, Entity> m_recipeIngredientIconEntities;

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
                        g_AudioSystem->PlayAudio("event:/PaperUp");
                        RefreshBook();
                    }
                    return;
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

            // 4. RIGHT PAGE "RECIPE INGREDIENT" CLICK
            float scrollIconHalf_UV_X = 50.0f / 1920.0f;
            float scrollIconHalf_UV_Y = 50.0f / 1080.0f;

            for (const auto& hit : m_recipeIngredientHitboxes) {
                float hitUV_X = hit.second.x / 1920.0f;
                float hitUV_Y = hit.second.y / 1080.0f;

                if (mouseUV_X >= (hitUV_X - scrollIconHalf_UV_X) && mouseUV_X <= (hitUV_X + scrollIconHalf_UV_X) &&
                    mouseUV_Y >= (hitUV_Y - scrollIconHalf_UV_Y) && mouseUV_Y <= (hitUV_Y + scrollIconHalf_UV_Y)) {

                    // 1. Force the book back to the INGREDIENTS tab
                    if (m_currentTab != BookTab::INGREDIENTS) {
                        m_currentTab = BookTab::INGREDIENTS;
                        g_AudioSystem->PlayAudio("event:/PaperUp");
                    }

                    // 2. Fire the click event to select the specific ingredient!
                    ItemClickedEvent data;
                    data.itemID = hit.first;
                    EventManager::Instance().Fire("ItemClicked", &data);

                    return; // Stop checking
                }
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
            RemoveRightTooltip(); 
            m_usedInHitboxes.clear();
            m_hoveredRightIconID = -1; 

            // --- ADD THESE TWO LINES ---
            m_recipeIngredientBGEntities.clear();
            m_recipeIngredientIconEntities.clear();

            RemoveRecipeTooltip();
            m_recipeIngredientHitboxes.clear();
            m_hoveredRecipeIngID = -1;

            RemoveTooltip();
            // Use a local copy or clear carefully
            for (Entity e : uiEntities) {
                // Safety check: is it a valid entity?
                if (e != 0) {
                    coordinator->DestroyEntity(e);
                }
            }
            uiEntities.clear();
            m_leftPageIconEntities.clear(); 
            for (int i = 0; i < 3; ++i) {
                m_tabBGEntities[i] = 0;
                m_tabIconEntities[i] = 0;
            }

            bookSpawned = false;
        }

        // Helper to refresh when state changes (avoids destroying/playing audio if not needed)
        void RefreshBook() {
            ClearBook();
            BuildBookUI();
        }

        void OnMouseMove(float mouseUV_X, float mouseUV_Y) {
            if (!bookSpawned) return;

            // --- STANDARDIZATION ---
            // Convert to pixel space once at the top to avoid doing messy UV division in every loop
            float mouseX = mouseUV_X * 1920.0f;
            float mouseY = mouseUV_Y * 1080.0f;

            // 1. SIDEBAR HOVER DETECTION
            int foundSideTab = -1;

            // Check if X is within the sidebar "strip" on the left (Using your existing UV constants)
            if (mouseUV_X >= SIDE_UV_MIN_X && mouseUV_X <= SIDE_UV_MAX_X) {
                for (int i = 0; i < 3; ++i) {
                    // Standardized to pixel space checking
                    float tabY = SIDE_TAB_START_Y - (i * SIDE_TAB_SPACING);
                    float halfHeight = SIDE_TAB_HEIGHT / 2.0f;

                    if (mouseY >= (tabY - halfHeight) && mouseY <= (tabY + halfHeight)) {
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

            // 2. LEFT PAGE HOVER
            float itemX, itemY;
            // GetItemAtPosition already expects UVs based on its parameters
            int foundItemID = GetItemAtPosition(mouseUV_X, mouseUV_Y, itemX, itemY);
            if (foundItemID != m_hoveredItemID) {
                m_hoveredItemID = foundItemID;
            }

            // 3. RIGHT PAGE HOVER (Used In)
            int hoveredOutID = -1;
            float hitX = 0, hitY = 0;
            float smallIconHalf = 40.0f; // Half of the 80.0f icon

            // Standardized pixel collision
            for (const auto& hit : m_usedInHitboxes) {
                if (mouseX >= (hit.second.x - smallIconHalf) && mouseX <= (hit.second.x + smallIconHalf) &&
                    mouseY >= (hit.second.y - smallIconHalf) && mouseY <= (hit.second.y + smallIconHalf)) {

                    hoveredOutID = hit.first;
                    hitX = hit.second.x; // Keep original pixel coords for spawning the tooltip
                    hitY = hit.second.y;
                    break;
                }
            }

            // Spawn or destroy the tooltip if the hover state changed
            if (hoveredOutID != m_hoveredRightIconID) {
                RemoveRightTooltip();
                m_hoveredRightIconID = hoveredOutID;
                if (hoveredOutID != -1) {
                    CreateRightTooltip(hoveredOutID, hitX, hitY);
                }
            }

            // 4. RIGHT PAGE "RECIPE INGREDIENT" HOVER
            int hoveredRecipeIng = -1;
            float rHitX = 0, rHitY = 0;
            float scrollIconHalf = 50.0f; // Half of the 100.0f scroll icon

            // Standardized pixel collision
            for (const auto& hit : m_recipeIngredientHitboxes) {
                if (mouseX >= (hit.second.x - scrollIconHalf) && mouseX <= (hit.second.x + scrollIconHalf) &&
                    mouseY >= (hit.second.y - scrollIconHalf) && mouseY <= (hit.second.y + scrollIconHalf)) {

                    hoveredRecipeIng = hit.first;
                    rHitX = hit.second.x;
                    rHitY = hit.second.y;
                    break;
                }
            }

            if (hoveredRecipeIng != m_hoveredRecipeIngID) {
                RemoveRecipeTooltip();
                m_hoveredRecipeIngID = hoveredRecipeIng;
                if (hoveredRecipeIng != -1) {
                    CreateRecipeTooltip(hoveredRecipeIng, rHitX, rHitY);
                }
            }
        }

        // Add to public functions:
        void UpdateVisuals(float dt) {
            if (!bookSpawned) return;

            for (int i = 0; i < 3; ++i) {
                // 1. Calculate the goal
                bool active = (static_cast<int>(m_currentTab) == i) || (m_hoveredSideTab == i);
                float targetX = active ? SIDE_TAB_EXTENDED_X : SIDE_TAB_HIDDEN_X;

                if (std::abs(m_tabVisualX[i] - targetX) > 0.1f) {
                    m_tabVisualX[i] += (targetX - m_tabVisualX[i]) * 15.0f * dt;

                    // Use 0 or your specific NULL_ENTITY constant
                    if (m_tabBGEntities[i] != 0) {
                        auto& trans = coordinator->GetComponent<TransformComponent>(m_tabBGEntities[i]);
                        trans.position.x = m_tabVisualX[i];
                    }
                    if (m_tabIconEntities[i] != 0) {
                        auto& trans = coordinator->GetComponent<TransformComponent>(m_tabIconEntities[i]);
                        trans.position.x = m_tabVisualX[i];
                    }
                }
            }

            // 2. Left Page Icon Hover Animation
            float baseIconSize = 80.0f;     // Your default size (40.0f * 2.0f)
            float hoveredIconSize = 100.0f; // The enlarged size
            float lerpSpeed = 15.0f;        // How snappy the animation is

            for (const auto& pair : m_leftPageIconEntities) {
                int itemID = pair.first;
                Entity entity = pair.second;

                if (entity != 0) {
                    auto& trans = coordinator->GetComponent<TransformComponent>(entity);

                    // Determine goal size based on hover state
                    float targetSize = (itemID == m_hoveredItemID) ? hoveredIconSize : baseIconSize;

                    // Smoothly interpolate X and Y scale towards the target size
                    if (std::abs(trans.scale.x - targetSize) > 0.1f) {
                        trans.scale.x += (targetSize - trans.scale.x) * lerpSpeed * dt;
                        trans.scale.y += (targetSize - trans.scale.y) * lerpSpeed * dt;
                    }
                }
            }

            // 3. Right Page Recipe Ingredient Hover Animation
            float baseRecipeBGSize = 100.0f; // The smallIconSize from BuildRightPage
            float hoveredRecipeBGSize = 115.0f; // Popped out size

            float baseRecipeIconSize = 40.0f * 1.3f; // 52.0f base scale
            float hoveredRecipeIconSize = baseRecipeIconSize * 1.15f; // Scale up proportionally

            for (const auto& pair : m_recipeIngredientBGEntities) {
                int ingID = pair.first;
                Entity bgEnt = pair.second;
                Entity iconEnt = m_recipeIngredientIconEntities[ingID];

                float targetBGSize = (ingID == m_hoveredRecipeIngID) ? hoveredRecipeBGSize : baseRecipeBGSize;
                float targetIconSize = (ingID == m_hoveredRecipeIngID) ? hoveredRecipeIconSize : baseRecipeIconSize;

                // Smoothly animate the item icon inside it
                if (iconEnt != 0) {
                    auto& trans = coordinator->GetComponent<TransformComponent>(iconEnt);
                    if (std::abs(trans.scale.x - targetIconSize) > 0.1f) {
                        trans.scale.x += (targetIconSize - trans.scale.x) * lerpSpeed * dt;
                        trans.scale.y += (targetIconSize - trans.scale.y) * lerpSpeed * dt;
                    }
                }
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

        void RemoveRightTooltip() {
            if (m_rightTooltipActive) {
                for (Entity e : m_rightTooltipEntities) {
                    // 1. Destroy the actual ECS components
                    coordinator->DestroyEntity(e);

                    // 2. Erase the "ghost" ID from the master render list
                    auto it = std::remove(uiEntities.begin(), uiEntities.end(), e);
                    if (it != uiEntities.end()) {
                        uiEntities.erase(it, uiEntities.end());
                    }
                }

                m_rightTooltipEntities.clear();
                m_rightTooltipActive = false;
            }
        }

        void CreateRightTooltip(int itemID, float iconX, float iconY) {
            const PotionRecipe* recipe = Framework::GetRecipeForResult(itemID);
            std::string methodText = recipe ? recipe->method : "Unknown";
            std::string itemName = Framework::GetItemDisplayName(itemID);

            // --- LAYOUT MATH ---
            float tooltipWidth = 248.0f;
            float tooltipHeight = 42.0f;
            float tooltipX = iconX;
            float tooltipY = iconY + 70.0f;

            // 1. Tooltip Background
            m_rightTooltipEntities.push_back(SpawnGenericSpriteReturnEntity(tooltipX, tooltipY, "RB_TooltipBackground", tooltipWidth, tooltipHeight, toolTipBackgroundZ));

            // 2. Method Pill (Top Left, overlapping the edge)
            float methodWidth = 98.0f;
            float methodHeight = 20.0f;
            float methodX = tooltipX - (tooltipWidth / 2.0f) + (methodWidth / 2.0f) + 15.0f;
            float methodY = tooltipY + (tooltipHeight / 2.0f);

            m_rightTooltipEntities.push_back(SpawnGenericSpriteReturnEntity(methodX, methodY, "RB_TooltipMethod", methodWidth, methodHeight, toolTipIconZ));
            m_rightTooltipEntities.push_back(SpawnTextReturnEntity(methodX, methodY - 5.0f, tooltipTextZ, methodText, 0.4f));

            // 3. Output Item Text
            m_rightTooltipEntities.push_back(SpawnTextReturnEntity(tooltipX - 40.0f, tooltipY - 10.0f, tooltipTextZ, itemName, 0.5f));

            // 4. Output Icon (Right Side)
            float rightIconX = tooltipX + (tooltipWidth / 2.0f) - 50.0f;
            m_rightTooltipEntities.push_back(SpawnGenericSpriteReturnEntity(rightIconX, tooltipY, "RB_ItemBackground", 40.0f, 40.0f, toolTipIconZ));
            m_rightTooltipEntities.push_back(SpawnGenericSpriteReturnEntity(rightIconX, tooltipY, Framework::kItemNames[itemID], 30.0f, 30.0f, toolTipIconZ + 0.05f));

            m_rightTooltipActive = true;
        }

        void RemoveRecipeTooltip() {
            if (m_recipeTooltipActive) {
                for (Entity e : m_recipeTooltipEntities) {
                    coordinator->DestroyEntity(e);

                    auto it = std::remove(uiEntities.begin(), uiEntities.end(), e);
                    if (it != uiEntities.end()) uiEntities.erase(it, uiEntities.end());
                }
                m_recipeTooltipEntities.clear();
                m_recipeTooltipActive = false;
            }
        }

        void CreateRecipeTooltip(int itemID, float iconX, float iconY) {
            std::string itemName = Framework::GetItemDisplayName(itemID);

            // Dynamic width based on name length
            float textScale = 0.6f;
            float charWidth = 15.0f;
            float bgWidth = (itemName.length() * charWidth) + 30.0f; // 30 is padding
            float bgHeight = 40.0f;

            float tooltipX = iconX;
            float tooltipY = iconY + 70.0f; // 70px above the icon

            m_recipeTooltipEntities.push_back(SpawnGenericSpriteReturnEntity(tooltipX, tooltipY, "RB_TooltipBackground", bgWidth, bgHeight, toolTipBackgroundZ));
            m_recipeTooltipEntities.push_back(SpawnTextReturnEntity(tooltipX, tooltipY - 5.0f, tooltipTextZ, itemName, textScale));

            m_recipeTooltipActive = true;
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
                    currentY -= (SLOT_SIZE + PADDING_Y);
                }
                else {
                    currentX += (SLOT_SIZE + PADDING_X);
                }
            }

            return -1; // Nothing found
        }

        // =========================================================
        //                 LEFT PAGE: ITEM GRID
        // =========================================================
        void BuildLeftPage() {
            float currentX = GRID_START_X;
            float currentY = GRID_START_Y;
            int colCounter = 0;

            for (int i = 0; i < Framework::kItemCount; i++) {
                Framework::ItemType type = Framework::GetItemType(i);

                // --- FILTER LOGIC ---
                if (m_currentTab == BookTab::INGREDIENTS && type != Framework::ItemType::INGREDIENT) continue;
                if (m_currentTab == BookTab::POTIONS && type != Framework::ItemType::POTION) continue;
                if (m_currentTab == BookTab::ITEMS && type != Framework::ItemType::DEFAULT) continue;

                // 1. Background Frame
                SpawnGenericSprite(currentX, currentY, "RB_ItemBackground", 125.0f, 125.0f, itemFrameZ);

				// 2. Spawning Item Icons + Store the entity for hover detection
                Entity iconEntity = SpawnUISpriteReturnEntity(currentX, currentY, Framework::kItemNames[i], 2.0f, itemIconZ);
                m_leftPageIconEntities[i] = iconEntity;

                // 3. Item Name Text (Centered below the slot)
                float textY = currentY - (SLOT_SIZE / 2.0f) - 25.0f;
                std::string displayName = Framework::GetItemDisplayName(i);
                SpawnText(currentX, textY, baseTextZ, displayName, 0.5f);

                // 4. Underline (Centered below the text)
                float underlineY = textY - 10.0f;
                // Assuming RB_Underline is roughly the width of the slot and relatively thin
                SpawnGenericSprite(currentX, underlineY, "RB_ItemOutline", 160.0f, 5.0f, itemIconZ);

                // Grid Math
                colCounter++;
                if (colCounter >= GRID_COLUMNS) {
                    colCounter = 0;
                    currentX = GRID_START_X;
                    currentY -= (SLOT_SIZE + PADDING_Y); // Use the new vertical padding
                }
                else {
                    currentX += (SLOT_SIZE + PADDING_X); // Use the new horizontal padding
                }
            }
        }

        // =========================================================
        //                  RIGHT PAGE: ITEM LORE & USAGE
        // =========================================================
        // =========================================================
        //                  RIGHT PAGE: ITEM LORE & USAGE
        // =========================================================
        void BuildRightPage(int selectedID) {
            float rightPageX = 1250.0f; // 3/4 of 1920
            float currentY = 767.0f;
            glm::vec4 black = { 0.0f, 0.0f, 0.0f, 1.0f };

            // --- 1. SHARED ENLARGED HEADER ---
            SpawnGenericSpriteReturnEntity(rightPageX, currentY, "RB_ItemBackground", 200.0f, 200.0f, itemFrameZ);
            SpawnUISpriteReturnEntity(rightPageX, currentY, Framework::kItemNames[selectedID], 3.5f, itemIconZ);

            float textY = currentY - 100.0f - 30.0f;
            SpawnTextReturnEntity(rightPageX, textY, baseTextZ, Framework::GetItemDisplayName(selectedID), 0.8f);
            SpawnGenericSpriteReturnEntity(rightPageX, textY - 15.0f, "RB_ItemOutline", 250.0f, 8.0f, itemIconZ);

            currentY = textY - 40.0f;

            // --- 2. SHARED DIVIDER 1 ---
            SpawnGenericSpriteReturnEntity(rightPageX, std::round(currentY), "RB_Divider", 522.0f, 2.0f, itemFrameZ);
            currentY -= 30.0f;

            // --- BRANCH: POTION vs INGREDIENT ---
            if (Framework::GetItemType(selectedID) == Framework::ItemType::POTION) {

                // --- POTION LAYOUT: RECIPE SCROLL ---
                SpawnTextReturnEntity(rightPageX - 160.0f, currentY, baseTextZ, "Recipe:", 0.5f, black, 0);
                currentY -= 70.0f;

                // Spawn the parchment scroll background
                SpawnGenericSpriteReturnEntity(rightPageX, currentY, "RB_RecipeBackground", 510.0f, 133.0f, itemFrameZ);

                // Fetch the recipe to draw the ingredients
                const PotionRecipe* recipe = Framework::GetRecipeForResult(selectedID);

                if (recipe && !recipe->ingredientIDs.empty()) {
                    float smallIconSize = 90.0f;
                    float spacing = smallIconSize + 20.0f;

                    // Start rendering ingredients from the left side of the scroll
                    float startX = rightPageX - 145.0f;

                    for (int ingID : recipe->ingredientIDs) {
                        // --- CHANGED: Using standard item background and standard Z-layering ---
                        Entity bg = SpawnGenericSpriteReturnEntity(startX, currentY, "RB_ItemBackground", smallIconSize, smallIconSize, itemFrameZ + 0.01f);
                        Entity icon = SpawnUISpriteReturnEntity(startX, currentY, Framework::kItemNames[ingID], 1.7f, itemIconZ);

                        // --- STORE ENTITIES FOR ANIMATION ---
                        m_recipeIngredientBGEntities[ingID] = bg;
                        m_recipeIngredientIconEntities[ingID] = icon;
                        m_recipeIngredientHitboxes.push_back({ ingID, glm::vec2(startX, currentY) });

                        startX += spacing;
                    }
                }
                else {
                    SpawnTextReturnEntity(rightPageX, currentY, baseTextZ, "Recipe Unknown", 0.5f, black, 1);
                }

                currentY -= 90.0f;

            }
            else {

                // --- INGREDIENT LAYOUT: LORE & USED IN ---
                SpawnTextReturnEntity(rightPageX - 160.0f, currentY, baseTextZ, "Found in:", 0.5f, black, 0);
                currentY -= 40.0f;
                SpawnTextReturnEntity(rightPageX, currentY, baseTextZ, Framework::GetItemLocation(selectedID), 0.8f);

                currentY -= 30.0f;

                SpawnGenericSpriteReturnEntity(rightPageX, std::round(currentY), "RB_Divider", 522.0f, 2.0f, itemFrameZ);
                currentY -= 30.0f;

                SpawnTextReturnEntity(rightPageX - 160.0f, currentY, baseTextZ, "Can be used in:", 0.5f, black, 0);
                currentY -= 60.0f;

                std::vector<int> usedInOutputs;
                for (const auto& recipe : Framework::GetGlobalRecipes()) {
                    if (std::find(recipe.ingredientIDs.begin(), recipe.ingredientIDs.end(), selectedID) != recipe.ingredientIDs.end()) {
                        if (recipe.resultItemID != -1) {
                            usedInOutputs.push_back(recipe.resultItemID);
                        }
                    }
                }

                if (usedInOutputs.empty()) {
                    SpawnTextReturnEntity(rightPageX, currentY, baseTextZ, "None", 0.5f);
                }
                else {
                    float smallIconSize = 80.0f;
                    float spacing = smallIconSize + 20.0f;
                    float startX = rightPageX - ((usedInOutputs.size() - 1) * spacing / 2.0f);

                    for (int outID : usedInOutputs) {
                        SpawnGenericSpriteReturnEntity(startX, currentY, "RB_ItemBackground", smallIconSize, smallIconSize, itemFrameZ);
                        SpawnUISpriteReturnEntity(startX, currentY, Framework::kItemNames[outID], 1.3f, itemIconZ);

                        // Track hover regions for the tooltip
                        m_usedInHitboxes.push_back({ outID, glm::vec2(startX, currentY) });

                        startX += spacing;
                    }
                }

                currentY -= 60.0f;
            }

            // --- 3. SHARED DIVIDER 2 ---
            SpawnGenericSpriteReturnEntity(rightPageX, std::round(currentY), "RB_Divider", 522.0f, 2.0f, itemFrameZ);
            currentY -= 50.0f;

            // --- 4. SHARED FLAVOR TEXT ---
            glm::vec4 flavorColor = glm::vec4(0.5f, 0.45f, 0.4f, 1.0f);
            SpawnTextReturnEntity(rightPageX, currentY, baseTextZ, "\"" + Framework::GetItemFlavorText(selectedID) + "\"", 0.45f, flavorColor);
        }

        void BuildSideTabs() {
            std::vector<std::string> sideIcons = { "RB_Ingredient", "RB_Cauldron", "RB_MortarPestle" };
            std::vector<std::string> sideBGs = { "RB_Ingredient_BKMK", "RB_Cauldron_BKMK", "RB_MortarPestle_BKMK" };

            for (int i = 0; i < 3; ++i) {
                float currentY = SIDE_TAB_START_Y - (i * SIDE_TAB_SPACING);
                float startX = m_tabVisualX[i]; // Use current animated position

                // Store the IDs so UpdateVisuals can find them
                m_tabBGEntities[i] = SpawnGenericSpriteReturnEntity(startX, currentY, sideBGs[i], SIDE_TAB_WIDTH, SIDE_TAB_HEIGHT, bookmarkZ);
                m_tabIconEntities[i] = SpawnGenericSpriteReturnEntity(startX, currentY, sideIcons[i], 60.0f, 60.0f, bookmarkIconZ);
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

        Entity SpawnUISpriteReturnEntity(float x, float y, std::string itemName, float scaleMult = 1.0f, float z = 1.0f) {
            if (itemName.empty()) return 0;
            return SpawnGenericSpriteReturnEntity(x, y, itemName, 40.0f * scaleMult, 40.0f * scaleMult, z);
        }

        Entity SpawnGenericSpriteReturnEntity(float x, float y, std::string texName, float width, float height, float z) {
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
            return e; // Return the ID so we can track it
        }

        void SpawnText(float x, float y, float z, std::string textContent, float scaleMult = 1.0f, glm::vec4 color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), int alignment = 1) {
            if (textContent.empty()) return;

            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            // Assuming 1920x1080 reference resolution
            trans.position = glm::vec3(x / 1920.0f, y / 1080.0f, z);
            trans.scale = glm::vec3(1.0f, 1.0f, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextProperties tp;

            tp.screenSpace = true;
            tp.text = textContent;
            tp.alignment = alignment; // Center
            tp.scale = scaleMult;
            tp.color = color;
            
            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, tp);

            uiEntities.push_back(e);
        }

        Entity SpawnTextReturnEntity(float x, float y, float z, std::string textContent, float scaleMult = 1.0f, glm::vec4 color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f), int alignment = 1) {
            if (textContent.empty()) return 0;

            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x / 1920.0f, y / 1080.0f, z);
            trans.scale = glm::vec3(1.0f, 1.0f, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            TextProperties tp;

            tp.screenSpace = true;
            tp.text = textContent;
            tp.alignment = alignment;
            tp.scale = scaleMult;
            tp.color = color;
            
            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, tp);

            uiEntities.push_back(e);
            return e;
        }
    };


}