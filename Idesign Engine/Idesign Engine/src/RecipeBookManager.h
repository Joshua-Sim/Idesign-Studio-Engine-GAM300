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

namespace Framework {
    struct Item {
        std::string name;
    };

    struct PotionRecipe {
        std::vector<Item> ingredients;
        Item resultingPotion;
    };

    class RecipeBookManager {
    private:
        Coordinator* coordinator;
        std::vector<PotionRecipe> allRecipes;
        std::vector<Entity> uiEntities;

    public:
        bool bookSpawned = false;

        RecipeBookManager(Coordinator* coord) : coordinator(coord) {
            PopulateData();
        }

        void PopulateData() {
            Item herb = { "unchopped_herb" };
            Item c_herb = { "chopped_herb" };
            Item potion = { "strength_potion" };

            PotionRecipe r1;
            PotionRecipe r2; //r2 = r1; allRecipes.push_back(r2);

            r1.ingredients = { herb, c_herb };
            r1.resultingPotion = potion;
            
            allRecipes.push_back(r1);
            
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

            for (const auto& recipe : allRecipes) {
                float currentX = startX;
                size_t count = recipe.ingredients.size();
                float totalRowWidth = (count * spacingX) + (spacingX * 0.5f);

                float bgHeight = 180.0f;               
                SpawnBook(960.0f, 400.0f, 1000.0f, 800.0f);
                SpawnRowBackground(974.0f, currentY, 800.0f, bgHeight);

                for (size_t i = 0; i < recipe.ingredients.size(); i++) {

                    SpawnUISprite(currentX, currentY, recipe.ingredients[i].name, slotSize);

                    if (i < recipe.ingredients.size() - 1) {
                        float plusX = currentX + (spacingX * 1.1f);
                        SpawnUIText(plusX, currentY, "+");
                    }
                    currentX += spacingX;
                }

                float equalX = currentX - (spacingX * 0.5f);
                SpawnUIText(equalX, currentY, "=");

                SpawnUISprite(currentX + 40.0f, currentY, recipe.resultingPotion.name, slotSize);

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

        void SpawnUISprite(float x, float y, std::string itemName, float scaleMult = 1.0f) {
            if (itemName.empty()) return;

            Entity e = coordinator->CreateEntity();

            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0.0f);
            trans.scale = glm::vec3(40.0f * scaleMult, 40.0f * scaleMult, 1.0f);

            MeshProperties mesh;
            // mesh.meshName = "Quad"; // Make sure your mesh name is set here if needed

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

        // Helper to make the Text symbols
        void SpawnUIText(float x, float y, std::string content) {
            Entity e = coordinator->CreateEntity();

            HierarchyComponent hc;
            coordinator->AddComponent(e, hc);

            // 1. Transform
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0);
            // NOTE: You might need to increase this scale if the text is too small!
            // Try 50.0f or 100.0f if you don't see it.
            trans.scale = glm::vec3(1.0f, 1.0f, 1.0f);
            coordinator->AddComponent(e, trans);

            // 2. Text Component
            TextProperties txt;
            txt.text = content;
            coordinator->AddComponent(e, txt);

            // 3. Texture Properties (For UI Flag)
            // If your text renders black or invisible, uncomment this!
            /*
            TextureProperties tex;
            tex.isUI = true;
            coordinator->AddComponent(e, tex);
            */

            uiEntities.push_back(e);
        }

        void SpawnBook(float x, float y, float width, float height) {
            Entity e = coordinator->CreateEntity();

            // 1. Transform
            // Z = 0.5f ensures it is behind ingredients (Z=0.0) and frames (Z=0.1)
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0);
            trans.scale = glm::vec3(width, height, 1.0f);
            coordinator->AddComponent(e, trans);

            // 2. Mesh
            MeshProperties mesh;
            coordinator->AddComponent(e, mesh);

            // 3. Texture
            TextureProperties texComp;
            texComp.path = "Assets/Textures/book.png"; // <--- Make sure you have this image!
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);
            coordinator->AddComponent(e, texComp);

            // 4. Hierarchy (To prevent crash)
            HierarchyComponent hc;
            coordinator->AddComponent(e, hc);

            uiEntities.push_back(e);
        }

        void SpawnRowBackground(float x, float y, float width, float height) {
            Entity e = coordinator->CreateEntity();

            // 1. Transform
            // Z = 0.5f ensures it is behind ingredients (Z=0.0) and frames (Z=0.1)
            TransformComponent trans;
            trans.position = glm::vec3(x, y, 0);
            trans.scale = glm::vec3(width, height, 1.0f);
            coordinator->AddComponent(e, trans);

            // 2. Mesh
            MeshProperties mesh;
            coordinator->AddComponent(e, mesh);

            // 3. Texture
            TextureProperties texComp;
            texComp.path = "Assets/Textures/recipe_layout.png"; // <--- Make sure you have this image!
            texComp.isUI = true;
            TextureLoader::loadTexture(texComp, true);
            coordinator->AddComponent(e, texComp);

            // 4. Hierarchy (To prevent crash)
            HierarchyComponent hc;
            coordinator->AddComponent(e, hc);

            uiEntities.push_back(e);
        }
    };
}