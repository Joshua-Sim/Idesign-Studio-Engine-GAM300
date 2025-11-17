
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
                            if (!mesh->path.empty())
                                MeshLoader::loadMesh(mesh->path, *mesh);

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
                            ecsInterface.AddComponent(entity, *tex);
                        else
                            std::cerr << "Missing TextureProperties for GUID: " << guid << "\n";
                    }
                }

                if (components.HasMember("LightProperties"))
                {
                    LightProperties light;
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
                            ecsInterface.AddComponent(entity, *anim);
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

                // TextureProperties � write only GUID
                if (ecsInterface.HasComponent<TextureProperties>(entity))
                {
                    const auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);
                    rapidjson::Value texRef(rapidjson::kObjectType);
                    texRef.AddMember("guid", rapidjson::Value(tex.guid.id.c_str(), allocator), allocator);
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
        //    (I'm assuming your ecs interface has functions like this)
        std::string sourceName = ecs.GetEntityName(sourceEntity);
        ecs.SetEntityName(newEntity, sourceName + " (Copy)");

        std::cout << "Duplicating entity " << sourceEntity << " (" << sourceName
            << ") to new entity " << newEntity << "..." << std::endl;

        // 3. Go through every component type, check if the source has it,
        //    and add a copy of it to the new entity.

        // --- (Copy logic for each component) --- 

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

        if (ecs.HasComponent<MeshProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<MeshProperties>(sourceEntity));
        }

        if (ecs.HasComponent<MaterialProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<MaterialProperties>(sourceEntity));
        }

        if (ecs.HasComponent<TextureProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<TextureProperties>(sourceEntity));
        }

        if (ecs.HasComponent<NavMeshComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<NavMeshComponent>(sourceEntity));
        }

        if (ecs.HasComponent<LightProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<LightProperties>(sourceEntity));
        }

        if (ecs.HasComponent<AnimationProperties>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<AnimationProperties>(sourceEntity));
        }

        if (ecs.HasComponent<NavMeshAgentComponent>(sourceEntity))
        {
            ecs.AddComponent(newEntity, ecs.GetComponent<NavMeshAgentComponent>(sourceEntity));
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
                Framework::ScriptEntry newInst;
                newInst.scriptPath = inst.scriptPath;
                newInst.hotReload = inst.hotReload;   // copy other fields if needed
                copy.scripts.push_back(newInst);
            }

            copy.scriptCount = static_cast<int>(copy.scripts.size());

            ecs.AddComponent<ScriptProperties>(newEntity, copy);
        }

        // !! IMPORTANT !!
        // Add any other components your engine uses here!
        // (e.g., Rigidbody, Collider, ScriptComponent, etc.)

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
        // 1. Generate GUID for new prefab
        std::string prefabGUID = GenerateGUID();

        // 2. BUILD ABSOLUTE PATHS
        std::filesystem::path projectRoot = Framework::ScriptingSystem::GetProjectRoot();
        std::filesystem::path prefabFolder = projectRoot / "Assets/Prefabs";
        std::filesystem::path prefabPath = prefabFolder / (prefabName + ".prefab");

        // 3. Ensure directory exists (using absolute path)
        std::error_code ec;
        std::filesystem::create_directories(prefabFolder, ec);
        if (ec)
        {
            std::cerr << "Failed to create prefab directory: " << ec.message() << "\n";
            return;
        }

        // 4. Serialize entity to prefab file (use absolute path)
        std::string absolutePrefabPath = prefabPath.string();
        saveEntityToPrefab(entity, absolutePrefabPath);

        // 5. Create PrefabProperties entry with RELATIVE path for storage
        std::string relativePath = std::filesystem::relative(absolutePrefabPath, projectRoot).string();

        auto prefabProps = std::make_unique<PrefabProperties>();
        prefabProps->guid = prefabGUID;
        prefabProps->name = prefabName;
        prefabProps->path = relativePath; // Store relative path
        prefabProps->isScenePrefab = false;

        // 6. Register in AssetManager (use relative path as key)
        AssetManager::Instance().RegisterAsset(
            std::move(prefabProps),
            &g_PrefabProps,
            relativePath
        );

        // 7. Save to JSON (use absolute path for JSON file)
        std::string jsonPath = (projectRoot / "Assets/JSON/PrefabAsset.json").string();
        AssetManager::Instance().SaveArrayToJson(
            TypeName<PrefabProperties>::value,
            jsonPath,
            "prefabs"
        );

        std::cout << "Created prefab: " << prefabName << " (GUID: " << prefabGUID << ")\n";
        std::cout << "  Saved to: " << absolutePrefabPath << "\n";
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
        else
        {
            // Assume project root is stored somewhere accessible
            prefabPath = std::filesystem::path(Framework::ScriptingSystem::GetProjectRoot()) / prefabProps->path;
        }

        std::error_code ec;
        auto canonicalPath = std::filesystem::weakly_canonical(prefabPath, ec);

        if (ec || !std::filesystem::exists(canonicalPath))
        {
            std::cerr << "Prefab file not found: " << prefabProps->path
                << " (resolved to: " << prefabPath << ")" << std::endl;
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
                if (tex) ecsInterface.AddComponent(newEntity, *tex);
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

        if (entity == std::numeric_limits<Entity>::max())
            return entity;

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

    bool MoveFileToAssetFolder(const std::string& sourcePath, const std::string& destinationFolder, std::string& outFinalPath, std::string& outError)
    {
        try 
        {
            // Ensure destination folder exists
            if (!fs::exists(destinationFolder))
                fs::create_directories(destinationFolder);

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
                    glm::vec3 temp{
                        v[0].GetFloat(),
                        v[1].GetFloat(),
                        v[2].GetFloat()
                    };
                    p.m_fnSet(pObject, &temp);
                }
                else
                {
                    std::cerr << "Invalid vec3 format for property: " << p.m_pName << std::endl;
                }
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