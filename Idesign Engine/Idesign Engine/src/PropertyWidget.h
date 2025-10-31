
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

extern Framework::Coordinator ecsInterface;
namespace Framework
{
	// For Entity Inspector
	void DrawInspector(void* pObject, const prop_list& propList);
	void DrawEntityInspector(Entity entity, Coordinator& ecs);

	// For Component Registration with reflection
	void RegisterComponentTypes();
	void AutoRegisterAllReflectedComponents(Coordinator& ecs);
    
    /////////////////////////////////////////////////////////////////////////////////////////////////
    // TEMPLATE FUNCTIONS
    /////////////////////////////////////////////////////////////////////////////////////////////////
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
            std::string name = asset->name.empty() ? asset->path : asset->name;
            assetNames.push_back(name);
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
                component.name = asset->name;
                component.path = asset->path;

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
            }
        }
    }
}
#endif // !_PROPERTY_WIDGET_H_