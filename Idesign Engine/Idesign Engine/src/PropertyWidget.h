
#pragma once
#ifndef _PROPERTY_WIDGET_H_
#define _PROPERTY_WIDGET_H_

// STL Library 
#include <algorithm>  // for std::copy, std::sort, etc.
#include <iterator>   // for std::begin, std::end, etc.

// ImGui Stuff
#include "imgui.h"
#include <gtc/type_ptr.hpp>

// Files
#include "Coordinator.h"
#include "TypeRegistry.h"
#include "PropertySystem.h"
#include "AssetManager.h"
#include "MeshLoader.h"
#include "TextureLoader.h"

// All Components || Properties
#include "TextureProperties.h"
#include "MeshProperties.h"
#include "AudioProperties.h"
#include "TransformProperties.h"
#include "TextProperties.h"
#include "NavMeshProperties.h"
#include "NavMeshAgentProperties.h"

extern Framework::Coordinator ecsInterface;
namespace Framework
{
	// For Entity Inspector
	void DrawInspector(void* pObject, const prop_list& propList);
	void DrawEntityInspector(Entity entity, Coordinator& ecs);

	// For Component Registration with reflection
	void RegisterComponentTypes();
	void AutoRegisterAllReflectedComponents(Coordinator& ecs);
    
    //------------------------------------------------------------------------------------------------------------------------------------
    //  TEMPLATE FUNCTIONS FOR WIDGETS
    //------------------------------------------------------------------------------------------------------------------------------------
    template<typename AssetType>
    void DrawAssetDropdown(const std::string& label, AssetType& component)
    {
        std::vector<std::string> assetNames;
        std::vector<AssetHandle> assetGuids;

        for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
        {
            if (entry.typeName != TypeName<AssetType>::value)
                continue;

            auto* asset = static_cast<AssetType*>(entry.pObject.get());

            if constexpr (requires(AssetType a) { a.name; a.path; })
            {
                if (asset->name.empty() && asset->path.empty())
                    continue; // Skip assets with no identifying info

                std::string name = asset->name.empty() ? asset->path : asset->name;
                assetNames.push_back(name);
            }
            else
            {
                // Fallback label for types without name/path
                assetNames.push_back(entry.typeName + " [" + std::to_string(guid) + "]");
            }

            assetGuids.push_back(guid);
        }

        std::vector<const char*> labelPtrs;
        for (const auto& name : assetNames)
            labelPtrs.push_back(name.c_str());

        int currentIndex = -1;
        for (size_t i = 0; i < assetGuids.size(); ++i)
        {
            if (assetGuids[i] == component.guid)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        ImGui::Separator();
        ImGui::Text("%s", label.c_str());
        if (ImGui::Combo(label.c_str(), &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
        {
            component.guid = assetGuids[currentIndex];

            if (auto* asset = AssetManager::Instance().Get<AssetType>(component.guid))
            {
                if constexpr (requires(AssetType a) { a.name; a.path; })
                {
                    if (asset->name.empty() && asset->path.empty())
                        return; // Skip if no identifying info

                    component.name = asset->name.empty() ? asset->path : asset->name;
                    component.path = asset->path;
                }

                if constexpr (std::is_same_v<AssetType, MeshProperties>)
                {
                    if (asset->vao == 0 && !asset->path.empty())
                    {
                        MeshLoader::loadMesh(asset->path, *asset); // Load once into asset
                    }

                    component.vao = asset->vao;
                    component.vbo = asset->vbo;
                    component.ebo = asset->ebo;
                    component.indexCount = asset->indexCount;
                }
                else if constexpr (std::is_same_v<AssetType, TextureProperties>)
                {
                    if (asset->textureID == 0) 
                    {
                        TextureLoader::loadTexture(*asset, true); // Load once
                    }
                    component.textureID = asset->textureID; // Reuse loaded texture
                }
                else if constexpr (std::is_same_v<AssetType, AudioProperties>)
                {
                    component.mode = asset->mode;
                    component.soundType = asset->soundType;
                }
                else if constexpr (std::is_same_v<AssetType, NavMeshComponent>)
                {
                    // The selected MeshProperties GUID is already stored in assetGuids[currentIndex]
                    component.guid = assetGuids[currentIndex];
                }
            }
        }
    }

    inline void DrawTexturePreview(const TextureProperties& tex)
    {
        if (tex.textureID != 0) {
            ImGui::Text("Preview:");
            ImGui::Image((ImTextureID)(uintptr_t)tex.textureID, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0)); // Flip both horizontally and vertically
            ImGui::Text("Size: %d x %d", tex.width, tex.height);
            ImGui::Text("Path: %s", tex.path.c_str());
        }
        else {
            ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No texture loaded.");
        }
    }
}
#endif // !_PROPERTY_WIDGET_H_