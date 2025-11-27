
#include <pch.h>
#include <AssetCompiler.h>
#include "NavMeshProperties.h"
#include "NavMeshAgentProperties.h"
#include "MeshLoader.h"
#include "SceneManager.h"
#include "PrefabProperties.h"
#include "PrefabComponent.h"
#include "ScriptProperties.h"
#include "ScriptingSystem.h"
#include <ItemComponent.h>

static const std::map<std::string, LightType> s_stringToLightType = {
    { "Directional", LightType::Directional },
    { "Point",       LightType::Point },
    { "Spot",        LightType::Spot }
};

static const std::map<LightType, std::string> s_lightTypeToString = {
    { LightType::Directional, "Directional" },
    { LightType::Point,       "Point" },
    { LightType::Spot,        "Spot" }
};

extern Framework::Coordinator ecsInterface;
namespace Framework
{
    void loadScene(const std::string& scenePath)
    {
        auto originalCwd = std::filesystem::current_path();

        std::cout << std::filesystem::current_path() << "path before loading " << std::endl;

        // BUILD ABSOLUTE PATH FIRST
        std::filesystem::path absoluteScenePath;
        if (std::filesystem::path(scenePath).is_absolute())
        {
            absoluteScenePath = scenePath;
        }
        else
        {
            absoluteScenePath = std::filesystem::path(ScriptingSystem::GetProjectRoot()) / scenePath;
        }

        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(absoluteScenePath, ec);
        if (ec || !std::filesystem::exists(canonical))
        {
            std::cerr << "Failed to find scene file: " << scenePath
                << " (resolved to: " << absoluteScenePath << ")\n";
            return;
        }

        std::string finalPath = canonical.string();

        // Parse JSON
        rapidjson::Document doc;
        {
            FILE* fp = fopen(finalPath.c_str(), "r");  // <-- USE ABSOLUTE PATH
            if (!fp)
            {
                std::cerr << "Failed to open scene file: " << finalPath << "\n";
                return;
            }

            std::unique_ptr<char[]> buffer = std::make_unique<char[]>(65536);
            rapidjson::FileReadStream is(fp, buffer.get(), 65536);
            doc.ParseStream(is);
            fclose(fp);
        }

        if (!doc.IsObject() || !doc.HasMember("Entities") || !doc["Entities"].IsArray())
        {
            std::cerr << "Invalid scene format: missing Entities array\n";
            return;
        }

        // Extract scene name
        std::string cleanName = scenePath;
        size_t lastSlash = cleanName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            cleanName = cleanName.substr(lastSlash + 1);
        size_t lastDot = cleanName.find_last_of('.');
        if (lastDot != std::string::npos)
            cleanName = cleanName.substr(0, lastDot);

        const auto& entities = doc["Entities"].GetArray();

        for (const auto& entityJson : entities)
        {
            const auto& components = entityJson["components"];

            // === CHECK IF THIS IS A PREFAB INSTANCE ===
            if (components.HasMember("PrefabComponent"))
            {
                // This is a prefab instance - instantiate from prefab
                PrefabComponent prefabComp;
                Deserialize(&prefabComp, g_PrefabComponentProps, components["PrefabComponent"]);

                // Instantiate from prefab
                Entity entity = InstantiatePrefab(prefabComp.prefabGUID.id);

                if (entity == std::numeric_limits<Entity>::max())
                {
                    std::cerr << "Failed to instantiate prefab: " << prefabComp.prefabGUID.id << "\n";
                    continue;
                }

                // Override entity GUID with saved one (maintain scene references)
                if (entityJson.HasMember("guid") && entityJson["guid"].IsString())
                {
                    std::string guid = entityJson["guid"].GetString();
                    ecsInterface.SetEntityGUID(entity, guid);
                }

                // Override entity name if saved
                if (entityJson.HasMember("name") && entityJson["name"].IsString())
                {
                    std::string name = entityJson["name"].GetString();
                    ecsInterface.SetEntityName(entity, name);
                }

                // Budget version: Done!
                // Delta version: Would apply overrides here from prefabComp

                sceneManager.RegisterEntityToScene(entity, cleanName);
            }
            else
            {
                // === REGULAR ENTITY - Load normally ===
                Entity entity = ecsInterface.CreateEntity();

                // Reassign guid from serialized field
                if (entityJson.HasMember("guid") && entityJson["guid"].IsString())
                {
                    std::string guid = entityJson["guid"].GetString();
                    ecsInterface.SetEntityGUID(entity, guid);
                }

                // Entity name
                if (entityJson.HasMember("name") && entityJson["name"].IsString())
                {
                    std::string name = entityJson["name"].GetString();
                    ecsInterface.SetEntityName(entity, name);
                }

                // Transform
                if (components.HasMember("TransformComponent"))
                {
                    TransformComponent transform;
                    Deserialize(&transform, g_TransformProps, components["TransformComponent"]);
                    ecsInterface.AddComponent(entity, transform);
                }

                // Tag 
                if (components.HasMember("TagComponent"))
                {
                    const auto& tagJson = components["TagComponent"];
                    TagComponent tagComp;
                    if (tagJson.HasMember("tags") && tagJson["tags"].IsArray())
                    {
                        for (const auto& tag : tagJson["tags"].GetArray())
                        {
                            if (tag.IsString())
                                tagComp.tags.push_back(tag.GetString());
                        }
                    }
                    ecsInterface.AddComponent(entity, tagComp);
                }
                else
                {
                    // Legacy support for pre-tag objects
                    TagComponent tagComp;
                    ecsInterface.AddComponent(entity, tagComp);
                }


                // Hierarchy
                if (components.HasMember("HierarchyComponent"))
                {
                    HierarchyComponent hierarchy;
                    const auto& hierarchyJson = components["HierarchyComponent"];

                    if (hierarchyJson.HasMember("parent") && hierarchyJson["parent"].IsString())
                        hierarchy.parentGUID = hierarchyJson["parent"].GetString();

                    if (hierarchyJson.HasMember("children") && hierarchyJson["children"].IsArray())
                    {
                        for (const auto& child : hierarchyJson["children"].GetArray())
                        {
                            if (child.IsString())
                                hierarchy.childrenGUIDs.push_back(child.GetString());
                        }
                    }

                    if (hierarchyJson.HasMember("sortIndex") && hierarchyJson["sortIndex"].IsInt())
                        hierarchy.sortIndex = hierarchyJson["sortIndex"].GetInt();

                    ecsInterface.AddComponent(entity, hierarchy);
                }

                // Mesh
                if (components.HasMember("MeshComponent"))
                {
                    const auto& meshRef = components["MeshComponent"];
                    if (meshRef.HasMember("guid") && meshRef["guid"].IsString())
                    {
                        std::string guid = meshRef["guid"].GetString();
                        MeshProperties* mesh = AssetManager::Instance().Get<MeshProperties>(guid);
                        if (mesh)
                        {
                            // Load mesh data if path is valid
                            auto animGUID = AssetManager::Instance().FindGuid<AnimationProperties>(mesh->name);
                            Framework::AnimationProperties* animAsset = nullptr;

                            if (!animGUID.empty()) {
                                animAsset = AssetManager::Instance().Get<AnimationProperties>(animGUID);
                            }

                            // 2. If found, load as Animated. Else, load as Static.
                            if (animAsset)
                            {
                                // Ensure skeleton is loaded so we can map bone IDs
                                if (animAsset->skeleton.bones.empty()) {
                                    AnimationLoader::loadAnimation(animAsset->path, *animAsset);
                                }

                                // Load with Weights & IDs
                                MeshLoader::loadAnimatedMesh(mesh->path, *animAsset, *mesh);
                            }
                            else
                            {
                                // Fallback to static
                                MeshLoader::loadMesh(mesh->path, *mesh);
                            }
                            // Restore material linkage if present
                            if (meshRef.HasMember("materialGuid") && meshRef["materialGuid"].IsString())
                            {
                                std::string matGuid = meshRef["materialGuid"].GetString();
                                mesh->materialGuid = AssetHandle(matGuid);
                            }

                            ecsInterface.AddComponent(entity, *mesh);
                        }
                        else
                        {
                            std::cerr << "Missing MeshProperties for GUID: " << guid << "\n";
                            Framework::MeshProperties defaultMesh;
                            ecsInterface.AddComponent(entity, defaultMesh);
                        }
                    }
                }
                
                // Texture
                if (components.HasMember("TextureComponent"))
                {
                    const auto& texRef = components["TextureComponent"];
                    if (texRef.HasMember("guid") && texRef["guid"].IsString())
                    {
                        std::string guid = texRef["guid"].GetString();
                        const TextureProperties* tex = AssetManager::Instance().Get<TextureProperties>(guid);
                        if (tex)
                        {
                            // 1. Create a local copy of the asset data
                            TextureProperties instanceTex = *tex;

                            // 2. [FIX] Check if the JSON has the isUI override and apply it
                            if (texRef.HasMember("isUI") && texRef["isUI"].IsBool())
                            {
                                instanceTex.isUI = texRef["isUI"].GetBool();
                            }

                            // 3. Add the modified component
                            ecsInterface.AddComponent(entity, instanceTex);
                        }
                        else
                        {
                            std::cerr << "Missing TextureProperties for GUID: " << guid << "\n";
                        }
                    }
                }

                if (components.HasMember("LightProperties"))
                {
                    LightProperties light;
                    const auto& lightJson = components["LightProperties"];

                    // Manually read the 'type' string and convert it
                    if (lightJson.HasMember("type") && lightJson["type"].IsString())
                    {
                        std::string typeStr = lightJson["type"].GetString();
                        if (s_stringToLightType.count(typeStr))
                        {
                            light.type = s_stringToLightType.at(typeStr);
                        }
                    }
                    Deserialize(&light, g_LightProps, components["LightProperties"]);
                    ecsInterface.AddComponent(entity, light);
                }

                if (components.HasMember("AnimationProperties"))
                {
                    const auto& animRef = components["AnimationProperties"];
                    if (animRef.HasMember("guid") && animRef["guid"].IsString())
                    {
                        std::string guid = animRef["guid"].GetString();
                        AnimationProperties* anim = AssetManager::Instance().Get<AnimationProperties>(guid);
                        if (anim)
                        {
                            AnimationProperties instanceAnim = *anim;
                            if (animRef.HasMember("defaultAnimationName") && animRef["defaultAnimationName"].IsString())
                                instanceAnim.defaultAnimationName = animRef["defaultAnimationName"].GetString();

                            if (animRef.HasMember("animationSpeed") && animRef["animationSpeed"].IsNumber())
                                instanceAnim.animationSpeed = animRef["animationSpeed"].GetFloat();

                            if (animRef.HasMember("bLoops") && animRef["bLoops"].IsBool())
                                instanceAnim.bLoops = animRef["bLoops"].GetBool();

                            if (animRef.HasMember("bPlayOnStart") && animRef["bPlayOnStart"].IsBool())
                                instanceAnim.bPlayOnStart = animRef["bPlayOnStart"].GetBool();
                            ecsInterface.AddComponent(entity, instanceAnim);
                        }
                        else
                        {
                            std::cerr << "Missing AnimationProperties for GUID: " << guid << "\n";
                        }
                    }
                }


                // Navmesh
                if (components.HasMember("NavMeshComponent"))
                {
                    const auto& navObj = components["NavMeshComponent"];
                    NavMeshComponent nav;

                    if (navObj.HasMember("guid"))
                        nav.guid = navObj["guid"].GetString();
                    if (navObj.HasMember("isNavMeshSource"))
                        nav.isNavMeshSource = navObj["isNavMeshSource"].GetBool();
                    if (navObj.HasMember("runtimeGenerated"))
                        nav.runtimeGenerated = navObj["runtimeGenerated"].GetBool();
                    if (navObj.HasMember("agentRadius"))
                        nav.agentRadius = navObj["agentRadius"].GetFloat();
                    if (navObj.HasMember("agentHeight"))
                        nav.agentHeight = navObj["agentHeight"].GetFloat();
                    if (navObj.HasMember("cellSize"))
                        nav.cellSize = navObj["cellSize"].GetFloat();
                    if (navObj.HasMember("cellHeight"))
                        nav.cellHeight = navObj["cellHeight"].GetFloat();
                    if (navObj.HasMember("maxSlope"))
                        nav.maxSlope = navObj["maxSlope"].GetFloat();

                    ecsInterface.AddComponent(entity, nav);
                }

                // NavMeshAgentComponent
                if (components.HasMember("NavMeshAgentComponent"))
                {
                    NavMeshAgentComponent agent;
                    Deserialize(&agent, g_NavMeshAgentProps, components["NavMeshAgentComponent"]);
                    ecsInterface.AddComponent(entity, agent);
                }

                // Scripting
                if (components.HasMember("ScriptProperties"))
                {
                    const auto& scriptObj = components["ScriptProperties"];
                    ScriptProperties sp;

                    if (scriptObj.HasMember("scripts") && scriptObj["scripts"].IsArray())
                    {
                        const auto& arr = scriptObj["scripts"].GetArray();
                        for (const auto& s : arr)
                        {
                            ScriptEntry entry;

                            if (s.HasMember("guid") && s["guid"].IsString())
                                entry.guid = AssetHandle{ s["guid"].GetString() };

                            if (s.HasMember("scriptPath") && s["scriptPath"].IsString())
                                entry.scriptPath = s["scriptPath"].GetString();

                            if (s.HasMember("hotReload") && s["hotReload"].IsBool())
                                entry.hotReload = s["hotReload"].GetBool();

                            sp.scripts.push_back(entry);
                        }
                    }

                    if (!sp.scripts.empty())
                    {
                        ecsInterface.AddComponent(entity, sp);
                    }
                }
                if (components.HasMember("BoxColliderComponent")) {
                    BoxColliderComponent comp;
                    auto& json = components["BoxColliderComponent"];

                    if (json.HasMember("colliders") && json["colliders"].IsArray()) {
                        for (const auto& val : json["colliders"].GetArray()) {
                            BoxColliderEntry entry;

                            // Load Size
                            if (val.HasMember("size")) {
                                auto arr = val["size"].GetArray();
                                entry.size = { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
                            }
                            // Load Center
                            if (val.HasMember("center")) {
                                auto arr = val["center"].GetArray();
                                entry.center = { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
                            }
                            if (val.HasMember("canPassThrough")) {
                                entry.canPassThrough = val["canPassThrough"].GetBool();
                            }
                            if (val.HasMember("triggerMessage")) entry.triggerMessage = val["triggerMessage"].GetString();
                            if (val.HasMember("isInteractable")) {
                                entry.isInteractable = val["isInteractable"].GetBool();
                            }
                            if (val.HasMember("interactMessage") && val["interactMessage"].IsString()) {
                                entry.interactMessage = val["interactMessage"].GetString();
                            }

                            comp.colliders.push_back(entry);
                        }
                    }
                    ecsInterface.AddComponent(entity, comp);
                }
                // PhysicsProperties
                if (components.HasMember("PhysicsProperties"))
                {
                    Framework::PhysicsProperties phys;
                    const auto& physJson = components["PhysicsProperties"];

                    // 1. Load 'isImmovable' and all other float properties
                    Deserialize(&phys, g_PhysicsProps, physJson);

                    if (phys.isImmovable)
                    {
                        phys.motionType = 0; // Set to Static
                    }

                    ecsInterface.AddComponent(entity, phys);
                }
                // --- Event Sender ---
                if (components.HasMember("EventSenderComponent"))
                {
                    const auto& jsonVal = components["EventSenderComponent"];
                    EventSenderComponent senderComp;

                    // Check if the "senders" array exists
                    if (jsonVal.HasMember("senders") && jsonVal["senders"].IsArray())
                    {
                        const auto& senderArray = jsonVal["senders"];

                        for (rapidjson::SizeType i = 0; i < senderArray.Size(); i++)
                        {
                            const auto& entryObj = senderArray[i];
                            EventSenderEntry newEntry;

                            // Extract Trigger Name
                            if (entryObj.HasMember("triggerName") && entryObj["triggerName"].IsString()) {
                                newEntry.triggerName = entryObj["triggerName"].GetString();
                            }

                            // Extract Message Name
                            if (entryObj.HasMember("messageName") && entryObj["messageName"].IsString()) {
                                newEntry.messageName = entryObj["messageName"].GetString();
                            }

                            // Add to the component's vector
                            senderComp.senders.push_back(newEntry);
                        }
                    }

                    ecsInterface.AddComponent(entity, senderComp);
                }

                // --- Event Receiver ---
                if (components.HasMember("EventReceiverComponent"))
                {
                    const auto& jsonVal = components["EventReceiverComponent"];
                    EventReceiverComponent receiverComp;

                    // Check if the "receivers" array exists
                    if (jsonVal.HasMember("receivers") && jsonVal["receivers"].IsArray())
                    {
                        const auto& receiverArray = jsonVal["receivers"];

                        for (rapidjson::SizeType i = 0; i < receiverArray.Size(); i++)
                        {
                            const auto& entryObj = receiverArray[i];
                            EventReceiverEntry newEntry;

                            // Extract Message to Listen For
                            if (entryObj.HasMember("messageToListenFor") && entryObj["messageToListenFor"].IsString()) {
                                newEntry.messageToListenFor = entryObj["messageToListenFor"].GetString();
                            }

                            // Extract Lua Function
                            if (entryObj.HasMember("luaFunctionToCall") && entryObj["luaFunctionToCall"].IsString()) {
                                newEntry.luaFunctionToCall = entryObj["luaFunctionToCall"].GetString();
                            }

                            // IMPORTANT: Explicitly ensure isSubscribed is false on load
                            // The EventSystem::Update loop will see this is false and automatically subscribe it.
                            newEntry.isSubscribed = false;

                            // Add to the component's vector
                            receiverComp.receivers.push_back(newEntry);
                        }
                    }

                    ecsInterface.AddComponent(entity, receiverComp);
                }

                // --- ListenerComponent ---
                if (components.HasMember("ListenerComponent"))
                {
                    const auto& jsonVal = components["ListenerComponent"];
                    ListenerComponent listenerComp;

                    // Deserialize all registered properties
                    Deserialize(&listenerComp, g_ListenerComponentProps, jsonVal);

                    // Add to ECS
                    ecsInterface.AddComponent(entity, listenerComp);
                }

                // --- AudioComponent ---
                if (components.HasMember("AudioComponent"))
                {
                    const auto& jsonVal = components["AudioComponent"];
                    AudioComponent audioComp;

                    // Deserialize all registered properties
                    Deserialize(&audioComp, g_AudioComponentProps, jsonVal);

                    // Runtime-only fields should be reset
                    audioComp.eventInstance = nullptr;   // FMOD instance is rebuilt at runtime
                    audioComp.isPlaying = false;     // don’t persist playback state
                    audioComp.channelID = -1;        // reset channel handle
                    audioComp.lastPosition = glm::vec3(0.0f); // will be updated when playing

                    // Add to ECS
                    ecsInterface.AddComponent(entity, audioComp);
                }

                // --- ButtonComponent ---
                if (components.HasMember("ButtonComponent"))
                {
                    const auto& jsonVal = components["ButtonComponent"];
                    ButtonComponent buttonComp;

                    // Deserialize all registered properties
                    Deserialize(&buttonComp, g_ButtonComponentProps, jsonVal);

                    // Add to ECS
                    ecsInterface.AddComponent(entity, buttonComp);
                }
                
                // --- Item Component ---
                if (components.HasMember("ItemComponent"))
                {
                    const auto& jsonVal = components["ItemComponent"];
                    ItemComponent item;

                    // 1. Name
                    if (jsonVal.HasMember("name") && jsonVal["name"].IsString())
                    {
                        item.name = jsonVal["name"].GetString();
                    }

                    // 2. Is Ingredient
                    if (jsonVal.HasMember("isIngredient") && jsonVal["isIngredient"].IsBool())
                    {
                        item.isIngredient = jsonVal["isIngredient"].GetBool();
                    }

                    // 3. Is Choppable
                    if (jsonVal.HasMember("isChoppable") && jsonVal["isChoppable"].IsBool())
                    {
                        item.isChoppable = jsonVal["isChoppable"].GetBool();
                    }

                    ecsInterface.AddComponent(entity, item);
                }

                sceneManager.RegisterEntityToScene(entity, cleanName);
            }
        }
        std::filesystem::current_path(originalCwd);
        std::cout << std::filesystem::current_path() << std::endl;
    }

    void saveScene(const std::string& outputPath)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        rapidjson::Value entitiesArray(rapidjson::kArrayType);

        for (Entity entity : ecsInterface.GetEntities())
        {
            rapidjson::Value entityObj(rapidjson::kObjectType);

            // Store entity name
            std::string entityName = ecsInterface.GetEntityName(entity);
            entityObj.AddMember("name", rapidjson::Value(entityName.c_str(), allocator), allocator);

            // Store GUID
            std::string guid = ecsInterface.GetEntityGUID(entity);
            entityObj.AddMember("guid", rapidjson::Value(guid.c_str(), allocator), allocator);

            rapidjson::Value componentsObj(rapidjson::kObjectType);

            // === PREFAB INSTANCE CHECK ===
            // If entity is a prefab instance, ONLY save the PrefabComponent
            if (ecsInterface.HasComponent<PrefabComponent>(entity))
            {
                const auto& prefabComp = ecsInterface.GetComponent<PrefabComponent>(entity);
                rapidjson::Value prefabJson(rapidjson::kObjectType);
                Serialize((void*)&prefabComp, g_PrefabComponentProps, prefabJson, allocator);
                componentsObj.AddMember("PrefabComponent", prefabJson, allocator);

                // Budget version: Skip all other components (they come from prefab)
                // Delta version: Would also save overridden properties here
            }
            else
            {
                // === REGULAR ENTITY - Save all components ===

                // TransformComponent
                if (ecsInterface.HasComponent<TransformComponent>(entity))
                {
                    const auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
                    rapidjson::Value transformJson(rapidjson::kObjectType);
                    Serialize((void*)&transform, g_TransformProps, transformJson, allocator);
                    componentsObj.AddMember("TransformComponent", transformJson, allocator);
                }

                // HierarchyComponent
                if (ecsInterface.HasComponent<HierarchyComponent>(entity))
                {
                    const auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
                    rapidjson::Value hierarchyJson(rapidjson::kObjectType);

                    hierarchyJson.AddMember("parent", rapidjson::Value(hierarchy.parentGUID.c_str(), allocator), allocator);

                    rapidjson::Value childrenArray(rapidjson::kArrayType);
                    for (const std::string& childGUID : hierarchy.childrenGUIDs)
                    {
                        childrenArray.PushBack(rapidjson::Value(childGUID.c_str(), allocator), allocator);
                    }
                    hierarchyJson.AddMember("children", childrenArray, allocator);
                    hierarchyJson.AddMember("sortIndex", hierarchy.sortIndex, allocator);

                    componentsObj.AddMember("HierarchyComponent", hierarchyJson, allocator);
                }

                // TagComponent
                if (ecsInterface.HasComponent<TagComponent>(entity))
                {
                    const auto& tagComp = ecsInterface.GetComponent<TagComponent>(entity);
                    rapidjson::Value tagJson(rapidjson::kObjectType);

                    rapidjson::Value tagsArray(rapidjson::kArrayType);
                    for (const std::string& tag : tagComp.tags)
                    {
                        tagsArray.PushBack(rapidjson::Value(tag.c_str(), allocator), allocator);
                    }

                    tagJson.AddMember("tags", tagsArray, allocator);
                    componentsObj.AddMember("TagComponent", tagJson, allocator);
                }

                // MeshProperties � write only GUID
                if (ecsInterface.HasComponent<MeshProperties>(entity))
                {
                    const auto& mesh = ecsInterface.GetComponent<MeshProperties>(entity);
                    rapidjson::Value meshRef(rapidjson::kObjectType);
                    meshRef.AddMember("guid", rapidjson::Value(mesh.guid.id.c_str(), allocator), allocator);

                    // Linked Material GUID (if valid)
                    if (!mesh.materialGuid.empty())
                    {
                        meshRef.AddMember("materialGuid", rapidjson::Value(mesh.materialGuid.id.c_str(), allocator), allocator);
                    }
                    componentsObj.AddMember("MeshComponent", meshRef, allocator);
                }

                if (ecsInterface.HasComponent<AnimationProperties>(entity))
                {
                    const auto& anim = ecsInterface.GetComponent<AnimationProperties>(entity);
                    rapidjson::Value animRef(rapidjson::kObjectType);
                    animRef.AddMember("guid", rapidjson::Value(anim.guid.id.c_str(), allocator), allocator);
                    animRef.AddMember("defaultAnimationName", rapidjson::Value(anim.defaultAnimationName.c_str(), allocator), allocator);
                    animRef.AddMember("animationSpeed", anim.animationSpeed, allocator);
                    animRef.AddMember("bLoops", anim.bLoops, allocator);
                    animRef.AddMember("bPlayOnStart", anim.bPlayOnStart, allocator);
                    componentsObj.AddMember("AnimationProperties", animRef, allocator);
                }

                // TextureProperties � write only GUID
                if (ecsInterface.HasComponent<TextureProperties>(entity))
                {
                    const auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);
                    rapidjson::Value texRef(rapidjson::kObjectType);
                    texRef.AddMember("guid", rapidjson::Value(tex.guid.id.c_str(), allocator), allocator);
                    texRef.AddMember("isUI", tex.isUI, allocator);
                    componentsObj.AddMember("TextureComponent", texRef, allocator);
                }

                // NavMesh Component
                if (ecsInterface.HasComponent<NavMeshComponent>(entity))
                {
                    const auto& nav = ecsInterface.GetComponent<NavMeshComponent>(entity);
                    rapidjson::Value navObj(rapidjson::kObjectType);

                    navObj.AddMember("guid", rapidjson::Value(nav.guid.c_str(), allocator), allocator);
                    navObj.AddMember("isNavMeshSource", nav.isNavMeshSource, allocator);
                    navObj.AddMember("runtimeGenerated", nav.runtimeGenerated, allocator);
                    navObj.AddMember("agentRadius", nav.agentRadius, allocator);
                    navObj.AddMember("agentHeight", nav.agentHeight, allocator);
                    navObj.AddMember("cellSize", nav.cellSize, allocator);
                    navObj.AddMember("cellHeight", nav.cellHeight, allocator);
                    navObj.AddMember("maxSlope", nav.maxSlope, allocator);

                    componentsObj.AddMember("NavMeshComponent", navObj, allocator);
                }
                // NavMeshAgentComponent
                if (ecsInterface.HasComponent<NavMeshAgentComponent>(entity))
                {
                    const auto& agent = ecsInterface.GetComponent<NavMeshAgentComponent>(entity);
                    rapidjson::Value agentJson(rapidjson::kObjectType);
                    Serialize((void*)&agent, g_NavMeshAgentProps, agentJson, allocator);
                    componentsObj.AddMember("NavMeshAgentComponent", agentJson, allocator);
                }

                // Scripts            
                if (ecsInterface.HasComponent<ScriptProperties>(entity))
                {
                    const auto& sp = ecsInterface.GetComponent<ScriptProperties>(entity);

                    rapidjson::Value scriptObj(rapidjson::kObjectType);
                    rapidjson::Value scriptsArr(rapidjson::kArrayType);

                    for (const auto& entry : sp.scripts)
                    {
                        rapidjson::Value entryObj(rapidjson::kObjectType);

                        entryObj.AddMember("guid", rapidjson::Value(entry.guid.id.c_str(), allocator), allocator);

                        entryObj.AddMember("scriptPath", rapidjson::Value(entry.scriptPath.c_str(), allocator), allocator);

                        entryObj.AddMember("hotReload", entry.hotReload, allocator);

                        // If later you add more saved fields (e.g. hotReload), add them here.

                        scriptsArr.PushBack(entryObj, allocator);
                    }

                    scriptObj.AddMember("scripts", scriptsArr, allocator);
                    componentsObj.AddMember("ScriptProperties", scriptObj, allocator);
                }
                if (ecsInterface.HasComponent<LightProperties>(entity))
                {
                    const auto& light = ecsInterface.GetComponent<LightProperties>(entity);
                    rapidjson::Value lightJson(rapidjson::kObjectType);

                    // 1. Run the generic serializer for color, intensity, etc.
                    Serialize((void*)&light, g_LightProps, lightJson, allocator);

                    // 2. Manually add the 'type' string to the SAME json object
                    if (s_lightTypeToString.count(light.type))
                    {
                        lightJson.AddMember("type", rapidjson::Value(s_lightTypeToString.at(light.type).c_str(), allocator), allocator);
                    }

                    // 3. Add the complete object to the component list
                    componentsObj.AddMember("LightProperties", lightJson, allocator);
                }
                if (ecsInterface.HasComponent<BoxColliderComponent>(entity)) {
                    const auto& comp = ecsInterface.GetComponent<BoxColliderComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);
                    rapidjson::Value arrayVal(rapidjson::kArrayType);

                    for (const auto& entry : comp.colliders) {
                        rapidjson::Value obj(rapidjson::kObjectType);

                        // Save Vec3 Size
                        rapidjson::Value sizeVal(rapidjson::kArrayType);
                        sizeVal.PushBack(entry.size.x, allocator).PushBack(entry.size.y, allocator).PushBack(entry.size.z, allocator);
                        obj.AddMember("size", sizeVal, allocator);

                        // Save Vec3 Center
                        rapidjson::Value centerVal(rapidjson::kArrayType);
                        centerVal.PushBack(entry.center.x, allocator).PushBack(entry.center.y, allocator).PushBack(entry.center.z, allocator);
                        obj.AddMember("center", centerVal, allocator);

                        obj.AddMember("canPassThrough", entry.canPassThrough, allocator);
                        if (entry.canPassThrough) {
                            rapidjson::Value val; val.SetString(entry.triggerMessage.c_str(), allocator);
                            obj.AddMember("triggerMessage", val, allocator);
                        }

                        obj.AddMember("isInteractable", entry.isInteractable, allocator);
                        if (entry.isInteractable) {
                            rapidjson::Value val; val.SetString(entry.interactMessage.c_str(), allocator);
                            obj.AddMember("interactMessage", val, allocator);
                        }

                        arrayVal.PushBack(obj, allocator);
                    }
                    compJson.AddMember("colliders", arrayVal, allocator);
                    componentsObj.AddMember("BoxColliderComponent", compJson, allocator);
                }
                // PhysicsProperties
                if (ecsInterface.HasComponent<PhysicsProperties>(entity))
                {
                    const auto& phys = ecsInterface.GetComponent<PhysicsProperties>(entity);
                    rapidjson::Value physJson(rapidjson::kObjectType);

                    // 1. Serialize standard properties (floats, friction, mass, etc.)
                    Serialize((void*)&phys, g_PhysicsProps, physJson, allocator);

                    // 3. DO NOT save 'motionType'. We rely purely on the boolean.

                    componentsObj.AddMember("PhysicsProperties", physJson, allocator);
                }
                // --- Event Sender ---
                if (ecsInterface.HasComponent<EventSenderComponent>(entity))
                {
                    const auto& senderComp = ecsInterface.GetComponent<EventSenderComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);

                    // Create the Array to hold the list of senders
                    rapidjson::Value senderArray(rapidjson::kArrayType);

                    for (const auto& entry : senderComp.senders)
                    {
                        rapidjson::Value entryObj(rapidjson::kObjectType);

                        // 1. Trigger Name
                        rapidjson::Value triggerVal;
                        triggerVal.SetString(entry.triggerName.c_str(), allocator);
                        entryObj.AddMember("triggerName", triggerVal, allocator);

                        // 2. Message Name
                        rapidjson::Value msgVal;
                        msgVal.SetString(entry.messageName.c_str(), allocator);
                        entryObj.AddMember("messageName", msgVal, allocator);

                        // Push into array
                        senderArray.PushBack(entryObj, allocator);
                    }

                    // Add the array to the component object
                    compJson.AddMember("senders", senderArray, allocator);
                    componentsObj.AddMember("EventSenderComponent", compJson, allocator);
                }

                // --- Event Receiver ---
                if (ecsInterface.HasComponent<EventReceiverComponent>(entity))
                {
                    const auto& receiverComp = ecsInterface.GetComponent<EventReceiverComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);

                    // Create the Array to hold the list of receivers
                    rapidjson::Value receiverArray(rapidjson::kArrayType);

                    for (const auto& entry : receiverComp.receivers)
                    {
                        rapidjson::Value entryObj(rapidjson::kObjectType);

                        // 1. Message to Listen For
                        rapidjson::Value msgVal;
                        msgVal.SetString(entry.messageToListenFor.c_str(), allocator);
                        entryObj.AddMember("messageToListenFor", msgVal, allocator);

                        // 2. Lua Function
                        rapidjson::Value funcVal;
                        funcVal.SetString(entry.luaFunctionToCall.c_str(), allocator);
                        entryObj.AddMember("luaFunctionToCall", funcVal, allocator);

                        // Note: We intentionally DO NOT save 'isSubscribed'

                        // Push into array
                        receiverArray.PushBack(entryObj, allocator);
                    }

                    // Add the array to the component object
                    compJson.AddMember("receivers", receiverArray, allocator);
                    componentsObj.AddMember("EventReceiverComponent", compJson, allocator);
                }

