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
#include "AudioSystem.h"

namespace Framework {

    class RecipeBookManager {

    private:
        Coordinator* coordinator;
        std::vector<Entity> uiEntities;

    public:
        bool bookSpawned = false;

        RecipeBookManager(Coordinator* coord) : coordinator(coord) {
            // No need to populate data anymore, we read directly from GlobalRecipes
        }

        void Shutdown()
        {
            ClearBook();
            std::cout << "[RecipeBook] Shutdown complete." << std::endl;
        }

        // --- DIRECTLY USE GLOBAL DATA ---
        void BuildBookUI() {
            g_AudioSystem->PlayAudio("event:/PaperUp"); 

            float startX = 660.0f;
            float startY = 630.0f;
            float slotSize = 2.0f;
            float spacingX = 278.0f;
            float spacingY = 250.0f;

            // 1. SPAWN THE MAIN BOOK BACKGROUND
            SpawnBook(960.0f, 400.0f, 1000.0f, 800.0f);

            float currentY = startY;

            const auto& globalRecipes = Framework::GetGlobalRecipes();

            for (const auto& recipe : globalRecipes) {

                float currentX = startX;

                float bgHeight = 180.0f;
                SpawnPotionName(974.0f, currentY + 100.0f, recipe.potionName, 1.5f);
                SpawnRowBackground(974.0f, currentY, 800.0f, bgHeight);

                for (size_t i = 0; i < recipe.ingredientIDs.size(); i++) {

                    int id = recipe.ingredientIDs[i];

                    if (id >= 0 && id < Framework::kItemCount) {
                        std::string texName = Framework::kItemNames[id];
                        SpawnUISprite(currentX, currentY, texName, slotSize);
                    }

                    currentX += spacingX;
                }

                // --- RESULT (POTION) ---
                int resultID = recipe.resultItemID;
                if (resultID >= 0 && resultID < Framework::kItemCount) {
                    // DIRECT LOOKUP: int -> string
                    std::string resultName = Framework::kItemNames[resultID];
                    SpawnUISprite(currentX + 40.0f, currentY, resultName, slotSize);
                }

                // Move down
                currentY -= spacingY;
            }
            bookSpawned = true;
        }

        void ClearBook() {
            g_AudioSystem->PlayAudio("event:/PaperDown");
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

        void SpawnPotionName(float x, float y, std::string potionName, float scaleMult = 1.0f) {
            if (potionName.empty()) return;

            Entity e = coordinator->CreateEntity();
            TransformComponent trans;
            trans.position = glm::vec3(x / 1920, y / 1080, 0.0f);
            trans.scale = glm::vec3(1.0f,1.0f, 1.0f);

            MeshProperties mesh;
            HierarchyComponent hc;
            
            TextProperties tp;
            tp.screenSpace = true;
            tp.text = potionName;
            tp.alignment = 1;
            tp.scale = scaleMult;
            coordinator->AddComponent(e, hc);
            coordinator->AddComponent(e, trans);
            coordinator->AddComponent(e, mesh);
            coordinator->AddComponent(e, tp);

            uiEntities.push_back(e);
        }

        void SpawnBook(float x, float y, float width, float height) {
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