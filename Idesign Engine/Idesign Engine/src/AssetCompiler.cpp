
#include <pch.h>
#include <AssetCompiler.h>
#include "NavMeshProperties.h"
#include "MeshLoader.h"
#include "SceneManager.h"

extern Framework::Coordinator ecsInterface;
namespace Framework
{
    void loadScene(const std::string& scenePath)
    {
        // Parse JSON
        rapidjson::Document doc;
        {
            FILE* fp = fopen(scenePath.c_str(), "r");
            if (!fp)
            {
                std::cerr << "Failed to open scene file: " << scenePath << "\n";
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

        // ---- substr and remove .json and directory for scene maping in scene manager ----
        std::string cleanName = scenePath;
        size_t lastSlash = cleanName.find_last_of("/\\");
        if (lastSlash != std::string::npos)
            cleanName = cleanName.substr(lastSlash + 1);

        size_t lastDot = cleanName.find_last_of('.');
        if (lastDot != std::string::npos)
            cleanName = cleanName.substr(0, lastDot);

        const auto& entities = doc["Entities"].GetArray();

        // ----- substr remove end -----

        for (const auto& entityJson : entities)
        {
            Entity entity = ecsInterface.CreateEntity();

            // Reassign guid from serialized field
            if (entityJson.HasMember("guid") && entityJson["guid"].IsString())
            {
                std::string guid = entityJson["guid"].GetString();
                ecsInterface.SetEntityGUID(entity, guid); // Overwrites the auto-generated one
            }

            // Entity name
            if (entityJson.HasMember("name") && entityJson["name"].IsString())
            {
                std::string name = entityJson["name"].GetString();
                ecsInterface.SetEntityName(entity, name);
            }

            const auto& components = entityJson["components"];

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

                // Parent GUID
                if (hierarchyJson.HasMember("parent") && hierarchyJson["parent"].IsString())
                {
                    hierarchy.parentGUID = hierarchyJson["parent"].GetString();
                }

                // Children GUIDs
                if (hierarchyJson.HasMember("children") && hierarchyJson["children"].IsArray())
                {
                    for (const auto& child : hierarchyJson["children"].GetArray())
                    {
                        if (child.IsString())
                            hierarchy.childrenGUIDs.push_back(child.GetString());
                    }
                }

                if (hierarchyJson.HasMember("sortIndex") && hierarchyJson["sortIndex"].IsInt())
                {
                    hierarchy.sortIndex = hierarchyJson["sortIndex"].GetInt();
                }

                ecsInterface.AddComponent(entity, hierarchy);
            }

            if (components.HasMember("MeshReference"))
            {
                const auto& meshRef = components["MeshReference"];
                if (meshRef.HasMember("guid") && meshRef["guid"].IsString())
                {
                    std::string guid = meshRef["guid"].GetString();
                    MeshProperties* mesh = AssetManager::Instance().Get<MeshProperties>(guid);
                    if (mesh)
                    {
                        // Ensure it has valid mesh data (path may be missing if deserialized incorrectly)
                        if (!mesh->path.empty())
                        {
                            MeshLoader::loadMesh(mesh->path, *mesh); // Re-load VAO/VBO/EBO etc.
                        }
                        else
                        {
                            std::cerr << "Mesh found in AssetManager but has empty path!\n";
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
                    {
                        ecsInterface.AddComponent(entity, *tex);
                    }
                    else
                    {
                        std::cerr << "Missing TextureProperties for GUID: " << guid << "\n";
                    }
                }
            }
            
            //Navmesh
            if (components.HasMember("NavMeshComponent"))
            {
                const auto& navObj = components["NavMeshComponent"];
                NavMeshComponent nav;

                if (navObj.HasMember("sourceMeshGUID"))
                    nav.sourceMeshGUID = navObj["sourceMeshGUID"].GetString();
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

            sceneManager.RegisterEntityToScene(entity, cleanName);
        }
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

            // Store entity name directly
            std::string entityName = ecsInterface.GetEntityName(entity);
            entityObj.AddMember("name", rapidjson::Value(entityName.c_str(), allocator), allocator);

            // Store GUID directly
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

                // Parent GUID (can be empty string)
                hierarchyJson.AddMember(
                    "parent",
                    rapidjson::Value(hierarchy.parentGUID.c_str(), allocator),
                    allocator
                );

                // Children GUIDs
                rapidjson::Value childrenArray(rapidjson::kArrayType);
                for (const std::string& childGUID : hierarchy.childrenGUIDs)
                {
                    childrenArray.PushBack(rapidjson::Value(childGUID.c_str(), allocator), allocator);
                }
                hierarchyJson.AddMember("children", childrenArray, allocator);

                hierarchyJson.AddMember("sortIndex", hierarchy.sortIndex, allocator);

                componentsObj.AddMember("HierarchyComponent", hierarchyJson, allocator);
            }

            // MeshProperties — write only GUID
            if (ecsInterface.HasComponent<MeshProperties>(entity))
            {
                const auto& mesh = ecsInterface.GetComponent<MeshProperties>(entity);
                rapidjson::Value meshRef(rapidjson::kObjectType);
                meshRef.AddMember("guid", rapidjson::Value(mesh.guid.c_str(), allocator), allocator);
                componentsObj.AddMember("MeshReference", meshRef, allocator);
            }

            // TextureProperties  write only GUID
            if (ecsInterface.HasComponent<TextureProperties>(entity))
            {
                const auto& tex = ecsInterface.GetComponent<TextureProperties>(entity);
                rapidjson::Value texRef(rapidjson::kObjectType);
                texRef.AddMember("guid", rapidjson::Value(tex.guid.c_str(), allocator), allocator);
                componentsObj.AddMember("TextureComponent", texRef, allocator);
            }
            // Add more components here...

            // Navmesh Component
            if (ecsInterface.HasComponent<NavMeshComponent>(entity))
            {
                const auto& nav = ecsInterface.GetComponent<NavMeshComponent>(entity);
                rapidjson::Value navObj(rapidjson::kObjectType);

                navObj.AddMember("sourceMeshGUID", rapidjson::Value(nav.sourceMeshGUID.c_str(), allocator), allocator);
                navObj.AddMember("isNavMeshSource", nav.isNavMeshSource, allocator);
                navObj.AddMember("runtimeGenerated", nav.runtimeGenerated, allocator);
                navObj.AddMember("agentRadius", nav.agentRadius, allocator);
                navObj.AddMember("agentHeight", nav.agentHeight, allocator);
                navObj.AddMember("cellSize", nav.cellSize, allocator);
                navObj.AddMember("cellHeight", nav.cellHeight, allocator);
                navObj.AddMember("maxSlope", nav.maxSlope, allocator);

                componentsObj.AddMember("NavMeshComponent", navObj, allocator);
            }

            entityObj.AddMember("components", componentsObj, allocator);
            entitiesArray.PushBack(entityObj, allocator);
        }

        doc.AddMember("Entities", entitiesArray, allocator);

        // Write to file
        FILE* fp = fopen(outputPath.c_str(), "w");
        if (!fp)
        {
            // Log error or show popup
            std::cerr << "Failed to open file for writing, save cancelled" << std::endl;
            return; // Prevent crash
        }
        auto buffer = std::make_unique<char[]>(65536);
        rapidjson::FileWriteStream os(fp, buffer.get(), 65536);
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        doc.Accept(writer);
        fclose(fp);
        std::cout << "Scene saved to: " << outputPath << "\n";
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
            case prop::FLOAT:  if (v.IsNumber()) { float        temp = static_cast<float>(v.GetDouble());     p.m_fnSet(pObject, &temp); } break;
            case prop::DOUBLE: if (v.IsNumber()) { double       temp = v.GetDouble();                         p.m_fnSet(pObject, &temp); } break;
            case prop::LONG:   if (v.IsInt64())  { long         temp = static_cast<long>(v.GetInt64());       p.m_fnSet(pObject, &temp); } break;
            case prop::SHORT:  if (v.IsInt())    { short        temp = static_cast<short>(v.GetInt());        p.m_fnSet(pObject, &temp); } break;
            case prop::BOOL:   if (v.IsBool())   { bool         temp = v.GetBool();                           p.m_fnSet(pObject, &temp); } break;
            case prop::STRING: if (v.IsString()) { std::string  temp(v.GetString(), v.GetStringLength());     p.m_fnSet(pObject, &temp); } break;
            case prop::CHAR:   if (v.IsString() && v.GetStringLength() == 1) { char temp = v.GetString()[0];  p.m_fnSet(pObject, &temp); } break;
            case prop::UINT:   if (v.IsUint())   { unsigned int temp = v.GetUint();                           p.m_fnSet(pObject, &temp); } break;
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