
#include "pch.h"
#include "imgui.h"
#include "PropertySystem.h"
#include "PropertyWidget.h"

namespace Framework
{
    void DrawInspector(void* pObject, const prop_list& propList)
    {
        int propIndex = 0;

        for (const auto& prop : propList.m_Props)
        {
            // Push a unique ID for each property instance
            ImGui::PushID(reinterpret_cast<uintptr_t>(pObject));
            ImGui::PushID(prop.m_pName);
            //printf("Inspecting property [%d]: %s\n", propIndex, prop.m_pName);

            if (prop.m_Flags & prop::HIDDEN)
            {
                //printf("  Skipped (HIDDEN)\n");
                ++propIndex;
                ImGui::PopID(); // pop prop.m_pName
                ImGui::PopID(); // pop pObject
                continue;
            }

            ImGui::Text("%s", prop.m_pName);
            if (prop.m_pTooltip)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", prop.m_pTooltip);
            }

            bool readOnly = (prop.m_Flags & prop::READ_ONLY);
            //printf("  Calling getter for type %d...\n", prop.m_Type);

            switch (prop.m_Type)
            {
            case prop::INT:
            {
                int value = 0;
                prop.m_fnGet(pObject, &value);
                if (prop.m_HasRange)
                    ImGui::SliderInt(prop.m_pName, &value, std::get<int>(prop.m_Min), std::get<int>(prop.m_Max));
                else
                    ImGui::InputInt(prop.m_pName, &value);
                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);
                break;
            }
            case prop::FLOAT:
            {
                float value = 0.0f;
                prop.m_fnGet(pObject, &value);
                if (prop.m_HasRange)
                    ImGui::SliderFloat(prop.m_pName, &value, prop.GetMinAsFloat(), prop.GetMaxAsFloat());
                else
                    ImGui::InputFloat(prop.m_pName, &value);
                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);
                break;
            }
            case prop::DOUBLE:
            {
                double value = 0.0;
                prop.m_fnGet(pObject, &value);
                ImGui::InputDouble(prop.m_pName, &value);
                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);
                break;
            }
            case prop::BOOL:
            {
                bool value = false;
                prop.m_fnGet(pObject, &value);
                ImGui::Checkbox(prop.m_pName, &value);
                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);
                break;
            }
            case prop::CHAR:
            {
                char buffer[2] = {};
                prop.m_fnGet(pObject, buffer);
                buffer[1] = '\0';
                ImGui::InputText(prop.m_pName, buffer, sizeof(buffer));
                // This is to avoid any label collision if multiple CHAR fields exist in the future 
                // ImGui:InputTextWithHint("##char", buffer, sizeof(buffer)); 
                if (!readOnly && ImGui::IsItemEdited())
                {
                    char value = buffer[0];
                    prop.m_fnSet(pObject, &value);
                }
                break;
            }
            case prop::STRING:
            {
                std::string value;
                prop.m_fnGet(pObject, &value);

                char buffer[256] = {};
                std::strncpy(buffer, value.c_str(), sizeof(buffer));
                buffer[sizeof(buffer) - 1] = '\0';

                if (readOnly) ImGui::BeginDisabled();
                if (ImGui::InputText(prop.m_pName, buffer, sizeof(buffer)))
                {
                    if (!readOnly)
                    {
                        std::string newValue(buffer);
                        prop.m_fnSet(pObject, &newValue);
                    }
                }
                if (readOnly) ImGui::EndDisabled();
                break;
            }
            case prop::UINT:
            {
                unsigned int value = 0;
                prop.m_fnGet(pObject, &value);

                if (readOnly) ImGui::BeginDisabled();

                if (prop.m_HasRange)
                {
                    ImGui::SliderScalar(prop.m_pName, ImGuiDataType_U32, &value,
                        &std::get<unsigned int>(prop.m_Min),
                        &std::get<unsigned int>(prop.m_Max));
                }
                else
                {
                    ImGui::InputScalar(prop.m_pName, ImGuiDataType_U32, &value);
                }

                if (readOnly) ImGui::EndDisabled();

                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);

                break;
            }
            case prop::VEC3:
            {
                glm::vec3 vec = {};
                prop.m_fnGet(pObject, &vec);
                if (ImGui::InputFloat3(prop.m_pName, glm::value_ptr(vec)))
                {
                    if (!readOnly)
                        prop.m_fnSet(pObject, &vec);
                }
                break;
            }
            case prop::DRAG_FLOAT:
            {
                float value = 0.0f;
                prop.m_fnGet(pObject, &value);

                float speed = 0.01f;
                float min = prop.m_HasRange ? prop.GetMinAsFloat() : 0.0f;
                float max = prop.m_HasRange ? prop.GetMaxAsFloat() : 100.0f;

                if (readOnly) ImGui::BeginDisabled();
                if (ImGui::DragFloat(prop.m_pName, &value, speed, min, max))
                {
                    if (!readOnly)
                        prop.m_fnSet(pObject, &value);
                }
                if (readOnly) ImGui::EndDisabled();
                break;
            }
            case prop::COLOR3:
            {
                glm::vec3 color = {};
                prop.m_fnGet(pObject, &color);

                if (readOnly) ImGui::BeginDisabled();
                if (ImGui::ColorEdit3(prop.m_pName, glm::value_ptr(color)))
                {
                    if (!readOnly)
                        prop.m_fnSet(pObject, &color);
                }
                if (readOnly) ImGui::EndDisabled();
                break;
            }
            case prop::SLIDER_FLOAT:
            {
                float value = 0.0f;
                prop.m_fnGet(pObject, &value);

                float min = prop.m_HasRange ? prop.GetMinAsFloat() : 0.0f;
                float max = prop.m_HasRange ? prop.GetMaxAsFloat() : 1.0f;

                if (readOnly) ImGui::BeginDisabled();
                if (ImGui::SliderFloat(prop.m_pName, &value, min, max))
                {
                    if (!readOnly)
                        prop.m_fnSet(pObject, &value);
                }
                if (readOnly) ImGui::EndDisabled();
                break;
            }
            default:
                ImGui::Text("Unsupported type: %d", prop.m_Type);
                break;
            }
            ImGui::PopID(); // pop prop.m_pName
            ImGui::PopID(); // pop pObject
            //printf("  Finished rendering property [%d]\n", propIndex);
            ImGui::Separator();
            ++propIndex;
        }
        //printf("DrawInspector completed.\n");
    }

    void DrawEntityInspector(Entity entity, Coordinator& ecs)
    {
        // Get all currently attached components
        auto attached = ecs.GetAllComponents(entity);

        for (const auto& [typeName, componentPtr] : attached)
        {
            const prop_list* propList = TypeRegistry::Instance().GetPropList(typeName);
            if (!propList || !propList->m_pName || propList->m_Props.empty())
                continue;

            if (ImGui::CollapsingHeader(propList->m_pName, ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (typeName == "TextureProperties")
                {
                    auto& tex = *static_cast<TextureProperties*>(componentPtr);
                    DrawAssetDropdown<TextureProperties>("Select Texture", tex);
                    DrawTexturePreview(tex);
                } 
                else if (typeName == "MeshProperties")
                {
                    auto& mesh = *static_cast<MeshProperties*>(componentPtr);
                    DrawAssetDropdown<MeshProperties>("Select Mesh", mesh);

                    if (ImGui::Button("Reload Mesh"))
                    {
                        MeshLoader::loadMesh(mesh.path, mesh);
                    }
                    DrawMaterialSelector(mesh.materialGuid);
                }
                else if (typeName == "AudioProperties")
                {
                    auto& audio = *static_cast<AudioProperties*>(componentPtr);
                    DrawAssetDropdown<AudioProperties>("Select Audio", audio);
                }
                else if (typeName == "NavMeshComponent")
                {
                    auto& navMesh = *static_cast<NavMeshComponent*>(componentPtr);

                    // Unique ID suffix using entity or typeName
                    std::string idSuffix = "##NavMesh_" + std::to_string(static_cast<uint32_t>(entity));

                    DrawAssetDropdown("NavMesh Source" + idSuffix, navMesh);

                    if (ImGui::Button(("Clear Runtime Buffers" + idSuffix).c_str()))
                    {
                        navMesh.positions.clear();
                        navMesh.indices.clear();
                    }

                    if (ImGui::Button(("Reset NavMesh Component" + idSuffix).c_str()))
                    {
                        navMesh = NavMeshComponent{};
                    }
                }
                else if (typeName == "AnimationProperties")
                {
                    auto& anim = *static_cast<AnimationProperties*>(componentPtr);
                    DrawAssetDropdown<AnimationProperties>("Select Animation", anim);
                }
                else if (typeName == "PhysicsProperties")
                {
                    auto& phys = *static_cast<PhysicsProperties*>(componentPtr);

                    DrawEnumDropdown("Motion Type", &phys.motionType, Framework::kMotionTypeNames);
                    DrawEnumDropdown("Category", &phys.category, Framework::kPhysicsCategoryNames);
                }
                else if (typeName == "LightProperties")
                {
                    auto& light = *static_cast<LightProperties*>(componentPtr);

                    const char* lightTypes[] = { "Directional", "Point", "Spot" };
                    int currentType = static_cast<int>(light.type);

                    if (ImGui::Combo("type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes)))
                    {
                        light.type = static_cast<LightType>(currentType);
                    }
                }
                else if (typeName == "NavMeshAgentComponent")
                {
                    auto& agent = *static_cast<NavMeshAgentComponent*>(componentPtr);
                    std::string idSuffix = "##Agent_" + std::to_string(static_cast<uint32_t>(entity));

                    if (ImGui::Button(("Reset Agent Data" + idSuffix).c_str()))
                    {
                        agent.currentPath.clear();
                        agent.currentWaypointIndex = 0;
                        agent.hasPath = false;
                        agent.isMoving = false;
                        agent.currentSpeed = 0.0f;
                    }

                    if (agent.hasPath)
                    {
                        ImGui::Text("Current Waypoint: %d / %d", agent.currentWaypointIndex + 1, static_cast<int>(agent.currentPath.size()));
                        if (ImGui::TreeNode("Path Preview"))
                        {
                            for (size_t i = 0; i < agent.currentPath.size(); ++i)
                            {
                                ImGui::BulletText("Waypoint %zu: (%.2f, %.2f, %.2f)", i,
                                    agent.currentPath[i].x, agent.currentPath[i].y, agent.currentPath[i].z);
                            }
                            ImGui::TreePop();
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("No active path.");
                    }
                }


                DrawInspector(componentPtr, *propList);

                if (ImGui::Button(std::string("Remove ").append(typeName).c_str()))
                {
                    TypeRegistry::Instance().RemoveComponent(typeName, ecs, entity);
                    break;
                }

            }
        }

        // Show "Add [Component]" buttons for missing ones
        for (global_registration* reg = global_registration::m_pHead; reg; reg = reg->m_pNext)
        {
            const prop_list* propList = reg->m_pPopList;
            if (!propList || !propList->m_pName || propList->m_Props.empty()) continue;
            if (attached.contains(propList->m_pName)) continue;

            if (propList->m_pName == "WindowConfig" ||
                propList->m_pName == "PrefabProperties" || propList->m_pName == "PrefabComponent" ||
                propList->m_pName == "MaterialMetadata" || propList->m_pName == "SceneProperties") continue;

            if (ImGui::Button(std::string("Add ").append(propList->m_pName).c_str()))
            {
                TypeRegistry::Instance().AddComponent(propList->m_pName, ecs, entity);
            }
        }      
    }

    void RegisterComponentTypes()
    {
        // Only if the component nids to be under the entity inspectorppppp
        TypeRegistry::Instance().Register<TextureProperties>("TextureProperties", &g_TexturePropertiesProps);
        TypeRegistry::Instance().Register<MeshProperties>("MeshProperties", &g_MeshProps);
        TypeRegistry::Instance().Register<AudioProperties>("AudioProperties", &g_AudioPropertiesProps);
        TypeRegistry::Instance().Register<TextProperties>("TextProperties", &g_TextComponentProps);
        TypeRegistry::Instance().Register<TransformComponent>("TransformComponent", &g_TransformProps);
        TypeRegistry::Instance().Register<NavMeshComponent>("NavMeshComponent", &g_NavMeshProps);
        TypeRegistry::Instance().Register<NavMeshAgentComponent>("NavMeshAgentComponent", &g_NavMeshAgentProps);
        TypeRegistry::Instance().Register<AnimationProperties>("AnimationProperties", &g_AnimationPropertiesProps);
        TypeRegistry::Instance().Register<LightProperties>("LightProperties", &g_LightProps);
        TypeRegistry::Instance().Register<PhysicsProperties>("PhysicsProperties", &g_PhysicsProps);
    }

    void AutoRegisterAllReflectedComponents(Coordinator& ecs)
    {
        for (global_registration* reg = global_registration::m_pHead; reg; reg = reg->m_pNext)
        {
            const prop_list* propList = reg->m_pPopList;
            if (!propList || !propList->m_pName)
                continue;

            const std::string& typeName = propList->m_pName;
            if (auto* getter = TypeRegistry::Instance().Get(typeName))
            {
                ecs.RegisterRawGetter(typeName, [getter, &ecs](Entity e) -> void*
                    {
                        return (*getter)(ecs, e);
                    });
            }
        }
    }
}