                // --- ListenerComponent ---
                if (ecsInterface.HasComponent<ListenerComponent>(entity))
                {
                    const auto& listener = ecsInterface.GetComponent<ListenerComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);

                    // Serialize all registered properties
                    Serialize((void*)&listener, g_ListenerComponentProps, compJson, allocator);

                    componentsObj.AddMember("ListenerComponent", compJson, allocator);
                }

                // --- AudioComponent ---
                if (ecsInterface.HasComponent<AudioComponent>(entity))
                {
                    const auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);

                    // Serialize all registered properties
                    Serialize((void*)&audioComp, g_AudioComponentProps, compJson, allocator);

                    componentsObj.AddMember("AudioComponent", compJson, allocator);
                }

                // --- ButtonComponent ---
                if (ecsInterface.HasComponent<ButtonComponent>(entity))
                {
                    const auto& buttonComp = ecsInterface.GetComponent<ButtonComponent>(entity);
                    rapidjson::Value compJson(rapidjson::kObjectType);

                    // Serialize all registered properties
                    Serialize((void*)&buttonComp, g_ButtonComponentProps, compJson, allocator);

                    componentsObj.AddMember("ButtonComponent", compJson, allocator);
                }

                // --- Item Component ---
                if (ecsInterface.HasComponent<ItemComponent>(entity))
                {
                    const auto& item = ecsInterface.GetComponent<ItemComponent>(entity);
                    rapidjson::Value itemJson(rapidjson::kObjectType);

                    // 1. Name
                    rapidjson::Value nameVal;
                    nameVal.SetString(item.name.c_str(), allocator);
                    itemJson.AddMember("name", nameVal, allocator);

                    // 2. Bools
                    itemJson.AddMember("isIngredient", item.isIngredient, allocator);
                    itemJson.AddMember("isChoppable", item.isChoppable, allocator);

                    componentsObj.AddMember("ItemComponent", itemJson, allocator);
                }
            }
            entityObj.AddMember("components", componentsObj, allocator);
            entitiesArray.PushBack(entityObj, allocator);
        }
        doc.AddMember("Entities", entitiesArray, allocator);

        // Write to file
        FILE* fp = fopen(outputPath.c_str(), "w");
        if (!fp)
        {
            std::cerr << "Failed to open file for writing, save cancelled" << std::endl;
            return;
        }
        auto buffer = std::make_unique<char[]>(65536);
        rapidjson::FileWriteStream os(fp, buffer.get(), 65536);
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        doc.Accept(writer);
        fclose(fp);
        std::cout << "Scene saved to: " << outputPath << "\n";
    }

    void DuplicateEntity(Framework::Coordinator& ecs, Entity sourceEntity)
    {
        // 1. Create a new entity. This automatically gives it a new, unique GUID.
        Entity newEntity = ecs.CreateEntity();

        // 2. Get the source entity's name and set the new name
        std::string sourceName = ecs.GetEntityName(sourceEntity);
        ecs.SetEntityName(newEntity, sourceName + " (Copy)");

        std::cout << "Duplicating entity " << sourceEntity << " (" << sourceName
            << ") to new entity " << newEntity << "..." << std::endl;

        // 3. Go through every component type, check if the source has it,
        //    and add a copy of it to the new entity.

        // === CORE & GRAPHICS ===

        if (ecs.HasComponent<TransformComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<TransformComponent>(sourceEntity));
        }

        if (ecs.HasComponent<HierarchyComponent>(sourceEntity))
        {
            auto hierarchy = ecs.GetComponent<HierarchyComponent>(sourceEntity);
            // A duplicated entity should not have the original's children.
            // It will share the same parent, becoming a new sibling.
            hierarchy.childrenGUIDs.clear();
            ecs.AddComponent(newEntity, hierarchy);
        }

        if (ecs.HasComponent<TagComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<TagComponent>(sourceEntity));
        }

        if (ecs.HasComponent<MeshProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<MeshProperties>(sourceEntity));
        }

        // (Kept this from your original code, assuming it is used even if not in saveScene snippet)
        if (ecs.HasComponent<MaterialProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<MaterialProperties>(sourceEntity));
        }

        if (ecs.HasComponent<TextureProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<TextureProperties>(sourceEntity));
        }

        if (ecs.HasComponent<LightProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<LightProperties>(sourceEntity));
        }

        if (ecs.HasComponent<AnimationProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<AnimationProperties>(sourceEntity));
        }

        // === AI & NAVIGATION ===

        if (ecs.HasComponent<NavMeshComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<NavMeshComponent>(sourceEntity));
        }

        if (ecs.HasComponent<NavMeshAgentComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<NavMeshAgentComponent>(sourceEntity));
        }

        // === PHYSICS ===

        // Added BoxColliderComponent
        if (ecs.HasComponent<BoxColliderComponent>(sourceEntity))
        {
            // Standard copy is sufficient as std::vector handles deep copy of collider entries
            ecs.AddComponent(newEntity, ecs.GetComponent<BoxColliderComponent>(sourceEntity));
        }

        // Added PhysicsProperties
        if (ecs.HasComponent<PhysicsProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<PhysicsProperties>(sourceEntity));
        }

        // === LOGIC & EVENTS ===

        // Added EventSenderComponent
        if (ecs.HasComponent<EventSenderComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<EventSenderComponent>(sourceEntity));
        }

        // Added EventReceiverComponent
        if (ecs.HasComponent<EventReceiverComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<EventReceiverComponent>(sourceEntity));
        }

        if (ecs.HasComponent<PrefabComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<PrefabComponent>(sourceEntity));
        }

        if (ecs.HasComponent<ScriptProperties>(sourceEntity))
        {
            const auto& original = ecs.GetComponent<ScriptProperties>(sourceEntity);
            ScriptProperties copy;

            for (const auto& inst : original.scripts)
            {
                ScriptEntry newInst;
                newInst.guid = inst.guid;
                newInst.scriptPath = inst.scriptPath;
                newInst.hotReload = inst.hotReload;
                copy.scripts.push_back(newInst);
            }

            copy.scriptCount = static_cast<int>(copy.scripts.size());
            ecs.AddComponent<ScriptProperties>(newEntity, copy);
        }

        // === AUDIO ===

        // Added ListenerComponent
        if (ecs.HasComponent<ListenerComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<ListenerComponent>(sourceEntity));
        }

        // Added AudioComponent
        if (ecs.HasComponent<AudioComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<AudioComponent>(sourceEntity));
        }

        // === UI ===

        // Added ButtonComponent
        if (ecs.HasComponent<ButtonComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<ButtonComponent>(sourceEntity));
        }

        // === GAMEPLAY ===

        // Added ItemComponent
        if (ecs.HasComponent<ItemComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<ItemComponent>(sourceEntity));
        }

        // --- [FIX #1: REGISTER THE NEW ENTITY WITH THE SCENEMANAGER] ---
        // We add it to the currently active scene, just like SpawnPrefab does.
        std::string currentScene = sceneManager.GetCurrentSceneName();
        if (!currentScene.empty())
        {
            sceneManager.RegisterEntityToScene(newEntity, currentScene);
            std::cout << "Registered new entity " << newEntity << " to scene: " << currentScene << std::endl;
        }

        // --- [FIX #2: UPDATE THE PARENT'S HIERARCHY] ---
        // Tell the parent entity that this new child exists.
        if (ecs.HasComponent<HierarchyComponent>(newEntity))
        {
            // Get the hierarchy component we just added to the new entity
            auto& newHierarchy = ecs.GetComponent<HierarchyComponent>(newEntity);

            // Check if it actually has a parent
            if (!newHierarchy.parentGUID.empty())
            {
                std::string newGuid = ecs.GetEntityGUID(newEntity);
                Entity parentEntity = ecs.GetEntityByGUID(newHierarchy.parentGUID);

                // Find the parent and add this new entity to its children list
                if (ecs.IsEntityValid(parentEntity) && ecs.HasComponent<HierarchyComponent>(parentEntity))
                {
                    auto& parentHierarchy = ecs.GetComponent<HierarchyComponent>(parentEntity);
                    parentHierarchy.childrenGUIDs.push_back(newGuid);
                    std::cout << "Added new entity " << newGuid << " to parent's child list (" << newHierarchy.parentGUID << ")" << std::endl;
                }
            }
        }

        std::cout << "Successfully duplicated entity." << std::endl;
    }

    void saveEntityToPrefab(Entity entity, const std::string& prefabPath)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        rapidjson::Value entityObj(rapidjson::kObjectType);

        // Store entity name
        std::string entityName = ecsInterface.GetEntityName(entity);
        entityObj.AddMember("name", rapidjson::Value(entityName.c_str(), allocator), allocator);

        // Store GUID
        std::string guid = ecsInterface.GetEntityGUID(entity);
        entityObj.AddMember("guid", rapidjson::Value(guid.c_str(), allocator), allocator);

        rapidjson::Value componentsObj(rapidjson::kObjectType);

        // TransformComponent
        if (ecsInterface.HasComponent<TransformComponent>(entity))
        {
            const auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            rapidjson::Value transformJson(rapidjson::kObjectType);
            Serialize((void*)&transform, g_TransformProps, transformJson, allocator);
            componentsObj.AddMember("TransformComponent", transformJson, allocator);
        }


        // TagComponent
        if (ecsInterface.HasComponent<TagComponent>(entity))
        {
            const auto& tagComp = ecsInterface.GetComponent<TagComponent>(entity);
            rapidjson::Value tagJson(rapidjson::kObjectType);

            rapidjson::Value tagsArray(rapidjson::kArrayType);
            for (const std::string& tag : tagComp.tags)
            {
                tagsArray.PushBack(rapidjson::Value(tag.c_str(), allocator), allocator);
            }

            tagJson.AddMember("tags", tagsArray, allocator);
            componentsObj.AddMember("TagComponent", tagJson, allocator);
        }

        // HierarchyComponent
        if (ecsInterface.HasComponent<HierarchyComponent>(entity))
        {
            const auto& hierarchy = ecsInterface.GetComponent<HierarchyComponent>(entity);
            rapidjson::Value hierarchyJson(rapidjson::kObjectType);

            hierarchyJson.AddMember("parent", rapidjson::Value(hierarchy.parentGUID.c_str(), allocator), allocator);

            rapidjson::Value childrenArray(rapidjson::kArrayType);
            for (const std::string& childGUID : hierarchy.childrenGUIDs)
            {
                childrenArray.PushBack(rapidjson::Value(childGUID.c_str(), allocator), allocator);
            }
            hierarchyJson.AddMember("children", childrenArray, allocator);
            hierarchyJson.AddMember("sortIndex", hierarchy.sortIndex, allocator);

            componentsObj.AddMember("HierarchyComponent", hierarchyJson, allocator);
        }

        // MeshProperties - write only GUID
        if (ecsInterface.HasComponent<MeshProperties>(entity))
        {
            const auto& mesh = ecsInterface.GetComponent<MeshProperties>(entity);
            rapidjson::Value meshRef(rapidjson::kObjectType);
            meshRef.AddMember("guid", rapidjson::Value(mesh.guid.id.c_str(), allocator), allocator);
            componentsObj.AddMember("MeshReference", meshRef, allocator);
        }

        // TextureProperties - write only GUID
        if (ecsInterface.HasComponent<TextureProperties>(entity))
        {
            const auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);
            rapidjson::Value texRef(rapidjson::kObjectType);
            texRef.AddMember("guid", rapidjson::Value(tex.guid.id.c_str(), allocator), allocator);
            texRef.AddMember("isUI", tex.isUI, allocator);
            componentsObj.AddMember("TextureComponent", texRef, allocator);
        }

        // NavMesh Component
        if (ecsInterface.HasComponent<NavMeshComponent>(entity))
        {
            const auto& nav = ecsInterface.GetComponent<NavMeshComponent>(entity);
            rapidjson::Value navObj(rapidjson::kObjectType);

            navObj.AddMember("guid", rapidjson::Value(nav.guid.c_str(), allocator), allocator);
            navObj.AddMember("isNavMeshSource", nav.isNavMeshSource, allocator);
            navObj.AddMember("runtimeGenerated", nav.runtimeGenerated, allocator);
            navObj.AddMember("agentRadius", nav.agentRadius, allocator);
            navObj.AddMember("agentHeight", nav.agentHeight, allocator);
            navObj.AddMember("cellSize", nav.cellSize, allocator);
            navObj.AddMember("cellHeight", nav.cellHeight, allocator);
            navObj.AddMember("maxSlope", nav.maxSlope, allocator);

            componentsObj.AddMember("NavMeshComponent", navObj, allocator);
        }

        // NavMeshAgentComponent
        if (ecsInterface.HasComponent<NavMeshAgentComponent>(entity))
        {
            const auto& agent = ecsInterface.GetComponent<NavMeshAgentComponent>(entity);
            rapidjson::Value agentJson(rapidjson::kObjectType);
            Serialize((void*)&agent, g_NavMeshAgentProps, agentJson, allocator);
            componentsObj.AddMember("NavMeshAgentComponent", agentJson, allocator);
        }


        if (ecsInterface.HasComponent<LightProperties>(entity))
        {
            const auto& light = ecsInterface.GetComponent<LightProperties>(entity);
            rapidjson::Value lightJson(rapidjson::kObjectType);
            Serialize((void*)&light, g_LightProps, lightJson, allocator);
            componentsObj.AddMember("LightProperties", lightJson, allocator);
        }

        if (ecsInterface.HasComponent<AnimationProperties>(entity))
        {
            const auto& anim = ecsInterface.GetComponent<AnimationProperties>(entity);
            rapidjson::Value animRef(rapidjson::kObjectType);
            animRef.AddMember("guid", rapidjson::Value(anim.guid.id.c_str(), allocator), allocator);
            componentsObj.AddMember("AnimationProperties", animRef, allocator);
        }
        
        //Scripts
        if (ecsInterface.HasComponent<ScriptProperties>(entity))
        {
            const auto& script = ecsInterface.GetComponent<ScriptProperties>(entity);

            rapidjson::Value scriptJson(rapidjson::kObjectType);
            rapidjson::Value arr(rapidjson::kArrayType);

            for (const auto& sc : script.scripts)
            {
                rapidjson::Value path;
                path.SetString(sc.scriptPath.c_str(), allocator);
                arr.PushBack(path, allocator);
            }

            scriptJson.AddMember("scripts", arr, allocator);

            // Use "ScriptProperties" as the JSON key to match your struct name
            componentsObj.AddMember("ScriptProperties", scriptJson, allocator);
        }

        // BoxColliderComponent
        if (ecsInterface.HasComponent<BoxColliderComponent>(entity)) {
            const auto& comp = ecsInterface.GetComponent<BoxColliderComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            rapidjson::Value arrayVal(rapidjson::kArrayType);

            for (const auto& entry : comp.colliders) {
                rapidjson::Value obj(rapidjson::kObjectType);

                // Save Vec3 Size
                rapidjson::Value sizeVal(rapidjson::kArrayType);
                sizeVal.PushBack(entry.size.x, allocator).PushBack(entry.size.y, allocator).PushBack(entry.size.z, allocator);
                obj.AddMember("size", sizeVal, allocator);

                // Save Vec3 Center
                rapidjson::Value centerVal(rapidjson::kArrayType);
                centerVal.PushBack(entry.center.x, allocator).PushBack(entry.center.y, allocator).PushBack(entry.center.z, allocator);
                obj.AddMember("center", centerVal, allocator);

                obj.AddMember("canPassThrough", entry.canPassThrough, allocator);
                if (entry.canPassThrough) {
                    rapidjson::Value val; val.SetString(entry.triggerMessage.c_str(), allocator);
                    obj.AddMember("triggerMessage", val, allocator);
                }

                obj.AddMember("isInteractable", entry.isInteractable, allocator);
                if (entry.isInteractable) {
                    rapidjson::Value val; val.SetString(entry.interactMessage.c_str(), allocator);
                    obj.AddMember("interactMessage", val, allocator);
                }

                arrayVal.PushBack(obj, allocator);
            }
            compJson.AddMember("colliders", arrayVal, allocator);
            componentsObj.AddMember("BoxColliderComponent", compJson, allocator);
        }

        // PhysicsProperties
        if (ecsInterface.HasComponent<PhysicsProperties>(entity))
        {
            const auto& phys = ecsInterface.GetComponent<PhysicsProperties>(entity);
            rapidjson::Value physJson(rapidjson::kObjectType);
            Serialize((void*)&phys, g_PhysicsProps, physJson, allocator);
            componentsObj.AddMember("PhysicsProperties", physJson, allocator);
        }

        // EventSenderComponent
        if (ecsInterface.HasComponent<EventSenderComponent>(entity))
        {
            const auto& senderComp = ecsInterface.GetComponent<EventSenderComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            rapidjson::Value senderArray(rapidjson::kArrayType);

            for (const auto& entry : senderComp.senders)
            {
                rapidjson::Value entryObj(rapidjson::kObjectType);
                rapidjson::Value triggerVal;
                triggerVal.SetString(entry.triggerName.c_str(), allocator);
                entryObj.AddMember("triggerName", triggerVal, allocator);

                rapidjson::Value msgVal;
                msgVal.SetString(entry.messageName.c_str(), allocator);
                entryObj.AddMember("messageName", msgVal, allocator);

                senderArray.PushBack(entryObj, allocator);
            }

            compJson.AddMember("senders", senderArray, allocator);
            componentsObj.AddMember("EventSenderComponent", compJson, allocator);
        }

        // EventReceiverComponent
        if (ecsInterface.HasComponent<EventReceiverComponent>(entity))
        {
            const auto& receiverComp = ecsInterface.GetComponent<EventReceiverComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            rapidjson::Value receiverArray(rapidjson::kArrayType);

            for (const auto& entry : receiverComp.receivers)
            {
                rapidjson::Value entryObj(rapidjson::kObjectType);
                rapidjson::Value msgVal;
                msgVal.SetString(entry.messageToListenFor.c_str(), allocator);
                entryObj.AddMember("messageToListenFor", msgVal, allocator);

                rapidjson::Value funcVal;
                funcVal.SetString(entry.luaFunctionToCall.c_str(), allocator);
                entryObj.AddMember("luaFunctionToCall", funcVal, allocator);

                receiverArray.PushBack(entryObj, allocator);
            }

            compJson.AddMember("receivers", receiverArray, allocator);
            componentsObj.AddMember("EventReceiverComponent", compJson, allocator);
        }

        // ListenerComponent
        if (ecsInterface.HasComponent<ListenerComponent>(entity))
        {
            const auto& listener = ecsInterface.GetComponent<ListenerComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            Serialize((void*)&listener, g_ListenerComponentProps, compJson, allocator);
            componentsObj.AddMember("ListenerComponent", compJson, allocator);
        }

        // AudioComponent
        if (ecsInterface.HasComponent<AudioComponent>(entity))
        {
            const auto& audioComp = ecsInterface.GetComponent<AudioComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            Serialize((void*)&audioComp, g_AudioComponentProps, compJson, allocator);
            componentsObj.AddMember("AudioComponent", compJson, allocator);
        }

        // ButtonComponent
        if (ecsInterface.HasComponent<ButtonComponent>(entity))
        {
            const auto& buttonComp = ecsInterface.GetComponent<ButtonComponent>(entity);
            rapidjson::Value compJson(rapidjson::kObjectType);
            Serialize((void*)&buttonComp, g_ButtonComponentProps, compJson, allocator);
            componentsObj.AddMember("ButtonComponent", compJson, allocator);
        }

        // ItemComponent
        if (ecsInterface.HasComponent<ItemComponent>(entity))
        {
            const auto& item = ecsInterface.GetComponent<ItemComponent>(entity);
            rapidjson::Value itemJson(rapidjson::kObjectType);

            rapidjson::Value nameVal;
            nameVal.SetString(item.name.c_str(), allocator);
            itemJson.AddMember("name", nameVal, allocator);

            itemJson.AddMember("isIngredient", item.isIngredient, allocator);
            itemJson.AddMember("isChoppable", item.isChoppable, allocator);

            componentsObj.AddMember("ItemComponent", itemJson, allocator);
        }

        entityObj.AddMember("components", componentsObj, allocator);
        doc.AddMember("entity", entityObj, allocator);  // Note: "entity" not "Entities"

        // BUILD ABSOLUTE PATH if needed
        std::filesystem::path absolutePath;
        if (std::filesystem::path(prefabPath).is_absolute())
        {
            absolutePath = prefabPath;
        }
        else
        {
            absolutePath = std::filesystem::path(Framework::ScriptingSystem::GetProjectRoot()) / prefabPath;
        }

        std::string finalPath = absolutePath.string();

        // Write to file using ABSOLUTE PATH
        FILE* fp = fopen(finalPath.c_str(), "w");
        if (!fp)
        {
            std::cerr << "Failed to create prefab file: " << finalPath << std::endl;
            return;
        }

        auto buffer = std::make_unique<char[]>(65536);
        rapidjson::FileWriteStream os(fp, buffer.get(), 65536);
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        writer.SetIndent(' ', 4);
        doc.Accept(writer);
        fclose(fp);

        std::cout << "Prefab saved to: " << finalPath << "\n";
    }

    void CreatePrefabFromEntity(Entity entity, const std::string& prefabName)
    {
        std::cout << "[CreatePrefab] START - Entity: " << entity << ", Name: " << prefabName << std::endl;

        // 2. BUILD ABSOLUTE PATHS
        std::filesystem::path projectRoot = Framework::ScriptingSystem::GetProjectRoot();
        std::filesystem::path prefabFolder = projectRoot / "Assets/Prefabs";
        std::filesystem::path prefabPath = prefabFolder / (prefabName + ".prefab");

        // === CHECK IF PREFAB ALREADY EXISTS ===
        std::string prefabGUID;
        bool isOverwriting = false;

        if (std::filesystem::exists(prefabPath))
        {
            std::cout << "[CreatePrefab] Prefab file already exists, checking for GUID..." << std::endl;

            // Try to find existing GUID
            std::string existingGUID = FindPrefabGUIDByName(prefabName);

            if (!existingGUID.empty())
            {
                // Reuse existing GUID
                prefabGUID = existingGUID;
                isOverwriting = true;
                std::cout << "[CreatePrefab] Overwriting existing prefab with GUID: " << prefabGUID << std::endl;
            }
            else
            {
                // File exists but not in registry - generate new GUID
                prefabGUID = GenerateGUID();
                std::cout << "[CreatePrefab] File exists but not registered, creating new with GUID: " << prefabGUID << std::endl;
            }
        }
        else
        {
            // New prefab - generate new GUID
            prefabGUID = GenerateGUID();
            std::cout << "[CreatePrefab] Creating new prefab with GUID: " << prefabGUID << std::endl;
        }

        // 3. Ensure directory exists (using absolute path)
        std::error_code ec;
        std::filesystem::create_directories(prefabFolder, ec);
        if (ec) {
            std::cerr << "[CreatePrefab] ERROR: Failed to create prefab directory: " << ec.message() << std::endl;
            return;
        }

        // 4. Serialize entity to prefab file (use absolute path)
        // This will overwrite if file exists
        std::string absolutePrefabPath = prefabPath.string();
        saveEntityToPrefab(entity, absolutePrefabPath);

        // 5. Create PrefabProperties entry with RELATIVE path for storage
        std::string relativePath = std::filesystem::relative(absolutePrefabPath, projectRoot).string();

        auto prefabProps = std::make_unique<PrefabProperties>();
        prefabProps->guid = prefabGUID;
        prefabProps->name = prefabName;
        prefabProps->path = relativePath;
        prefabProps->isScenePrefab = false;

        // 6. Register in AssetManager (this will update if already exists)
        AssetManager::Instance().RegisterAsset(std::move(prefabProps), &g_PrefabProps, relativePath);

        // 7. Save to JSON (use absolute path for JSON file)
        std::string jsonPath = (projectRoot / "Assets/JSON/PrefabAsset.json").string();
        AssetManager::Instance().SaveArrayToJson(TypeName<PrefabProperties>::value, jsonPath, "prefabs");

        if (isOverwriting)
        {
            std::cout << "[CreatePrefab] UPDATED existing prefab: " << prefabName << " (GUID: " << prefabGUID << ")" << std::endl;
        }
        else
        {
            std::cout << "[CreatePrefab] CREATED new prefab: " << prefabName << " (GUID: " << prefabGUID << ")" << std::endl;
        }
        std::cout << "[CreatePrefab] Saved to: " << absolutePrefabPath << std::endl;
    }

    Entity InstantiatePrefab(const std::string& prefabGUID)
    {
        // 1. Get prefab properties from asset manager
        auto* prefabProps = AssetManager::Instance().Get<PrefabProperties>(prefabGUID);
        if (!prefabProps)
        {
            std::cerr << "Prefab not found: " << prefabGUID << std::endl;
            return std::numeric_limits<Entity>::max();
        }

        // 2. BUILD ABSOLUTE PATH
        std::filesystem::path prefabPath;
        if (std::filesystem::path(prefabProps->path).is_absolute())
        {
            prefabPath = prefabProps->path;
        }
        // Assume project root is stored somewhere accessible
        else prefabPath = std::filesystem::path(Framework::ScriptingSystem::GetProjectRoot()) / prefabProps->path;

        std::error_code ec;
        auto canonicalPath = std::filesystem::weakly_canonical(prefabPath, ec);

        if (ec || !std::filesystem::exists(canonicalPath))
        {
            std::cerr << "Prefab file not found: " << prefabProps->path << " (resolved to: " << prefabPath << ")" << std::endl;
            return std::numeric_limits<Entity>::max();
        }

        std::string absolutePrefabPath = canonicalPath.string();

        // 3. Load the prefab file using ABSOLUTE PATH
        FILE* fp = fopen(absolutePrefabPath.c_str(), "r");
        if (!fp)
        {
            std::cerr << "Failed to open prefab file: " << absolutePrefabPath << std::endl;
            return std::numeric_limits<Entity>::max();
        }

        char buffer[65536];
        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (!doc.HasMember("entity"))
        {
            std::cerr << "Invalid prefab format\n";
            return std::numeric_limits<Entity>::max();
        }

        const auto& entityJson = doc["entity"];

        // 3. Create new entity
        Entity newEntity = ecsInterface.CreateEntity();

        // 4. Set name (from prefab)
        if (entityJson.HasMember("name") && entityJson["name"].IsString())
        {
            std::string name = entityJson["name"].GetString();
            ecsInterface.SetEntityName(newEntity, name);
        }

        // Note: DON'T use the prefab's GUID - generate new one (already done by CreateEntity)

        const auto& components = entityJson["components"];

        // 5. Load all components (same logic as loadScene)
        if (components.HasMember("TransformComponent"))
        {
            TransformComponent transform;
            Deserialize(&transform, g_TransformProps, components["TransformComponent"]);
            ecsInterface.AddComponent(newEntity, transform);
        }

        if (components.HasMember("HierarchyComponent"))
        {
            HierarchyComponent hierarchy;
            const auto& hierarchyJson = components["HierarchyComponent"];
            if (hierarchyJson.HasMember("parent") && hierarchyJson["parent"].IsString())
                hierarchy.parentGUID = hierarchyJson["parent"].GetString();
            if (hierarchyJson.HasMember("children") && hierarchyJson["children"].IsArray())
            {
                for (const auto& child : hierarchyJson["children"].GetArray())
                    if (child.IsString()) hierarchy.childrenGUIDs.push_back(child.GetString());
            }
            if (hierarchyJson.HasMember("sortIndex") && hierarchyJson["sortIndex"].IsInt())
                hierarchy.sortIndex = hierarchyJson["sortIndex"].GetInt();
            ecsInterface.AddComponent(newEntity, hierarchy);
        }


        // TagComponent
        if (components.HasMember("TagComponent"))
        {
            const auto& tagJson = components["TagComponent"];
            TagComponent tagComp;

            if (tagJson.HasMember("tags") && tagJson["tags"].IsArray())
            {
                for (const auto& tag : tagJson["tags"].GetArray())
                {
                    if (tag.IsString()) tagComp.tags.push_back(tag.GetString());
                }
            }
            ecsInterface.AddComponent(newEntity, tagComp);
        }

        // Mesh, Texture, NavMesh... (same as loadScene)
        if (components.HasMember("MeshReference"))
        {
            const auto& meshRef = components["MeshReference"];
            if (meshRef.HasMember("guid") && meshRef["guid"].IsString())
            {
                std::string guid = meshRef["guid"].GetString();
                MeshProperties* mesh = AssetManager::Instance().Get<MeshProperties>(guid);
                if (mesh) ecsInterface.AddComponent(newEntity, *mesh);
            }
        }

        if (components.HasMember("MaterialComponent"))
        {
            const auto& materialRef = components["MaterialComponent"];
            if (materialRef.HasMember("guid") && materialRef["guid"].IsString())
            {
                std::string guid = materialRef["guid"].GetString();
                MaterialProperties* mesh = AssetManager::Instance().Get<MaterialProperties>(guid);
                if (mesh) ecsInterface.AddComponent(newEntity, *mesh);
            }
        }

        if (components.HasMember("TextureComponent"))
        {
            const auto& texRef = components["TextureComponent"];
            if (texRef.HasMember("guid") && texRef["guid"].IsString())
            {
                std::string guid = texRef["guid"].GetString();
                const TextureProperties* tex = AssetManager::Instance().Get<TextureProperties>(guid);
                if (tex)
                {
                    // 1. Copy
                    TextureProperties instanceTex = *tex;

                    if (texRef.HasMember("isUI") && texRef["isUI"].IsBool())
                    {
                        instanceTex.isUI = texRef["isUI"].GetBool();
                    }
                    ecsInterface.AddComponent(newEntity, instanceTex);
                }
            }
        }

        if (components.HasMember("NavMeshComponent"))
        {
            const auto& navObj = components["NavMeshComponent"];
            NavMeshComponent nav;
            if (navObj.HasMember("guid")) nav.guid = navObj["guid"].GetString();
            if (navObj.HasMember("isNavMeshSource")) nav.isNavMeshSource = navObj["isNavMeshSource"].GetBool();
            if (navObj.HasMember("runtimeGenerated")) nav.runtimeGenerated = navObj["runtimeGenerated"].GetBool();
            if (navObj.HasMember("agentRadius")) nav.agentRadius = navObj["agentRadius"].GetFloat();
            if (navObj.HasMember("agentHeight")) nav.agentHeight = navObj["agentHeight"].GetFloat();
            if (navObj.HasMember("cellSize")) nav.cellSize = navObj["cellSize"].GetFloat();
            if (navObj.HasMember("cellHeight")) nav.cellHeight = navObj["cellHeight"].GetFloat();
            if (navObj.HasMember("maxSlope")) nav.maxSlope = navObj["maxSlope"].GetFloat();
            ecsInterface.AddComponent(newEntity, nav);
        }

        if (components.HasMember("NavMeshAgentComponent"))
        {
            NavMeshAgentComponent agent;
            Deserialize(&agent, g_NavMeshAgentProps, components["NavMeshAgentComponent"]);
            ecsInterface.AddComponent(newEntity, agent);
        }


        if (components.HasMember("LightProperties"))
        {
            LightProperties light;
            Deserialize(&light, g_LightProps, components["LightProperties"]);
            ecsInterface.AddComponent(newEntity, light);
        }

        if (components.HasMember("AnimationProperties"))
        {
            const auto& animRef = components["AnimationProperties"];
            if (animRef.HasMember("guid") && animRef["guid"].IsString())
            {
                std::string guid = animRef["guid"].GetString();
                AnimationProperties* anim = AssetManager::Instance().Get<AnimationProperties>(guid);
                if (anim) ecsInterface.AddComponent(newEntity, *anim);
            }
        }

        if (components.HasMember("ScriptProperties")) 
        {
            ScriptProperties script;
            const auto& scArr = components["ScriptProperties"]["scripts"];
            for (auto& s : scArr.GetArray()) 
            {
                ScriptEntry inst;
                inst.scriptPath = s.GetString();
                script.scripts.push_back(inst);
            }
            script.scriptCount = static_cast<int>(script.scripts.size());
            ecsInterface.AddComponent<ScriptProperties>(newEntity, script);
        }

        // BoxColliderComponent
        if (components.HasMember("BoxColliderComponent")) {
            BoxColliderComponent comp;
            auto& json = components["BoxColliderComponent"];

            if (json.HasMember("colliders") && json["colliders"].IsArray()) {
                for (const auto& val : json["colliders"].GetArray()) {
                    BoxColliderEntry entry;

                    if (val.HasMember("size")) {
                        auto arr = val["size"].GetArray();
                        entry.size = { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
                    }
                    if (val.HasMember("center")) {
                        auto arr = val["center"].GetArray();
                        entry.center = { arr[0].GetFloat(), arr[1].GetFloat(), arr[2].GetFloat() };
                    }
                    if (val.HasMember("canPassThrough")) {
                        entry.canPassThrough = val["canPassThrough"].GetBool();
                    }
                    if (val.HasMember("triggerMessage")) entry.triggerMessage = val["triggerMessage"].GetString();
                    if (val.HasMember("isInteractable")) {
                        entry.isInteractable = val["isInteractable"].GetBool();
                    }
                    if (val.HasMember("interactMessage") && val["interactMessage"].IsString()) {
                        entry.interactMessage = val["interactMessage"].GetString();
                    }

                    comp.colliders.push_back(entry);
                }
            }
            ecsInterface.AddComponent(newEntity, comp);
        }

        // PhysicsProperties
        if (components.HasMember("PhysicsProperties"))
        {
            Framework::PhysicsProperties phys;
            const auto& physJson = components["PhysicsProperties"];
            Deserialize(&phys, g_PhysicsProps, physJson);

            if (phys.isImmovable)
            {
                phys.motionType = 0; // Set to Static
            }

            ecsInterface.AddComponent(newEntity, phys);
        }

        // EventSenderComponent
        if (components.HasMember("EventSenderComponent"))
        {
            const auto& jsonVal = components["EventSenderComponent"];
            EventSenderComponent senderComp;

            if (jsonVal.HasMember("senders") && jsonVal["senders"].IsArray())
            {
                const auto& senderArray = jsonVal["senders"];
                for (rapidjson::SizeType i = 0; i < senderArray.Size(); i++)
                {
                    const auto& entryObj = senderArray[i];
                    EventSenderEntry newEntry;

                    if (entryObj.HasMember("triggerName") && entryObj["triggerName"].IsString()) {
                        newEntry.triggerName = entryObj["triggerName"].GetString();
                    }
                    if (entryObj.HasMember("messageName") && entryObj["messageName"].IsString()) {
                        newEntry.messageName = entryObj["messageName"].GetString();
                    }

                    senderComp.senders.push_back(newEntry);
                }
            }

            ecsInterface.AddComponent(newEntity, senderComp);
        }

        // EventReceiverComponent
        if (components.HasMember("EventReceiverComponent"))
        {
            const auto& jsonVal = components["EventReceiverComponent"];
            EventReceiverComponent receiverComp;

            if (jsonVal.HasMember("receivers") && jsonVal["receivers"].IsArray())
            {
                const auto& receiverArray = jsonVal["receivers"];
                for (rapidjson::SizeType i = 0; i < receiverArray.Size(); i++)
                {
                    const auto& entryObj = receiverArray[i];
                    EventReceiverEntry newEntry;

                    if (entryObj.HasMember("messageToListenFor") && entryObj["messageToListenFor"].IsString()) {
                        newEntry.messageToListenFor = entryObj["messageToListenFor"].GetString();
                    }
                    if (entryObj.HasMember("luaFunctionToCall") && entryObj["luaFunctionToCall"].IsString()) {
                        newEntry.luaFunctionToCall = entryObj["luaFunctionToCall"].GetString();
                    }

                    newEntry.isSubscribed = false;
                    receiverComp.receivers.push_back(newEntry);
                }
            }

            ecsInterface.AddComponent(newEntity, receiverComp);
        }

        // ListenerComponent
        if (components.HasMember("ListenerComponent"))
        {
            const auto& jsonVal = components["ListenerComponent"];
            ListenerComponent listenerComp;
            Deserialize(&listenerComp, g_ListenerComponentProps, jsonVal);
            ecsInterface.AddComponent(newEntity, listenerComp);
        }

        // AudioComponent
        if (components.HasMember("AudioComponent"))
        {
            const auto& jsonVal = components["AudioComponent"];
            AudioComponent audioComp;
            Deserialize(&audioComp, g_AudioComponentProps, jsonVal);

            // Reset runtime fields
            audioComp.eventInstance = nullptr;
            audioComp.isPlaying = false;
            audioComp.channelID = -1;
            audioComp.lastPosition = glm::vec3(0.0f);

            ecsInterface.AddComponent(newEntity, audioComp);
        }

        // ButtonComponent
        if (components.HasMember("ButtonComponent"))
        {
            const auto& jsonVal = components["ButtonComponent"];
            ButtonComponent buttonComp;
            Deserialize(&buttonComp, g_ButtonComponentProps, jsonVal);
            ecsInterface.AddComponent(newEntity, buttonComp);
        }

        // ItemComponent
        if (components.HasMember("ItemComponent"))
        {
            const auto& jsonVal = components["ItemComponent"];
            ItemComponent item;

            if (jsonVal.HasMember("name") && jsonVal["name"].IsString())
            {
                item.name = jsonVal["name"].GetString();
            }
            if (jsonVal.HasMember("isIngredient") && jsonVal["isIngredient"].IsBool())
            {
                item.isIngredient = jsonVal["isIngredient"].GetBool();
            }
            if (jsonVal.HasMember("isChoppable") && jsonVal["isChoppable"].IsBool())
            {
                item.isChoppable = jsonVal["isChoppable"].GetBool();
            }
            if (jsonVal.HasMember("toDestroyWhenPicked") && jsonVal["toDestroyWhenPicked"].IsBool())
            {
                item.toDestroyWhenPicked = jsonVal["toDestroyWhenPicked"].GetBool();
            }

            ecsInterface.AddComponent(newEntity, item);
        }

        if (newEntity != std::numeric_limits<Entity>::max())
        {
            PrefabComponent prefabComp;
            prefabComp.prefabGUID.id = prefabGUID;
            ecsInterface.AddComponent(newEntity, prefabComp);
        }

        std::cout << "Spawned prefab: " << prefabProps->name << " as entity " << newEntity << std::endl;
        return newEntity;
    }

    // Overload: Spawn with custom position only
    Entity InstantiatePrefab(const std::string& prefabGUID, const glm::vec3& position)
    {
        Entity entity = InstantiatePrefab(prefabGUID);

        if (entity == std::numeric_limits<Entity>::max()) return entity;

        // Override position only
        if (ecsInterface.HasComponent<TransformComponent>(entity))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            transform.position = position;
        }
        return entity;
    }

    // Overload: Spawn with full transform override
    Entity InstantiatePrefab(const std::string& prefabGUID, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
    {
        Entity entity = InstantiatePrefab(prefabGUID);

        if (entity == std::numeric_limits<Entity>::max())
            return entity;

        // Override full transform
        if (ecsInterface.HasComponent<TransformComponent>(entity))
        {
            auto& transform = ecsInterface.GetComponent<TransformComponent>(entity);
            transform.position = position;
            transform.rotation = rotation;
            transform.scale = scale;
        }
        return entity;
    }

    // Find prefab GUID by name
    std::string FindPrefabGUIDByName(const std::string& prefabName)
    {
        // Get all prefabs from AssetManager
        // This depends on your AssetManager implementation
        // You might need to add a GetAll<T>() method to AssetManager

        // For now, let's iterate through what we can access
        // If your AssetManager has a way to iterate, use that
        // Otherwise, you'll need to add this capability

        std::cout << "[FindPrefab] Looking for prefab: " << prefabName << std::endl;

        // Placeholder - you'll need to implement based on your AssetManager
        // This is a simple approach if you don't have iteration:
        // Try to load from the JSON directly

        std::filesystem::path jsonPath = std::filesystem::path(ScriptingSystem::GetProjectRoot()) / "Assets/JSON/PrefabAsset.json";

        FILE* fp = fopen(jsonPath.string().c_str(), "r");
        if (!fp)
        {
            std::cerr << "[FindPrefab] Could not open PrefabAsset.json\n";
            return "";
        }

        char buffer[65536];
        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (!doc.HasMember("prefabs") || !doc["prefabs"].IsArray())
        {
            std::cerr << "[FindPrefab] Invalid JSON format\n";
            return "";
        }

        const auto& prefabs = doc["prefabs"].GetArray();
        for (const auto& prefab : prefabs)
        {
            if (prefab.HasMember("name") && prefab["name"].IsString())
            {
                std::string name = prefab["name"].GetString();
                if (name == prefabName)
                {
                    if (prefab.HasMember("guid") && prefab["guid"].IsString())
                    {
                        std::string guid = prefab["guid"].GetString();
                        std::cout << "[FindPrefab] Found: " << prefabName << " -> " << guid << std::endl;
                        return guid;
                    }
                }
            }
        }

        std::cerr << "[FindPrefab] Prefab not found: " << prefabName << std::endl;
        return "";
    }

    // Overload that takes name instead of GUID
    Entity InstantiatePrefabByName(const std::string& prefabName)
    {
        std::string guid = FindPrefabGUIDByName(prefabName);
        if (guid.empty())
        {
            std::cerr << "Failed to find prefab: " << prefabName << std::endl;
            return std::numeric_limits<Entity>::max();
        }

        return InstantiatePrefab(guid);
    }

    Entity InstantiatePrefabByName(const std::string& prefabName, const glm::vec3& position)
    {
        std::string guid = FindPrefabGUIDByName(prefabName);
        if (guid.empty()) return std::numeric_limits<Entity>::max();

        return InstantiatePrefab(guid, position);
    }

    Entity InstantiatePrefabByName(const std::string& prefabName, const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
    {
        std::string guid = FindPrefabGUIDByName(prefabName);
        if (guid.empty()) return std::numeric_limits<Entity>::max();

        return InstantiatePrefab(guid, position, rotation, scale);
    }


    bool MoveFileToAssetFolder(const std::string& sourcePath, const std::string& destinationFolder, std::string& outFinalPath, std::string& outError)
    {
        try 
        {
            // Ensure destination folder exists
            if (!fs::exists(destinationFolder)) fs::create_directories(destinationFolder);

            // Extract filename
            std::string fileName = sourcePath.substr(sourcePath.find_last_of("/\\") + 1);
            std::string destinationPath = destinationFolder + "/" + fileName;

            // Check if file already exists
            if (fs::exists(destinationPath))
            {
                outError = "File already exists at destination: " + destinationPath;
                return false;
            }

            // Copy file
            fs::copy_file(sourcePath, destinationPath, fs::copy_options::overwrite_existing);
            outFinalPath = destinationPath;
            return true;
        }
        catch (const fs::filesystem_error& e) 
        {
            outError = e.what();
            return false;
        }
    }

    void Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj)
	{
        assert(pObject != nullptr);
        assert(obj.IsObject());

        for (const prop& p : PropList.m_Props)
        {
            if (!obj.HasMember(p.m_pName)) continue;
            const rapidjson::Value& v = obj[p.m_pName];
            switch (p.m_Type)
            {
            case prop::INT:    if (v.IsInt())    { int          temp = v.GetInt();                            p.m_fnSet(pObject, &temp); } break;
            case prop::DRAG_FLOAT:
            case prop::SLIDER_FLOAT:
            case prop::FLOAT:  if (v.IsNumber()) { float        temp = static_cast<float>(v.GetDouble());     p.m_fnSet(pObject, &temp); } break;
            case prop::DOUBLE: if (v.IsNumber()) { double       temp = v.GetDouble();                         p.m_fnSet(pObject, &temp); } break;
            case prop::LONG:   if (v.IsInt64())  { long         temp = static_cast<long>(v.GetInt64());       p.m_fnSet(pObject, &temp); } break;
            case prop::SHORT:  if (v.IsInt())    { short        temp = static_cast<short>(v.GetInt());        p.m_fnSet(pObject, &temp); } break;
            case prop::BOOL:   if (v.IsBool())   { bool         temp = v.GetBool();                           p.m_fnSet(pObject, &temp); } break;
            case prop::STRING: if (v.IsString()) { std::string  temp(v.GetString(), v.GetStringLength());     p.m_fnSet(pObject, &temp); } break;
            case prop::CHAR:   if (v.IsString() && v.GetStringLength() == 1) { char temp = v.GetString()[0];  p.m_fnSet(pObject, &temp); } break;
            case prop::UINT:   if (v.IsUint())   { unsigned int temp = v.GetUint();                           p.m_fnSet(pObject, &temp); } break;
            case prop::COLOR3:
            case prop::VEC3:
            {
                if (v.IsArray() && v.Size() == 3)
                {
                    glm::vec3 temp{v[0].GetFloat(), v[1].GetFloat(), v[2].GetFloat()};
                    p.m_fnSet(pObject, &temp);
                }
                else std::cerr << "Invalid vec3 format for property: " << p.m_pName << std::endl;
                break;
            }
            case prop::VEC2:
            {
                if (v.IsArray() && v.Size() == 2)
                {
                    glm::vec2 temp{ v[0].GetFloat(), v[1].GetFloat() };
                    p.m_fnSet(pObject, &temp);
                }
                else std::cerr << "Invalid vec2 format for property: " << p.m_pName << std::endl;
                break;
            }
            default:
                std::cerr << "Unsupported or mismatched type for property: " << p.m_pName << std::endl;
                break;
            }
        }
	}

	void Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator)
	{
        assert(pObject != nullptr);
        assert(outObj.IsObject());

        for (const prop& p : PropList.m_Props)
        {
            rapidjson::Value key;
            key.SetString(p.m_pName, allocator);
            switch (p.m_Type)
            {
            case prop::INT:    { int          v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::DRAG_FLOAT:
            case prop::SLIDER_FLOAT:
            case prop::FLOAT:  { float        v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::DOUBLE: { double       v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::LONG:   { long         v; p.m_fnGet(pObject, &v); outObj.AddMember(key, static_cast<int64_t>(v), allocator); break; }
            case prop::SHORT:  { short        v; p.m_fnGet(pObject, &v); outObj.AddMember(key, static_cast<int>(v), allocator); break; }
            case prop::BOOL:   { bool         v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::UINT:   { unsigned int v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::CHAR:
            {
                char v;
                p.m_fnGet(pObject, &v);
                rapidjson::Value jsonValue;
                jsonValue.SetString(&v, 1, allocator); // Single char as string
                outObj.AddMember(key, jsonValue, allocator);
                break;
            }
            case prop::STRING:
            {
                std::string v;
                p.m_fnGet(pObject, &v);
                rapidjson::Value jsonValue;
                jsonValue.SetString(v.c_str(), allocator);
                outObj.AddMember(key, jsonValue, allocator);
                break;
            }
            case prop::COLOR3:
            case prop::VEC3:
            {
                glm::vec3 value;
                p.m_fnGet(pObject, &value);
                rapidjson::Value vecArray(rapidjson::kArrayType);
                vecArray.PushBack(value.x, allocator);
                vecArray.PushBack(value.y, allocator);
                vecArray.PushBack(value.z, allocator);
                outObj.AddMember(key, vecArray, allocator);
                break;
            }
            case prop::VEC2:
            {
                glm::vec2 value;
                p.m_fnGet(pObject, &value);

                rapidjson::Value vecArray(rapidjson::kArrayType);
                vecArray.PushBack(value.x, allocator);
                vecArray.PushBack(value.y, allocator);

                outObj.AddMember(key, vecArray, allocator);
                break;
            }

            default:
                std::cerr << "Unsupported type during serialization: " << p.m_pName << std::endl;
                break;
            }
        }
	}

    void SetGuid(void* pObject, const prop_list& PropList)
    {
        for (const auto& p : PropList.m_Props)
        {
            if (std::string(p.m_pName) == "guid")
            {
                AssetHandle currentGuid{};
                p.m_fnGet(pObject, &currentGuid);

                if (currentGuid.id.empty())
                {
                    AssetHandle newGuid(GenerateGUID());
                    p.m_fnSet(pObject, &newGuid);
                }
                break;
            }
        }
    }
}