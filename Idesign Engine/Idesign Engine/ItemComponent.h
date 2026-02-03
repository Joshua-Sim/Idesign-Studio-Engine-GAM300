#pragma once
#ifndef _ITEM_COMPONENT_H_
#define _ITEM_COMPONENT_H_

#include "Coordinator.h"
#include <string>
#include <vector>
#include <iostream>
#include <algorithm> // For std::find, etc.
#include "PropertySystem.h"
#include "PropertyMacro.h"
#include <rapidjson/document.h>
#include <fstream>
#include <rapidjson/istreamwrapper.h>
#include <filesystem> // Make sure this is at the top
#include <rapidjson/error/en.h> // Add this to print human-readable JSON errors


namespace Framework
{
    // --- 1. Internal Structures ---
    enum class ItemType { DEFAULT = 0, INGREDIENT, POTION };

    struct ItemDefinition {
        std::string id;
        std::string displayName;
        ItemType type;
    };

    struct PotionRecipe {
        std::string recipeLabel;
        std::vector<int> ingredientIDs;
        int toolItemID;
        int resultItemID;
    };

    // --- 2. Global Storage ---
    inline std::vector<ItemDefinition>& GetItemDatabase() {
        static std::vector<ItemDefinition> items;
        return items;
    }

    inline std::vector<PotionRecipe>& GetRecipeDatabase() {
        static std::vector<PotionRecipe> recipes;
        return recipes;
    }

    // --- 3. COMPATIBILITY LAYER (Fixes 'kItemNames' errors) ---
    // This trick allows 'kItemNames[i]' to work dynamically without changing RecipeBookManager code
    struct ItemNamesProxy {
        const char* operator[](size_t index) const {
            const auto& db = GetItemDatabase();
            if (index < db.size()) return db[index].id.c_str();
            return "INVALID";
        }
    };

    inline ItemNamesProxy kItemNames;
    inline int GetGlobalItemCount() { return (int)GetItemDatabase().size(); }
    inline int kItemCount = 0;

    inline int ItemNameToId(const std::string& name) {
        const auto& items = GetItemDatabase();
        for (int i = 0; i < items.size(); ++i) {
            if (items[i].id == name) return i;
        }
        return -1;
    }

    inline std::string MakeItemIconPath(int itemId) {
        const auto& items = GetItemDatabase();
        if (itemId < 0 || itemId >= items.size()) return {};
        // Construct path: Assets/Textures/ + item_id + .png
        return std::string("Assets/Textures/") + items[itemId].id + ".png";
    }

    inline ItemType GetItemType(int id) {
        const auto& items = GetItemDatabase();
        if (id < 0 || id >= items.size()) return ItemType::DEFAULT;
        return items[id].type;
    }

    inline const std::vector<PotionRecipe>& GetGlobalRecipes() {
        return GetRecipeDatabase();
    }

    inline const PotionRecipe* GetRecipeForResult(int resultItemID) {
        const auto& recipes = GetRecipeDatabase();
        for (const auto& recipe : recipes) {
            if (recipe.resultItemID == resultItemID) return &recipe;
        }
        return nullptr;
    }

