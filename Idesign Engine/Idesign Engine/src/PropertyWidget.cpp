
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

                    mesh.materialGuid = GetDefaultMaterial();

                    if (ImGui::Button("Reload Mesh"))
                    {
                        MeshLoader::loadMesh(mesh.path, mesh);
                    }
                    DrawMaterialSelector(mesh.materialGuid);
                }
                
                else if (typeName == "AudioComponent")
                {
                    auto& audio = *static_cast<AudioMetadata*>(componentPtr);
                    DrawAssetDropdown<AudioMetadata>("Select Audio", audio);
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

                else if (typeName == "ScriptProperties")
                {
                    auto& scripts = *static_cast<ScriptProperties*>(componentPtr);
                    DrawScriptPropertiesWidget(scripts, entity);

                    // We fully handle this component ourselves, so we DON'T call DrawInspector
                    if (ImGui::Button("Remove ScriptProperties"))
                    {
                        TypeRegistry::Instance().RemoveComponent(typeName, ecs, entity);
                    }
                    continue;   
                }
                // 1. Handle EVENT RECEIVER (Updated variable names)
                if (typeName == "EventReceiverComponent")
                {
                    auto& receiver = *static_cast<Framework::EventReceiverComponent*>(componentPtr);

                    // --- Input 1: Message to Listen For ---
                    char msgListenBuffer[256];
                    strncpy_s(msgListenBuffer, sizeof(msgListenBuffer), receiver.messageToListenFor.c_str(), _TRUNCATE);
                    if (ImGui::InputText("Listen For (Msg)", msgListenBuffer, sizeof(msgListenBuffer)))
                    {
                        receiver.messageToListenFor = msgListenBuffer;
                    }

                    // --- Input 2: Lua Function to Call ---
                    char luaFuncBuffer[256];
                    strncpy_s(luaFuncBuffer, sizeof(luaFuncBuffer), receiver.luaFunctionToCall.c_str(), _TRUNCATE);
                    // Note: Changed label to match new variable name
                    if (ImGui::InputText("Lua Function", luaFuncBuffer, sizeof(luaFuncBuffer)))
                    {
                        receiver.luaFunctionToCall = luaFuncBuffer;
                    }
                }
                // 2. Handle EVENT SENDER (New! Needs to handle strings now, not enums)
                else if (typeName == "EventSenderComponent")
                {
                    auto& sender = *static_cast<Framework::EventSenderComponent*>(componentPtr);

                    // --- Input 1: Trigger Name (e.g. "OnCollision") ---
                    char triggerBuffer[256];
                    strncpy_s(triggerBuffer, sizeof(triggerBuffer), sender.triggerName.c_str(), _TRUNCATE);
                    if (ImGui::InputText("Trigger On", triggerBuffer, sizeof(triggerBuffer)))
                    {
                        sender.triggerName = triggerBuffer;
                    }

                    // --- Input 2: Message to Send (e.g. "ShowPrompt") ---
                    char msgSendBuffer[256];
                    strncpy_s(msgSendBuffer, sizeof(msgSendBuffer), sender.messageName.c_str(), _TRUNCATE);
                    if (ImGui::InputText("Send Message", msgSendBuffer, sizeof(msgSendBuffer)))
                    {
                        sender.messageName = msgSendBuffer;
                    }
                }
                // 3. Handle SCRIPTS (Existing logic)
                else if (typeName == "ScriptProperties")
                {
                    auto& scripts = *static_cast<ScriptProperties*>(componentPtr);
                    DrawScriptPropertiesWidget(scripts, entity);

                    if (ImGui::Button("Remove ScriptProperties"))
                    {
                        TypeRegistry::Instance().RemoveComponent(typeName, ecs, entity);
                    }
                }
                // 4. Handle EVERYTHING ELSE (Default Inspector)
                else
                {
                    DrawInspector(componentPtr, *propList);
                }
                
                

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
            if (excludedComponents.count(propList->m_pName)) continue;

            if (ImGui::Button(std::string("Add ").append(propList->m_pName).c_str()))
            {
                TypeRegistry::Instance().AddComponent(propList->m_pName, ecs, entity);

                if (std::string(propList->m_pName) == "BoxColliderComponent")
                {
                    if (!ecs.HasComponent<MeshProperties>(entity))
                    {
                        std::cout << "yee";
                        MeshProperties meshComponent{};
                        meshComponent.debugShape = DebugShapeType::Box;
                        meshComponent.debugcolor = glm::vec3(0.0f, 1.0f, 0.0f); // Green
                        MeshLoader::loadMesh(meshComponent.path, meshComponent);

                        ecs.AddComponent<MeshProperties>(entity, meshComponent);

                        std::cout << "[Editor] Auto-added Debug Mesh for BoxCollider on Entity " << entity << std::endl;
                    }
                    else {
                        MeshProperties& meshComponent = ecs.GetComponent<MeshProperties>(entity);
                        meshComponent.debugShape = DebugShapeType::Box;
                        meshComponent.debugcolor = glm::vec3(0.0f, 1.0f, 0.0f); // Green
                        MeshLoader::loadMesh(meshComponent.path, meshComponent);
                    }
                }
            }
        }      
    }

    void RegisterComponentTypes()
    {
        // Only if the component nids to be under the entity inspector
        TypeRegistry::Instance().Register<TextureProperties>("TextureProperties", &g_TexturePropertiesProps);
        TypeRegistry::Instance().Register<MeshProperties>("MeshProperties", &g_MeshProps);
        TypeRegistry::Instance().Register<AudioComponent>("AudioComponent", &g_AudioComponentProps);
        TypeRegistry::Instance().Register<ListenerComponent>("ListenerComponent", &g_ListenerComponentProps);
        TypeRegistry::Instance().Register<TextProperties>("TextProperties", &g_TextComponentProps);
        TypeRegistry::Instance().Register<TransformComponent>("TransformComponent", &g_TransformProps);
        TypeRegistry::Instance().Register<NavMeshComponent>("NavMeshComponent", &g_NavMeshProps);
        TypeRegistry::Instance().Register<NavMeshAgentComponent>("NavMeshAgentComponent", &g_NavMeshAgentProps);
        TypeRegistry::Instance().Register<AnimationProperties>("AnimationProperties", &g_AnimationPropertiesProps);
        TypeRegistry::Instance().Register<LightProperties>("LightProperties", &g_LightProps);
        TypeRegistry::Instance().Register<PhysicsProperties>("PhysicsProperties", &g_PhysicsProps);
        TypeRegistry::Instance().Register<EventReceiverComponent>("EventReceiverComponent", &g_EventReceiverProps);
        TypeRegistry::Instance().Register<EventSenderComponent>("EventSenderComponent", &g_EventSenderProps);
        TypeRegistry::Instance().Register<ScriptProperties>("ScriptProperties", &g_ScriptPropertiesProps);
        TypeRegistry::Instance().Register<ScriptEntry>("ScriptEntry", &g_ScriptEntryProps);
        TypeRegistry::Instance().Register<BoxColliderComponent>("BoxColliderComponent", &g_BoxColliderProps);
    }

    void AutoRegisterAllReflectedComponents(Coordinator& ecs)
    {
        for (global_registration* reg = global_registration::m_pHead; reg; reg = reg->m_pNext)
        {
            const prop_list* propList = reg->m_pPopList;
            if (!propList || !propList->m_pName) continue;

            const std::string typeName = propList->m_pName;

            // 1. Get the pointer to the function in the registry
            if (auto* getterPtr = TypeRegistry::Instance().Get(typeName))
            {
                // 2. [FIX] Create a local COPY of the std::function.
                // This decouples it from the TypeRegistry map's memory address.
                TypeGetter getterCopy = *getterPtr;

                // 3. [FIX] Capture the COPY by value 'getterCopy'
                // We capture &ecs by reference because ecs outlives this lambda.
                ecs.RegisterRawGetter(typeName, [getterCopy, &ecs](Entity e) -> void*
                    {
                        // 4. Call the copied function directly. 
                        // This is safe because the function lives inside this lambda now.
                        return getterCopy(ecs, e);
                    });
            }
        }
    }
}