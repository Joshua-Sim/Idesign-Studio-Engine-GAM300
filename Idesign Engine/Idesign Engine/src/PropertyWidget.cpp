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
            printf("Inspecting property [%d]: %s\n", propIndex, prop.m_pName);

            if (prop.m_Flags & prop::HIDDEN)
            {
                printf("  Skipped (HIDDEN)\n");
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
            printf("  Calling getter for type %d...\n", prop.m_Type);

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
                if (!readOnly && ImGui::IsItemEdited())
                {
                    char value = buffer[0];
                    prop.m_fnSet(pObject, &value);
                }
                break;
            }

            case prop::STRING:
            {
                char buffer[256] = {};
                prop.m_fnGet(pObject, buffer);

                if (readOnly) ImGui::BeginDisabled();
                ImGui::InputText("##string", buffer, sizeof(buffer));
                if (readOnly) ImGui::EndDisabled();

                if (!readOnly && ImGui::IsItemEdited())
                {
                    std::string value(buffer);
                    prop.m_fnSet(pObject, &value);
                }
                break;
            }

            case prop::UINT:
            {
                unsigned int value = 0;
                prop.m_fnGet(pObject, &value);

                if (readOnly) ImGui::BeginDisabled();

                if (prop.m_HasRange)
                {
                    ImGui::SliderScalar("##slider", ImGuiDataType_U32, &value,
                        &std::get<unsigned int>(prop.m_Min),
                        &std::get<unsigned int>(prop.m_Max));
                }
                else
                {
                    ImGui::InputScalar("##input", ImGuiDataType_U32, &value);
                }

                if (readOnly) ImGui::EndDisabled();

                if (!readOnly && ImGui::IsItemEdited())
                    prop.m_fnSet(pObject, &value);

                break;
            }

            case prop::ENUM:
            {
                int value = 0;
                prop.m_fnGet(pObject, &value);
                const char* currentLabel = "Unknown";

                for (size_t i = 0; i < prop.m_EnumCount; ++i)
                {
                    if (prop.m_pEnumEntries[i].value == value)
                    {
                        currentLabel = prop.m_pEnumEntries[i].name;
                        break;
                    }
                }

                if (ImGui::BeginCombo(prop.m_pName, currentLabel))
                {
                    for (size_t i = 0; i < prop.m_EnumCount; ++i)
                    {
                        bool selected = (value == prop.m_pEnumEntries[i].value);
                        if (ImGui::Selectable(prop.m_pEnumEntries[i].name, selected))
                        {
                            value = prop.m_pEnumEntries[i].value;
                            if (!readOnly)
                                prop.m_fnSet(pObject, &value);
                        }
                    }
                    ImGui::EndCombo();
                }
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

            case prop::STRING_DROPDOWN:
            {
                std::string currentValue;
                prop.m_fnGet(pObject, &currentValue);

                std::vector<std::string> options = prop.GetDropdownOptions();
                const char* preview = currentValue.c_str();

                if (ImGui::BeginCombo(prop.m_pName, preview))
                {
                    for (const auto& option : options)
                    {
                        bool selected = (option == currentValue);
                        if (ImGui::Selectable(option.c_str(), selected))
                        {
                            currentValue = option;
                            if (!readOnly)
                                prop.m_fnSet(pObject, &currentValue);
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                break;
            }

            default:
                ImGui::Text("Unsupported type: %d", prop.m_Type);
                break;
            }
            ImGui::PopID(); // pop prop.m_pName
            ImGui::PopID(); // pop pObject
            printf("  Finished rendering property [%d]\n", propIndex);
            ImGui::Separator();
            ++propIndex;
        }
        printf("DrawInspector completed.\n");
    }

    void DrawEntityInspector(Entity entity, Coordinator& ecs)
    {
        for (global_registration* reg = global_registration::m_pHead; reg; reg = reg->m_pNext)
        {
            const prop_list* propList = reg->m_pPopList;
            if (!propList || !propList->m_pName || propList->m_Props.empty())
                continue;

            // Try to get the component from ECS
            void* componentPtr = ecs.GetRawComponent(entity, propList->m_pName);
            if (!componentPtr)
                continue;

            // Draw collapsible section for this component
            if (ImGui::CollapsingHeader(propList->m_pName, ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawInspector(componentPtr, *propList);
            }
        }
    }

    void RegisterComponentTypes()
    {
        TypeRegistry::Instance().Register<TextureProperties>("TextureProperties");
        TypeRegistry::Instance().Register<MeshProperties>("MeshProperties");
        // Audio Component not registered with ECS yet, so cant use below function
        //TypeRegistry::Instance().Register<AudioProperties>("AudioProperties");
        TypeRegistry::Instance().Register<TransformComponent>("TransformComponent");
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