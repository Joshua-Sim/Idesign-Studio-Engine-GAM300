///////////////////////////////////////////////////////////////////////////////
///
///	@file  : PropertyMacro.h
/// @Brief : 
///	
///	@Authors: Edwin Leow
///	@Copyright 2025, Digipen Institute of Technology
///
///////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef _PROPERTY_MACRO_H_
#define _PROPERTY_MACRO_H_

#include "PropertySystem.h"

//------------------------------------------------------------------------------------------------------------------------------------
//  MACRO
//------------------------------------------------------------------------------------------------------------------------------------

#define PROPERTY_END(TypeName, PropArrayName) \
    }; \
    static const Framework::prop_list TypeName##_PropList = { \
        #TypeName, std::span<const Framework::prop>(PropArrayName, sizeof(PropArrayName) / sizeof(Framework::prop)) \
    }; \
    static Framework::global_registration TypeName##_Registration(&TypeName##_PropList); \
    namespace Framework { \
        template<> inline const prop_list& GetPropList<TypeName>() { return TypeName##_PropList; } \
    }

//------------------------------------------------------------------------------------------------------------------------------------
//  INT MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_INT(field, type) \
    Framework::prop{ #field, Framework::prop::INT, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const int*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<int*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_INT_READONLY(field, type) \
    PROP_INT_FLAGS(field, type, Framework::prop::READ_ONLY)

//------------------------------------------------------------------------------------------------------------------------------------
//  FLOAT MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_FLOAT(field, type) \
    Framework::prop{ #field, Framework::prop::FLOAT, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const float*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<float*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_FLOAT_READONLY(field, type) \
    PROP_FLOAT_FLAGS(field, type, Framework::prop::READ_ONLY)

//------------------------------------------------------------------------------------------------------------------------------------
//  DOUBLE MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_DOUBLE(field, type) \
    Framework::prop{ #field, Framework::prop::DOUBLE, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const double*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<double*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_DOUBLE_READONLY(field, type) \
    PROP_DOUBLE_FLAGS(field, type, Framework::prop::READ_ONLY)

//------------------------------------------------------------------------------------------------------------------------------------
//  LONG MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_LONG(field, type) \
    Framework::prop{ #field, Framework::prop::LONG, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const long*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<long*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, Framework::prop::NONE, {} }

//------------------------------------------------------------------------------------------------------------------------------------
//  SHORT MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_SHORT(field, type) \
    Framework::prop{ #field, Framework::prop::SHORT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const short*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<short*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, Framework::prop::NONE, {} }

//------------------------------------------------------------------------------------------------------------------------------------
//  STRING MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_STRING(field, type) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::string*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_STRING_READONLY(field, type) \
    PROP_STRING_FLAGS(field, type, Framework::prop::READ_ONLY)

#define PROP_STRING_DROPDOWN(field, type, optionsFn) \
    Framework::prop{ #field, Framework::prop::STRING_DROPDOWN, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::string*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, Framework::prop::NONE, optionsFn }

#define PROP_STRING_CHAR(field, type) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { \
            std::strncpy(static_cast<type*>(pObj)->field, static_cast<const char*>(pData), sizeof(static_cast<type*>(pObj)->field)); \
            static_cast<type*>(pObj)->field[sizeof(static_cast<type*>(pObj)->field) - 1] = '\0'; \
        }, \
        [](const void* pObj, void* pData) { \
            std::strncpy(static_cast<char*>(pData), static_cast<const type*>(pObj)->field, sizeof(static_cast<const type*>(pObj)->field)); \
            static_cast<char*>(pData)[sizeof(static_cast<const type*>(pObj)->field) - 1] = '\0'; \
        }, \
        {}, {}, false, nullptr, nullptr, 0, Framework::prop::NONE, {} }

//------------------------------------------------------------------------------------------------------------------------------------
//  CHAR MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_CHAR(field, type) \
    Framework::prop{ #field, Framework::prop::CHAR, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const char*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<char*>(pData) = static_cast<const type*>(pObj)->field; }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_CHAR_READONLY(field, type) \
    PROP_CHAR_FLAGS(field, type, Framework::prop::READ_ONLY)

//------------------------------------------------------------------------------------------------------------------------------------
//  BOOL MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_BOOL(field, type) \
    Framework::prop{ #field, Framework::prop::BOOL, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const bool*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<bool*>(pData) = static_cast<const type*>(pObj)->field; } }

//------------------------------------------------------------------------------------------------------------------------------------
//  UNSIGNED MACRO
//------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------
//  UNSIGNED INT MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_UINT(field, type) \
    Framework::prop{ #field, Framework::prop::UINT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const unsigned int*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<unsigned int*>(pData) = static_cast<const type*>(pObj)->field; } }

//------------------------------------------------------------------------------------------------------------------------------------
//  META MACRO
//------------------------------------------------------------------------------------------------------------------------------------

// INT PROPERTY META MACRO
#define PROP_INT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::INT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const int*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<int*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// FLOAT PROPERTY META MACRO
#define PROP_FLOAT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::FLOAT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const float*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<float*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, min, max, true }

// DOUBLE PROPERTY META MACRO
#define PROP_DOUBLE_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::DOUBLE, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const double*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<double*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// LONG PROPERTY META MACRO
#define PROP_LONG_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::LONG, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const long*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<long*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// SHORT PROPERTY META MACRO
#define PROP_SHORT_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::SHORT, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const short*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<short*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// STRING PROPERTY META MACRO
#define PROP_STRING_META(field, type, tooltip, category) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const std::string*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, 0.0f, 0.0f, true }

// CHAR PROPERTY META MACRO
#define PROP_CHAR_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::CHAR, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const char*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<char*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }

// BOOL PROPERTY META MACRO
#define PROP_BOOL_META(field, type, tooltip, category, min, max) \
    Framework::prop{ #field, Framework::prop::BOOL, \
        [](void* pObj, const void* pData) { assert(pObj && pData); static_cast<type*>(pObj)->field = *static_cast<const bool*>(pData); }, \
        [](const void* pObj, void* pData) { assert(pObj && pData); *static_cast<bool*>(pData) = static_cast<const type*>(pObj)->field; }, \
        tooltip, category, static_cast<float>(min), static_cast<float>(max), true }
#endif // !_PROPERTY_MACRO_H_

//------------------------------------------------------------------------------------------------------------------------------------
//  ARRAY MACRO
//------------------------------------------------------------------------------------------------------------------------------------

#define PROP_INT_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::INT_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<int>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<int>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

#define PROP_FLOAT_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::FLOAT_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<float>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<float>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

#define PROP_STRING_ARRAY(field, type) \
    Framework::prop{ \
        #field, Framework::prop::STRING_ARRAY, \
        [](void* pObj, const void* pData) { static_cast<type*>(pObj)->field = *static_cast<const std::vector<std::string>*>(pData); }, \
        [](const void* pObj, void* pData) { *static_cast<std::vector<std::string>*>(pData) = static_cast<const type*>(pObj)->field; } \
    }

//------------------------------------------------------------------------------------------------------------------------------------
//  VEC3 MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_VEC3(field, type) \
    Framework::prop{ #field, Framework::prop::VEC3, \
        [](void* pObj, const void* pData) { \
            assert(pObj && pData); \
            static_cast<type*>(pObj)->field = *static_cast<const glm::vec3*>(pData); \
        }, \
        [](const void* pObj, void* pData) { \
            assert(pObj && pData); \
            *static_cast<glm::vec3*>(pData) = static_cast<const type*>(pObj)->field; \
        } \
    }

//------------------------------------------------------------------------------------------------------------------------------------
//  GUID MACRO
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_GUID_FLAGS(field, type, flags) \
    Framework::prop{ #field, Framework::prop::STRING, \
        [](void* pObj, const void* pData) { \
            static_cast<type*>(pObj)->field = Framework::AssetHandle(*static_cast<const std::string*>(pData)); \
        }, \
        [](const void* pObj, void* pData) { \
            *static_cast<std::string*>(pData) = static_cast<const type*>(pObj)->field.id; \
        }, \
        {}, {}, false, nullptr, nullptr, 0, flags, {} }

#define PROP_GUID_READONLY(field, type) \
    PROP_GUID_FLAGS(field, type, Framework::prop::READ_ONLY)

//------------------------------------------------------------------------------------------------------------------------------------
//  TYPENAME MACRO
//------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
struct TypeName;

#define REGISTER_TYPE_NAME(Type) \
    template<> struct TypeName<Type> { static constexpr const char* value = #Type; }

//------------------------------------------------------------------------------------------------------------------------------------
//  WIDGET || OTHER MACROS
//------------------------------------------------------------------------------------------------------------------------------------
#define PROP_DRAG_FLOAT(field, type) \
    Framework::prop(#field, Framework::prop::DRAG_FLOAT, \
        [](void* obj, const void* in) { static_cast<type*>(obj)->field = *static_cast<const float*>(in); }, \
        [](const void* obj, void* out) { *static_cast<float*>(out) = static_cast<const type*>(obj)->field; })

#define PROP_COLOR3(field, type) \
    Framework::prop(#field, Framework::prop::COLOR3, \
        [](void* obj, const void* in) { static_cast<type*>(obj)->field = *static_cast<const glm::vec3*>(in); }, \
        [](const void* obj, void* out) { *static_cast<glm::vec3*>(out) = static_cast<const type*>(obj)->field; })

#define PROP_SLIDER_FLOAT(field, type, min, max) \
    Framework::prop(#field, Framework::prop::SLIDER_FLOAT, \
        [](void* obj, const void* in) { static_cast<type*>(obj)->field = *static_cast<const float*>(in); }, \
        [](const void* obj, void* out) { *static_cast<float*>(out) = static_cast<const type*>(obj)->field; }, \
        min, max, true)

#endif // !_PROPERTY_MACRO_H_
