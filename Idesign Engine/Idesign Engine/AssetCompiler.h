
#pragma once
#ifndef  _ASSET_COMPILER_H_
#define _ASSET_COMPILER_H_

#include <JsonSerialize.h>
#include <PropertySystem.h>

// Note
/*
    Step 1: Read Json and populate object
    Deserialize(&obj, props, doc);
    Step 2: Serialize object to binary
    JsonToBinary(out, obj, props);
    Step 3: Load binary and reconsutruct object
    BinaryToObject(in, obj, props);

    Convert this function to scripting ?
*/

namespace Framework
{
    void Deserialize(void* pObject, const prop_list& PropList, const rapidjson::Value& obj);
    void Serialize(void* pObject, const prop_list& PropList, rapidjson::Value& outObj, rapidjson::Document::AllocatorType& allocator);

    template<typename T>
    bool CompileSingleObject(const std::string& jsonPath, const std::string& outPath, const prop_list& props)
    {
        // Parse JSON
        FILE* fp = fopen(jsonPath.c_str(), "r");
        char buffer[65536];
        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (doc.HasParseError())
        {
            std::cerr << "JSON parse error\n";
            return false;
        }

        std::unique_ptr<T> obj = std::make_unique<T>();
        Deserialize(obj.get(), props, doc);

        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open()) return false;

        JsonToBinary(out, *obj, props);
        out.close();
        return true;
    }

    template<typename T>
    bool CompileArray(const std::string& jsonPath, const std::string& outPath, const prop_list& props, const std::string& arrayKey)
    {
        FILE* fp = fopen(jsonPath.c_str(), "r");
        char buffer[65536];
        rapidjson::FileReadStream is(fp, buffer, sizeof(buffer));
        rapidjson::Document doc;
        doc.ParseStream(is);
        fclose(fp);

        if (doc.HasParseError() || !doc.HasMember(arrayKey.c_str()) || !doc[arrayKey.c_str()].IsArray())
        {
            std::cerr << "Invalid array format\n";
            return false;
        }

        const auto& arr = doc[arrayKey.c_str()].GetArray();
        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open()) return false;

        uint32_t count = static_cast<uint32_t>(arr.Size());
        out.write(reinterpret_cast<char*>(&count), sizeof(count));

        for (const auto& item : arr)
        {
            std::unique_ptr<T> obj = std::make_unique<T>();
            Deserialize(obj.get(), props, item);
            JsonToBinary(out, *obj, props);
        }
        out.close();
        return true;
    }

    template<typename T>
    void JsonToBinary(std::ofstream& out, const T& obj, const Framework::prop_list& props)
    {
        for (const auto& p : props.m_Props)
        {
            switch (p.m_Type)
            {
            case prop::INT:    { int    v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::FLOAT:  { float  v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::DOUBLE: { double v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::LONG:   { long   v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::SHORT:  { short  v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::CHAR:   { char   v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::BOOL:   { bool   v; p.m_fnGet(&obj, &v); out.write(reinterpret_cast<const char*>(&v), sizeof(v)); break; }
            case prop::STRING:
            {
                std::string value;
                p.m_fnGet(&obj, &value);
                uint32_t length = static_cast<uint32_t>(value.size());
                out.write(reinterpret_cast<const char*>(&length), sizeof(length));
                out.write(value.data(), length);
                break;
            }
            default:
                throw std::runtime_error(std::string("Unsupported property type: ") + p.m_pName);
            }
        }
    }

    template<typename T>
    void SaveToBinary(std::ofstream& out, const T& obj, const prop_list& props)
    {
        for (const auto& p : props.m_Props)
        {
            switch (p.m_Type)
            {
            case prop::INT:    { int    v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::FLOAT:  { float  v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::DOUBLE: { double v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::LONG:   { long   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::SHORT:  { short  v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::CHAR:   { char   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::BOOL:   { bool   v; p.m_fnGet(&obj, &v); out.write((char*)&v, sizeof(v)); break; }
            case prop::STRING:
            {
                std::string v;
                p.m_fnGet(&obj, &v);
                uint32_t len = static_cast<uint32_t>(v.size());
                out.write((char*)&len, sizeof(len));
                out.write(v.data(), len);
                break;
            }
            default:
                throw std::runtime_error(std::string("Unsupported property type: ") + p.m_pName);
            }
        }
    }

    template<typename T>
    void BinaryToObject(std::istream& in, T& obj, const prop_list& props)
    {
        constexpr uint32_t MAX_STRING_LENGTH = 1024 * 1024;
        for (const auto& p : props.m_Props)
        {
            if (!p.m_fnSet) throw std::runtime_error("Function pointer m_fnSet is null for property: " + std::string(p.m_pName));
            switch (p.m_Type)
            {
            case prop::INT:    { int    v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::FLOAT:  { float  v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::DOUBLE: { double v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::LONG:   { long   v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::SHORT:  { short  v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::CHAR:   { char   v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::BOOL:   { bool   v; in.read(reinterpret_cast<char*>(&v), sizeof(v)); if (!in) throw; p.m_fnSet(&obj, &v); break; }
            case prop::STRING:
            {
                uint32_t length;
                in.read(reinterpret_cast<char*>(&length), sizeof(length));

                if (!in || length > MAX_STRING_LENGTH) throw std::runtime_error("Invalid string length for property: " + std::string(p.m_pName));

                std::string value(length, '\0');
                in.read(&value[0], length);

                if (!in) throw std::runtime_error("Failed to read string data for property: " + std::string(p.m_pName));
                p.m_fnSet(&obj, &value);
                break;
            }
            default:
                throw std::runtime_error("Unsupported property type: " + std::string(p.m_pName));
            }
        }
    }
}
#endif // ! _ASSET_COMPILER_H_