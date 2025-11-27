#include <string>
#include <vector>
#include <iostream>
#include "Coordinator.h"
#include <string>
#include <MeshProperties.h>
#include <TransformProperties.h>
#include <TextProperties.h>
#include <TextureProperties.h>
#include <ComponentList.h>
#include "TextureLoader.h"
#include "ItemComponent.h" // CRITICAL: Includes Global Recipe Definitions

namespace Framework {

    // Helper for UI-specific data
    struct UIRecipe {
        std::vector<std::string> ingredientNames;
        std::string resultName;
    };

    class RecipeBookManager {
    private:
        Coordinator* coordinator;
        std::vector<UIRecipe> displayRecipes; // Local cache of names for rendering
        std::vector<Entity> uiEntities;

    public:
        bool bookSpawned = false;

        RecipeBookManager(Coordinator* coord) : coordinator(coord) {
            PopulateData();
        }

        // --- NEW: FETCH FROM GLOBAL SOURCE ---
        void PopulateData() {
            displayRecipes.clear();

            // 1. Get the authoritative list from ItemComponent
            const auto& globalRecipes = Framework::GetGlobalRecipes();

            // 2. Convert IDs to Names for the UI
            for (const auto& gRecipe : globalRecipes) {
                UIRecipe uiRecipe;

                // Ingredients
                for (int id : gRecipe.ingredientIDs) {
                    if (id >= 0 && id < Framework::kItemCount) {
                        uiRecipe.ingredientNames.push_back(Framework::kItemNames[id]);
                    }
                    else {
                        uiRecipe.ingredientNames.push_back("Unknown");
                    }
                }

                // Result
                if (gRecipe.resultItemID >= 0 && gRecipe.resultItemID < Framework::kItemCount) {
                    uiRecipe.resultName = Framework::kItemNames[gRecipe.resultItemID];
                }
                else {
                    uiRecipe.resultName = "Unknown";
                }

                displayRecipes.push_back(uiRecipe);
            }
        }

        // --- UPDATED BUILD FUNCTION ---
        void BuildBookUI() {
            ClearBook();

            float startX = 660.0f;
            float startY = 650.0f;
            float slotSize = 2.0f;
            float spacingX = 278.0f;
            float spacingY = 250.0f;

            float currentY = startY;

            // Iterate over our display-ready recipes
            for (const auto& recipe : displayRecipes) {
                float currentX = startX;
                size_t count = recipe.ingredientNames.size();
                float totalRowWidth = (count * spacingX) + (spacingX * 0.5f);

                float bgHeight = 180.0f;
                SpawnBook(960.0f, 400.0f, 1000.0f, 800.0f);
                SpawnRowBackground(974.0f, currentY, 800.0f, bgHeight);

                for (size_t i = 0; i < recipe.ingredientNames.size(); i++) {
                    SpawnUISprite(currentX, currentY, recipe.ingredientNames[i], slotSize);

                    // Add 'Plus' symbol logic here if needed
                    if (i < recipe.ingredientNames.size() - 1) {
                        // float plusX = currentX + (spacingX * 1.1f);
                        // SpawnUIText(plusX, currentY, "+"); 
                    }
                    currentX += spacingX;
                }

                // Arrow or Equals sign logic could go here

                SpawnUISprite(currentX + 40.0f, currentY, recipe.resultName, slotSize);

                currentY -= spacingY;
            }
            bookSpawned = true;
        }

        void ClearBook() {
            for (Entity e : uiEntities) {
                coordinator->DestroyEntity(e);
            }
            uiEntities.clear();
        }

    private:
        // ... (SpawnUISprite, SpawnUIText, SpawnBook, SpawnRowBackground remain exactly the same) ...

        void SpawnUISprite(float x, float y, std::string itemName, float scaleMult = 1.0f) {
            if (itemName.empty() || itemName == "Unknown") return;

            Entity e = coordinator->CreateEntity();

            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0.0f);
            trans.scale = glm::vec3(40.0f * scaleMult, 40.0f * scaleMult, 1.0f);

            MeshProperties mesh;
            // mesh.meshName = "Quad"; 

            TextureProperties texComp;
            HierarchyComponent hc;

            std::string fullPath = "Assets/Textures/" + itemName + ".png";
            texComp.path = fullPath;
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);

            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, texComp);

            uiEntities.push_back(e);
        }

        // ... Include the rest of your spawn functions here ...
        void SpawnBook(float x, float y, float width, float height) {
            // (Copy your existing implementation here)
            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0);
            trans.scale = glm::vec3(width, height, 1.0f);
            coordinator->AddComponent(e, trans);

            MeshProperties mesh;
            coordinator->AddComponent(e, mesh);

            TextureProperties texComp;
            texComp.path = "Assets/Textures/book.png";
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);
            coordinator->AddComponent(e, texComp);

            HierarchyComponent hc;
            coordinator->AddComponent(e, hc);

            uiEntities.push_back(e);
        }

        void SpawnRowBackground(float x, float y, float width, float height) {
            // (Copy your existing implementation here)
            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0);
            trans.scale = glm::vec3(width, height, 1.0f);
            coordinator->AddComponent(e, trans);

            MeshProperties mesh;
            coordinator->AddComponent(e, mesh);

            TextureProperties texComp;
            texComp.path = "Assets/Textures/recipe_layout.png";
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);
            coordinator->AddComponent(e, texComp);

            HierarchyComponent hc;
            coordinator->AddComponent(e, hc);

            uiEntities.push_back(e);
        }
    };
}