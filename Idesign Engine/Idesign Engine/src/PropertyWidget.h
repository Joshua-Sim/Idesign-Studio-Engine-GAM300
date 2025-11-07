
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
#include "MaterialLoader.h"

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
    template<typename T, typename = void>
    struct has_name_and_path : std::false_type {};

    template<typename T>
    struct has_name_and_path<T, std::void_t<
        decltype(std::declval<T>().name),
        decltype(std::declval<T>().path)
        >> : std::true_type {};

    template<typename AssetType>
    void DrawAssetDropdown(const std::string& label, AssetType& component)
    {
        std::vector<std::string> assetNames;
        std::vector<AssetHandle> assetGuids;

        for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
        {
            if (entry.typeName != TypeName<AssetType>::value) continue;

            auto* asset = static_cast<AssetType*>(entry.pObject.get());

            if constexpr (has_name_and_path<AssetType>::value)
            {
                if (asset->name.empty() && asset->path.empty()) continue;
                std::string name = asset->name.empty() ? asset->path : asset->name;
                assetNames.push_back(name);
            }
            else continue;
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
                if constexpr (has_name_and_path<AssetType>::value)
                {
                    if (asset->name.empty() && asset->path.empty()) return; // Skip if no identifying info
                    component.name = asset->name.empty() ? asset->path : asset->name;
                    component.path = asset->path;
                }

                else if constexpr (std::is_same_v<AssetType, MeshProperties>)
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

                else if constexpr (std::is_same_v<AssetType, MaterialMetadata>)
                {
                    component.ambient = asset->ambient;
                    component.albedo = asset->albedo;
                    component.specular = asset->specular;
                    component.shininess = asset->shininess;
                    component.opacity = asset->opacity;
                }
            }
        }
    }

    // Overload for NavMeshComponent using MeshProperties as asset type
    inline void DrawAssetDropdown(const std::string& label, NavMeshComponent& navComp)
    {
        std::vector<std::string> meshNames;
        std::vector<AssetHandle> meshGuids;

        for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
        {
            if (entry.typeName != TypeName<MeshProperties>::value)
                continue;

            auto* mesh = static_cast<MeshProperties*>(entry.pObject.get());
            if (mesh->name.empty() && mesh->path.empty())
                continue;

            std::string name = mesh->name.empty() ? mesh->path : mesh->name;
            meshNames.push_back(name);
            meshGuids.push_back(guid);
        }

        std::vector<const char*> labelPtrs;
        for (const auto& name : meshNames)
            labelPtrs.push_back(name.c_str());

        int currentIndex = -1;
        for (size_t i = 0; i < meshGuids.size(); ++i)
        {
            if (meshGuids[i] == navComp.guid)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        ImGui::Separator();
        ImGui::Text("%s", label.c_str());
        if (ImGui::Combo(label.c_str(), &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
        {
            navComp.guid = meshGuids[currentIndex];
        }
    }

    inline void DrawTexturePreview(const TextureProperties& tex)
    {
        if (tex.textureID != 0) 
        {
             ImGui::Text("Preview:");
             ImGui::Image((ImTextureID)(uintptr_t)tex.textureID, ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0)); // Flip both horizontally and vertically
             ImGui::Text("Size: %d x %d", tex.width, tex.height);
        }
        else ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "No texture loaded.");
    }

    inline void DrawMaterialSelector(MaterialProperties& component)
    {
        static bool isCreatingNewMaterial = false;
        static bool wasCreatingNewMaterial = false;
        static MaterialProperties newMaterial;
        static char newName[64] = "NewMaterial";
        static int lastSelectedIndex = -1;

        std::vector<std::string> assetNames;
        std::vector<AssetHandle> assetGuids;

        // Add placeholder for "No material assigned"
        assetNames.push_back("No material assigned");
        assetGuids.push_back(AssetHandle{}); // Null handle

        // Populate existing materials
        for (const auto& [guid, entry] : AssetManager::Instance().GetAssets())
        {
            if (entry.typeName != TypeName<MaterialMetadata>::value)
                continue;

            auto* meta = static_cast<MaterialMetadata*>(entry.pObject.get());
            if (meta->name.empty() && meta->path.empty())
                continue;

            assetNames.push_back(meta->name.empty() ? meta->path : meta->name);
            assetGuids.push_back(guid);
        }

        // Add "Create New..." option
        assetNames.push_back("Create New...");
        assetGuids.push_back(AssetHandle{}); // Placeholder

        std::vector<const char*> labelPtrs;
        for (const auto& name : assetNames)
            labelPtrs.push_back(name.c_str());

        // Find current selection
        int currentIndex = 0;
        for (size_t i = 0; i < assetGuids.size(); ++i)
        {
            if (assetGuids[i] == component.guid)
            {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        ImGui::Separator();
        ImGui::Text("Select Material");

        if (ImGui::Combo("Select Material", &currentIndex, labelPtrs.data(), static_cast<int>(labelPtrs.size())))
        {
            isCreatingNewMaterial = (currentIndex == static_cast<int>(assetGuids.size()) - 1);

            if (isCreatingNewMaterial && !wasCreatingNewMaterial)
            {
                // First time entering creation mode
                newMaterial = MaterialProperties{};
                strcpy(newName, "NewMaterial");
            }

            if (!isCreatingNewMaterial && currentIndex != 0)
            {
                component.guid = assetGuids[currentIndex];
                auto* meta = AssetManager::Instance().Get<MaterialMetadata>(component.guid);
                if (meta)
                {
                    auto material = LoadMaterialAsset(meta->path, meta->guid);
                    if (material)
                    {
                        component.ambient = material->ambient;
                        component.albedo = material->albedo;
                        component.specular = material->specular;
                        component.shininess = material->shininess;
                        component.opacity = material->opacity;
                    }
                }
            }
            else if (currentIndex == 0)
            {
                component.guid = AssetHandle{}; // Clear
            }
            lastSelectedIndex = currentIndex;
        }
        wasCreatingNewMaterial = isCreatingNewMaterial;

        // Show creation UI
        if (isCreatingNewMaterial)
        {
            ImGui::Separator();
            ImGui::Text("Create New Material");

            ImGui::InputText("New Material Name", newName, IM_ARRAYSIZE(newName));
            ImGui::ColorEdit3("Ambient", glm::value_ptr(newMaterial.ambient));
            ImGui::ColorEdit3("Albedo", glm::value_ptr(newMaterial.albedo));
            ImGui::ColorEdit3("Specular", glm::value_ptr(newMaterial.specular));
            ImGui::DragFloat("Shininess", &newMaterial.shininess, 0.1f, 0.0f, 128.0f);
            ImGui::DragFloat("Opacity", &newMaterial.opacity, 0.01f, 0.0f, 1.0f);

            if (ImGui::Button("Save Material"))
            {
                AssetHandle newGuid = GenerateGUID();
                std::string path = "Assets/Materials/" + std::string(newName) + ".material";

                SaveMaterialAsset(newMaterial, path, newGuid);
                RegisterMaterialMetadata(newGuid, newName, path);

                component = newMaterial;
                component.guid = newGuid;

                ImGui::TextColored(ImVec4(0, 1, 0, 1), "Material saved!");
                isCreatingNewMaterial = false;
            }
        }
    }
}
#endif // !_PROPERTY_WIDGET_H_