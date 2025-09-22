
#include <pch.h>
#include <AssetCompiler.h>

namespace Framework
{
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
            case prop::INT:    if (v.IsInt())    { int         temp = v.GetInt();                            p.m_fnSet(pObject, &temp); } break;
            case prop::FLOAT:  if (v.IsNumber()) { float       temp = static_cast<float>(v.GetDouble());     p.m_fnSet(pObject, &temp); } break;
            case prop::DOUBLE: if (v.IsNumber()) { double      temp = v.GetDouble();                         p.m_fnSet(pObject, &temp); } break;
            case prop::LONG:   if (v.IsInt64())  { long        temp = static_cast<long>(v.GetInt64());       p.m_fnSet(pObject, &temp); } break;
            case prop::SHORT:  if (v.IsInt())    { short       temp = static_cast<short>(v.GetInt());        p.m_fnSet(pObject, &temp); } break;
            case prop::BOOL:   if (v.IsBool())   { bool        temp = v.GetBool();                           p.m_fnSet(pObject, &temp); } break;
            case prop::STRING: if (v.IsString()) { std::string temp(v.GetString(), v.GetStringLength());     p.m_fnSet(pObject, &temp); } break;
            case prop::CHAR:   if (v.IsString() && v.GetStringLength() == 1) { char temp = v.GetString()[0]; p.m_fnSet(pObject, &temp); } break;
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
            case prop::INT:    { int    v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::FLOAT:  { float  v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::DOUBLE: { double v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
            case prop::LONG:   { long   v; p.m_fnGet(pObject, &v); outObj.AddMember(key, static_cast<int64_t>(v), allocator); break; }
            case prop::SHORT:  { short  v; p.m_fnGet(pObject, &v); outObj.AddMember(key, static_cast<int>(v), allocator); break; }
            case prop::BOOL:   { bool   v; p.m_fnGet(pObject, &v); outObj.AddMember(key, v, allocator); break; }
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
            default:
                std::cerr << "Unsupported type during serialization: " << p.m_pName << std::endl;
                break;
            }
        }
	}
}