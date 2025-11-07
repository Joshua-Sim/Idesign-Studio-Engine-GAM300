
#include "pch.h"
#include "MaterialLoader.h"

namespace Framework
{
    std::unique_ptr<MaterialProperties> LoadMaterialAsset(const std::string& path, const AssetHandle& guid)
    {
        FILE* fp = fopen(path.c_str(), "r");
        if (!fp) return nullptr;

        std::unique_ptr<char[]> buffer = std::make_unique<char[]>(65536);
        rapidjson::FileReadStream is(fp, buffer.get(), 65536);
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (doc.HasParseError()) return nullptr;

        auto material = std::make_unique<MaterialProperties>();

        // Read GUID from file if present
        if (doc.HasMember("guid") && doc["guid"].IsString())
            material->guid = AssetHandle(doc["guid"].GetString());
        else
            material->guid = guid; // fallback if file has no guid

        if (doc.HasMember("ambient") && doc["ambient"].IsArray())
            material->ambient = glm::vec3(doc["ambient"][0].GetFloat(), doc["ambient"][1].GetFloat(), doc["ambient"][2].GetFloat());

        if (doc.HasMember("albedo") && doc["albedo"].IsArray())
            material->albedo = glm::vec3(doc["albedo"][0].GetFloat(), doc["albedo"][1].GetFloat(), doc["albedo"][2].GetFloat());

        if (doc.HasMember("specular") && doc["specular"].IsArray())
            material->specular = glm::vec3(doc["specular"][0].GetFloat(), doc["specular"][1].GetFloat(), doc["specular"][2].GetFloat());

        if (doc.HasMember("shininess"))
            material->shininess = doc["shininess"].GetFloat();

        if (doc.HasMember("opacity"))
            material->opacity = doc["opacity"].GetFloat();

        return material;
    }

    void SaveMaterialAsset(const MaterialProperties& mat, const std::string& path, const AssetHandle& guid)
    {
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        doc.AddMember("guid", rapidjson::Value(guid.id.c_str(), allocator), allocator);

        // Ambient
        rapidjson::Value ambientArr(rapidjson::kArrayType);
        ambientArr.PushBack(mat.ambient.x, allocator);
        ambientArr.PushBack(mat.ambient.y, allocator);
        ambientArr.PushBack(mat.ambient.z, allocator);
        doc.AddMember("ambient", ambientArr, allocator);

        // Albedo
        rapidjson::Value albedoArr(rapidjson::kArrayType);
        albedoArr.PushBack(mat.albedo.x, allocator);
        albedoArr.PushBack(mat.albedo.y, allocator);
        albedoArr.PushBack(mat.albedo.z, allocator);
        doc.AddMember("albedo", albedoArr, allocator);

        // Specular
        rapidjson::Value specularArr(rapidjson::kArrayType);
        specularArr.PushBack(mat.specular.x, allocator);
        specularArr.PushBack(mat.specular.y, allocator);
        specularArr.PushBack(mat.specular.z, allocator);
        doc.AddMember("specular", specularArr, allocator);

        // Scalars
        doc.AddMember("shininess", mat.shininess, allocator);
        doc.AddMember("opacity", mat.opacity, allocator);

        // Write to file
        FILE* fp = fopen(path.c_str(), "w");
        if (!fp)
        {
            std::cerr << "Failed to open material file for writing: " << path << "\n";
            return;
        }

        char buffer[65536];
        rapidjson::FileWriteStream os(fp, buffer, sizeof(buffer));
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        doc.Accept(writer);
        fclose(fp);
    }

    void RegisterMaterialMetadata(const AssetHandle& guid, const std::string& name, const std::string& path)
    {
        auto meta = std::make_unique<MaterialMetadata>();
        meta->guid = guid;
        meta->name = name;
        meta->path = path;

        const prop_list* props = TypeRegistry::Instance().GetPropList(TypeName<MaterialMetadata>::value);
        AssetManager::Instance().RegisterAsset(std::move(meta), props, path);
    }
}