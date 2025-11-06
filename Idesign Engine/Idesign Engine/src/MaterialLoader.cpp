
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
}