    // --- 5. JSON Loader ---
    inline void LoadItemSystem(const rapidjson::Document& doc) {
        auto& itemDB = GetItemDatabase();
        auto& recipeDB = GetRecipeDatabase();

        itemDB.clear();
        recipeDB.clear();

        // 1. Load Items
        if (doc.HasMember("items") && doc["items"].IsArray()) {
            const auto& itemsArray = doc["items"];
            for (rapidjson::SizeType i = 0; i < itemsArray.Size(); i++) {
                const auto& item = itemsArray[i];
                ItemDefinition def;
                def.id = item["id"].GetString();
                def.displayName = item["name"].GetString();

                std::string typeStr = item["type"].GetString();
                if (typeStr == "ingredient") def.type = ItemType::INGREDIENT;
                else if (typeStr == "potion") def.type = ItemType::POTION;
                else def.type = ItemType::DEFAULT;

                itemDB.push_back(def);
            }
        }

        // UPDATE COMPATIBILITY VARIABLE
        kItemCount = (int)itemDB.size();

        // 2. Load Recipes
        if (doc.HasMember("recipes") && doc["recipes"].IsArray()) {
            const auto& recipesArray = doc["recipes"];
            for (rapidjson::SizeType i = 0; i < recipesArray.Size(); i++) {
                const auto& recipeJson = recipesArray[i];
                PotionRecipe recipe;
                recipe.recipeLabel = recipeJson["id"].GetString();

                if (recipeJson.HasMember("inputs") && recipeJson["inputs"].IsArray()) {
                    for (const auto& v : recipeJson["inputs"].GetArray())
                        recipe.ingredientIDs.push_back(ItemNameToId(v.GetString()));
                }

                recipe.toolItemID = recipeJson.HasMember("tool") ? ItemNameToId(recipeJson["tool"].GetString()) : -1;
                recipe.resultItemID = recipeJson.HasMember("output") ? ItemNameToId(recipeJson["output"].GetString()) : -1;

                recipeDB.push_back(recipe);
            }
        }
    }

    inline bool InitializeItemDatabase(std::string relativePath) {
        // 1. Try to find the file by searching up the directory tree
        std::filesystem::path searchPath = relativePath;
        bool found = false;

        for (int i = 0; i < 4; ++i) {
            if (std::filesystem::exists(searchPath)) {
                found = true;
                break;
            }
            // Move search path one level up
            searchPath = "../" + searchPath.string();
        }

        if (!found) {
            std::cerr << "[ItemSystem] CRITICAL FAILURE: Could not find '" << relativePath
                << "' in current directory or parent directories." << std::endl;
            return false;
        }

        std::cout << "[ItemSystem] Found database at: " << searchPath << std::endl;

        // 2. Open the found path
        std::ifstream ifs(searchPath);
        if (!ifs.is_open()) {
            std::cerr << "[ItemSystem] Found file but could not open it (Locked? Permissions?)" << std::endl;
            return false;
        }

        // 3. Parse JSON
        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;
        doc.ParseStream(isw);

        if (doc.HasParseError()) {
            std::cerr << "[ItemSystem] JSON Parse Error: " << rapidjson::GetParseError_En(doc.GetParseError()) << std::endl;
            return false;
        }

        LoadItemSystem(doc);
        return true;
    }

    // ImGui needs this specific function signature to read from std::vector
    inline bool ItemGetter(void* data, int idx, const char** out_text)
    {
        const auto& items = GetItemDatabase();
        if (idx < 0 || idx >= items.size()) return false;
        *out_text = items[idx].id.c_str(); // Returns the ID string (e.g., "tool_knife")
        return true;
    }

    inline std::string GetItemDisplayName(int id) {
        const auto& items = GetItemDatabase();
        if (id < 0 || id >= items.size()) return "Unknown";
        return items[id].displayName;
    }




    // --- 6. ECS Components ---
    struct ItemClickedEvent { int itemID; };
    struct ItemComponent {
        std::string name;
        bool isIngredient = false;
        bool isChoppable = false;
        bool toDestroyWhenPicked = false;
        bool isZipping = false;
    };
    inline const std::vector g_ItemPropsArray = {
        PROP_STRING(name, ItemComponent), PROP_BOOL(isIngredient, ItemComponent),
        PROP_BOOL(isChoppable, ItemComponent), PROP_BOOL(toDestroyWhenPicked, ItemComponent)
    };
    inline const prop_list g_ItemProps{ "ItemComponent", g_ItemPropsArray };
    inline global_registration g_ItemPropsReg(&g_ItemProps);

} // namespace Framework

#endif // _ITEM_COMPONENT_H_